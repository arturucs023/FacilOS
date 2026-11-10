#include "usb_ohci.h"
#include "pci.h"
#include "cpu.h"
#include "klog.h"
#include "ktimer.h"
#include "irq.h"
#include "pag.h"
#include "teclado.h"

/* FacilOS32 v2.4-USB: OHCI por sondeo. Sin IRQ nueva (el bucle kt a
 * 10 ms revisa el teclado; la recepcion de red ya es por sondeo).
 * DMA por identidad: .bss del nucleo <4 GB = fisica directa. */

/* Registros operativos (offset desde BAR0 MMIO). */
#define OH_REV      0x00
#define OH_CONTROL  0x04
#define OH_CMD      0x08
#define OH_HCCA     0x18
#define OH_CTRLHEAD 0x20
#define OH_FMINTER  0x34
#define OH_PERSTART 0x40
#define OH_RHDESCA  0x48
#define OH_RHSTATUS 0x50
#define OH_RHPORT   0x54

/* HcControl: PLE/CD... bits usados. */
#define HC_PLE   (1u << 2)
#define HC_CLE   (1u << 4)
#define HC_BLE   (1u << 5)
#define HC_OPERA (2u << 6)

/* HCCA: 32 cabezas periodicas + marco + cabeza de hechos (256 B). */
typedef struct {
    uint32_t inter[32];
    uint16_t marco;
    uint16_t relleno;
    uint32_t hecho;
} ohci_hcca_t;

/* ED de control (16 B, alineado a 16). */
typedef struct {
    uint32_t ctrl;      /* FA|EN|DIR|VELOCIDAD|MPS */
    uint32_t cola;      /* TailP */
    uint32_t cabeza;    /* HeadP (+Halted) */
    uint32_t sig;       /* NextED */
} ohci_ed_t;

static ohci_hcca_t hcca __attribute__((aligned(256)));
static ohci_ed_t ed_ctrl __attribute__((aligned(16)));

/* v2.4-USB etapa 2: un TD de setup + hasta 8 de datos + uno de estado. */
typedef struct {
    uint32_t ctrl;      /* R|DP|DI|T|EC|CC */
    uint32_t buf;       /* CurrentBufferPointer */
    uint32_t sig;       /* NextTD */
    uint32_t fin;       /* BufferEnd */
} ohci_td_t;

static volatile ohci_td_t td_setup __attribute__((aligned(16)));
static volatile ohci_td_t td_dato[8] __attribute__((aligned(16)));
static volatile ohci_td_t td_fin __attribute__((aligned(16)));
static volatile ohci_td_t td_cola __attribute__((aligned(16)));
static uint8_t usb_pet[8] __attribute__((aligned(16)));
static uint8_t usb_datos[256] __attribute__((aligned(16)));

static int usb_fa = 0;          /* direccion del teclado */
static int usb_lento = 1;       /* 1 = low speed */
static int usb_mps = 8;
static int usb_epin = 0;
static int usb_iface = 0;
static int usb_tec_ok = 0;
static int usb_mps_ep = 8;

/* v2.4-USB etapa 3: interrupcion IN periodica (1 ms) + sondeo. */
static ohci_ed_t ed_int __attribute__((aligned(16)));
static volatile ohci_td_t td_int __attribute__((aligned(16)));
static uint8_t rep_int[8] __attribute__((aligned(16)));
static uint8_t rep_ant[8];
static int usb_tgl = 2;   /* DATA0 primero (tras configurar) */
static int usb_err_log = 0;

static volatile uint32_t *oh = 0;
static int oh_hay = 0;
static int oh_puertos = 0;

static uint32_t rl(int off) {
    return oh[off / 4];
}

static void wl(int off, uint32_t v) {
    oh[off / 4] = v;
}

/* Espera minima en ms (bucle PIO; sin IRQs aun en el arranque). */
static void espera_ms(uint32_t ms) {
    volatile uint32_t i, n = ms * 1500u;
    for (i = 0; i < n; i++) io_espera();
}

int usb_hay(void) {
    return oh_hay;
}

/* Espera con tope a que un TD deje CC=0xF (lo toca el HC al completar). */
static int td_espera(volatile ohci_td_t *td) {
    uint32_t i;
    for (i = 0; i < 20000000u; i++) {
        if ((td->ctrl >> 28) != 0xFu) return 0;
        if ((i & 0xFFFu) == 0) io_espera();
    }
    return -1;
}

static uint32_t td_ctrl(int r, int dp, int di, int t) {
    return ((uint32_t)r << 18) | ((uint32_t)dp << 19) |
           ((uint32_t)di << 21) | ((uint32_t)t << 24) |
           ((uint32_t)0xFu << 28);
}

/* Transferencia de control (bloquea con tope). dir: 0 OUT, 1 IN.
 * Devuelve 0 ok, <0 error. */
static int ohci_control(uint8_t bmReq, uint8_t bReq,
                        uint16_t valor, uint16_t indice, uint16_t largo) {
    int nd, i, dir_in, dp_stat;
    uint32_t fa = (uint32_t)usb_fa;
    uint32_t vel = usb_lento ? (1u << 13) : 0u;
    uint32_t mps = (uint32_t)usb_mps << 16;
    dir_in = (bmReq & 0x80) ? 1 : 0;
    /* Setup: DATA0, sin datos. */
    usb_pet[0] = bmReq; usb_pet[1] = bReq;
    usb_pet[2] = (uint8_t)(valor & 0xFFu);
    usb_pet[3] = (uint8_t)(valor >> 8);
    usb_pet[4] = (uint8_t)(indice & 0xFFu);
    usb_pet[5] = (uint8_t)(indice >> 8);
    usb_pet[6] = (uint8_t)(largo & 0xFFu);
    usb_pet[7] = (uint8_t)(largo >> 8);
    td_setup.ctrl = td_ctrl(0, 0, 7, 2);
    td_setup.buf = (uint32_t)usb_pet;
    td_setup.fin = (uint32_t)(usb_pet + 7);
    /* Datos (si los hay): DATA1 alternando. */
    nd = 0;
    if (largo) {
        int resto = (int)largo, tgl = 3;   /* datos: DATA1, alterna */
        while (resto > 0 && nd < 8) {
            int trozo = resto > usb_mps ? usb_mps : resto;
            int off = (int)largo - resto;
            td_dato[nd].ctrl = td_ctrl(1, dir_in ? 2 : 1, 7, tgl);
            td_dato[nd].buf = (uint32_t)(usb_datos + off);
            td_dato[nd].fin = (uint32_t)(usb_datos + off + trozo - 1);
            tgl = (tgl == 3) ? 2 : 3;
            resto -= trozo;
            nd++;
        }
        if (resto > 0) return -1;
    }
    /* Estado: direccion contraria a los datos (o a bmReq sin datos),
       siempre DATA1. */
    dp_stat = dir_in ? 1 : 2;
    td_fin.ctrl = td_ctrl(0, dp_stat, 7, 3);
    td_fin.buf = 0;
    td_fin.fin = 0;
    /* Encadena: setup -> datos -> estado -> cola (dummy: el HC
       para al llegar a TailP sin ejecutar lo que apunta). */
    td_setup.sig = nd ? (uint32_t)&td_dato[0] : (uint32_t)&td_fin;
    for (i = 0; i < nd; i++)
        td_dato[i].sig = (i + 1 < nd) ?
            (uint32_t)&td_dato[i + 1] : (uint32_t)&td_fin;
    td_fin.sig = (uint32_t)&td_cola;
    /* ED de control al dia y en marcha (TailP = dummy final).
       CLF: avisa al HC de que la lista tiene trabajo. */
    ed_ctrl.ctrl = fa | (0u << 7) | (0u << 11) | vel | mps;
    ed_ctrl.cola = (uint32_t)&td_cola;
    ed_ctrl.cabeza = (uint32_t)&td_setup;
    wl(OH_CTRLHEAD, (uint32_t)&ed_ctrl);
    wl(0x24, (uint32_t)&ed_ctrl);   /* corriente tambien */
    wl(OH_CMD, rl(OH_CMD) | (1u << 1));   /* CLF */
    if (td_espera(&td_setup)) return -2;
    for (i = 0; i < nd; i++)
        if (td_espera(&td_dato[i])) return -3;
    if (td_espera(&td_fin)) return -4;
    if ((td_setup.ctrl >> 28) != 0) return -5;
    for (i = 0; i < nd; i++)
        if ((td_dato[i].ctrl >> 28) != 0) return -6;
    if ((td_fin.ctrl >> 28) != 0) return -7;
    return 0;
}

void usb_sondea(void) {
    /* v2.4-USB etapa 3: revisa el TD de interrupcion (no bloquea).
       Si completo: procesa el informe y rearma con toggle alterno. */
    int i, mods;
    if (!oh_hay || !usb_tec_ok) return;
    if ((td_int.ctrl >> 28) == 0xFu) return;   /* aun en vuelo */
    if ((td_int.ctrl >> 28) != 0) {
        /* Error: rearma en DATA0 y sigue (sin escupir cada vez). */
        if (!usb_err_log) {
            usb_err_log = 1;
            klog_hex(KLOG_WARN, "usb", "cc-int",
                     td_int.ctrl >> 28);
        }
        usb_tgl = 2;
    } else {
        usb_err_log = 0;
        mods = (int)rep_int[0];
        if (mods != (int)rep_ant[0])
            teclado_usb_tecla((uint8_t)mods, 0, 0);
        for (i = 2; i < 8; i++) {
            int k = (int)rep_int[i], j, estaba = 0;
            if (!k || k == 1) continue;   /* vacio o error */
            for (j = 2; j < 8; j++)
                if ((int)rep_ant[j] == k) { estaba = 1; break; }
            if (!estaba)
                teclado_usb_tecla((uint8_t)mods, (uint8_t)k, 1);
        }
        for (i = 0; i < 8; i++) rep_ant[i] = rep_int[i];
        usb_tgl = (usb_tgl == 2) ? 3 : 2;
    }
    td_int.ctrl = td_ctrl(1, 2, 7, usb_tgl);
    td_int.buf = (uint32_t)rep_int;
    td_int.fin = (uint32_t)(rep_int + usb_mps_ep - 1);
    td_int.sig = 0;
    ed_int.cabeza = (uint32_t)&td_int;
}

/* v2.4-USB etapa 2: resetea el puerto y enumera (direccion 1).
 * Devuelve 0 con teclado HID-boot listo, <0 si no. */
static int enumera_puerto(int p) {
    uint32_t pst;
    uint32_t k;
    int wt, i;
    int ep = 0, mps_ep = 8, iface = 0;
    /* Reset (PRS) y espera a PRSC con tope. */
    wl(OH_RHPORT + (uint32_t)p * 4u, (1u << 4));
    for (k = 0; k < 800000u; k++) {
        pst = rl(OH_RHPORT + (uint32_t)p * 4u);
        if (pst & (1u << 16)) break;
        if ((k & 0xFFFu) == 0) io_espera();
    }
    wl(OH_RHPORT + (uint32_t)p * 4u, (1u << 20));   /* limpia PRSC */
    wl(OH_RHPORT + (uint32_t)p * 4u, (1u << 16));   /* limpia CSC */
    espera_ms(20);   /* TRSTRCY: el dispositivo se estabiliza */
    pst = rl(OH_RHPORT + (uint32_t)p * 4u);
    klog_hex(KLOG_INFO, "usb", "puerto tras reset", pst);
    if (!(pst & 0x02u)) return -1;   /* sin PES */
    usb_lento = (pst & (1u << 9)) ? 1 : 0;
    usb_fa = 0;
    usb_mps = 8;
    /* Descriptor de dispositivo (8 B: importa el MPS). */
    if (ohci_control(0x80, 6, 0x0100, 0, 8)) {
        klog_hex(KLOG_INFO, "usb", "cc-setup",
                 td_setup.ctrl >> 28);
        klog_hex(KLOG_INFO, "usb", "cc-dato",
                 td_dato[0].ctrl >> 28);
        klog_hex(KLOG_INFO, "usb", "cc-fin",
                 td_fin.ctrl >> 28);
        klog_hex(KLOG_INFO, "usb", "hc-control", rl(OH_CONTROL));
        return -2;
    }
    usb_mps = usb_datos[7];
    if (usb_mps != 8 && usb_mps != 16 &&
        usb_mps != 32 && usb_mps != 64)
        usb_mps = 8;
    /* SET_ADDRESS 1. */
    if (ohci_control(0x00, 5, 1, 0, 0)) return -3;
    usb_fa = 1;
    espera_ms(10);
    /* Descriptor completo (18). */
    if (ohci_control(0x80, 6, 0x0100, 0, 18)) return -4;
    klog_hex(KLOG_INFO, "usb", "vid",
             (uint32_t)usb_datos[8] | ((uint32_t)usb_datos[9] << 8));
    klog_hex(KLOG_INFO, "usb", "pid",
             (uint32_t)usb_datos[10] | ((uint32_t)usb_datos[11] << 8));
    /* Configuracion: 9 B para el largo total, luego entera. */
    if (ohci_control(0x80, 6, 0x0200, 0, 9)) return -5;
    wt = (int)usb_datos[2] | ((int)usb_datos[3] << 8);
    if (wt < 9 || wt > 256) return -6;
    if (ohci_control(0x80, 6, 0x0200, 0, (uint16_t)wt)) return -7;
    /* Busca interfaz HID-boot-teclado + endpoint IN. */
    i = 0;
    while (i + 2 < wt) {
        int lb = (int)usb_datos[i];
        int tp = (int)usb_datos[i + 1];
        if (lb < 2 || i + lb > wt) break;
        if (tp == 4 && lb >= 9 &&
            usb_datos[i + 5] == 3 && usb_datos[i + 6] == 1 &&
            usb_datos[i + 7] == 1)
            iface = (int)usb_datos[i + 2];
        if (tp == 5 && lb >= 7 && (usb_datos[i + 2] & 0x80) &&
            (usb_datos[i + 3] & 0x03) == 0x03 && !ep) {
            ep = (int)(usb_datos[i + 2] & 0x0Fu);
            mps_ep = (int)usb_datos[i + 4] |
                     ((int)usb_datos[i + 5] << 8);
            if (mps_ep < 1 || mps_ep > 64) mps_ep = 8;
        }
        i += lb;
    }
    klog_hex(KLOG_INFO, "usb", "iface", (uint32_t)iface);
    klog_hex(KLOG_INFO, "usb", "epin", (uint32_t)ep);
    if (!ep) return -8;
    /* SET_CONFIGURATION 1 + protocolo boot (0). */
    if (ohci_control(0x00, 9, 1, 0, 0)) return -9;
    if (ohci_control(0x21, 0x0B, 0, (uint16_t)iface, 0)) return -10;
    usb_epin = ep;
    usb_iface = iface;
    if (mps_ep > 8) mps_ep = 8;
    usb_mps_ep = mps_ep;
    usb_tec_ok = 1;
    /* Interrupcion IN: ED periodico con la tabla HCCA apuntandolo
       (intervalo 1 ms) + primer TD. El sondeo kt lo rearma. */
    for (i = 0; i < 8; i++) { rep_int[i] = 0; rep_ant[i] = 0; }
    ed_int.ctrl = (uint32_t)usb_fa | ((uint32_t)ep << 7) |
                  (2u << 11) | (usb_lento ? (1u << 13) : 0u) |
                  ((uint32_t)mps_ep << 16);
    ed_int.cola = 0;
    ed_int.cabeza = 0;
    ed_int.sig = 0;
    for (i = 0; i < 32; i++) hcca.inter[i] = (uint32_t)&ed_int;
    td_int.ctrl = td_ctrl(1, 2, 7, usb_tgl);
    td_int.buf = (uint32_t)rep_int;
    td_int.fin = (uint32_t)(rep_int + usb_mps_ep - 1);
    td_int.sig = 0;
    ed_int.cabeza = (uint32_t)&td_int;
    wl(OH_CONTROL, rl(OH_CONTROL) | HC_PLE);   /* periodica en marcha */
    klog(KLOG_INFO, "usb", "teclado HID-boot listo");
    return 0;
}

int usb_init(void) {
    int bus, dev, fun, p;
    uint32_t cmd, bar, base, rev, rda;
    if (oh) return 0;   /* idempotente */
    if (pci_busca_clase(0x0C, 0x03, 0x10, 0, &bus, &dev, &fun)) {
        klog(KLOG_INFO, "usb", "sin OHCI (PS/2 sigue)");
        return 0;
    }
    cmd = pci_cfg_lee(bus, dev, fun, 0x04);
    pci_cfg_pon(bus, dev, fun, 0x04, cmd | 0x06u);   /* MEM + mastering */
    bar = pci_cfg_lee(bus, dev, fun, 0x10);
    if (bar & 1u) {
        klog(KLOG_WARN, "usb", "BAR PIO (OHCI exige MMIO)");
        return 0;
    }
    base = bar & 0xFFFFFFF0u;
    if (!base) {
        klog(KLOG_WARN, "usb", "BAR vacia");
        return 0;
    }
    if (pag_mapa(0, base, base,
                 PAG_PRESENTE | PAG_ESCRITURA | PAG_NOCACHE)) {
        klog(KLOG_WARN, "usb", "sin mapa MMIO");
        return 0;
    }
    oh = (volatile uint32_t *)base;
    rev = rl(OH_REV);
    klog_hex(KLOG_INFO, "usb", "OHCI rev", rev & 0xFFu);
    klog_hex(KLOG_INFO, "usb", "MMIO", base);
    /* Reset del controlador (HCR) con tope. */
    wl(OH_CMD, 1u);
    {
        int i;
        for (i = 0; i < 100000; i++) {
            if (!(rl(OH_CMD) & 1u)) break;
            io_espera();
        }
        if (rl(OH_CMD) & 1u) {
            klog(KLOG_WARN, "usb", "sin reset");
            oh = 0;
            return 0;
        }
    }
    /* Sin IRQs propias (sondeo): todo deshabilitado. */
    wl(0x14, 0xFFFFFFFFu);
    /* HCCA + lista de control (parada) + periodica valida. */
    for (p = 0; p < 32; p++) hcca.inter[p] = 0;
    hcca.marco = 0;
    hcca.hecho = 0;
    wl(OH_HCCA, (uint32_t)&hcca);
    ed_ctrl.ctrl = (64u << 16);   /* MPS 64, direccion TD, full speed */
    ed_ctrl.cola = 0;
    ed_ctrl.cabeza = 1u;          /* parada (Halted) */
    ed_ctrl.sig = 0;
    wl(OH_CTRLHEAD, (uint32_t)&ed_ctrl);
    wl(OH_FMINTER, 0x2EDFu);      /* 11999: valor USB */
    wl(OH_PERSTART, 0x2A2Fu);
    wl(OH_CONTROL, HC_OPERA | HC_CLE);   /* operativa + lista control */
    rda = rl(OH_RHDESCA);
    oh_puertos = (int)(rda & 0xFFu);
    if (oh_puertos < 0 || oh_puertos > 15) oh_puertos = 0;
    klog_hex(KLOG_INFO, "usb", "puertos raiz", (uint32_t)oh_puertos);
    /* Alimenta los puertos; si hay algo conectado, lo enumera.
       Prueba la velocidad detectada y la contraria (LSDA no fiable
       en todos los HC/emuladores). */
    wl(OH_RHSTATUS, (1u << 16));   /* power global (si conmuta global) */
    for (p = 0; p < oh_puertos && p < 4; p++) {
        uint32_t pst;
        int rc;
        wl(OH_RHPORT + (uint32_t)p * 4u, (1u << 8));   /* PPS */
        espera_ms(120);
        pst = rl(OH_RHPORT + (uint32_t)p * 4u);
        klog_hex(KLOG_INFO, "usb", "puerto", pst);
        if (!(pst & 0x01u)) continue;   /* sin CCS */
        rc = enumera_puerto(p);
        if (rc == -2) {
            usb_lento = !usb_lento;   /* reintenta otra velocidad */
            rc = enumera_puerto(p);
        }
        klog_hex(KLOG_INFO, "usb", "enumera", (uint32_t)(rc & 0xFFFF));
    }
    oh_hay = 1;
    /* Sondeo a 10 ms (kt_tick es IRQ0: usb_sondea no bloquea). */
    kt_cada(1, (kt_fn)usb_sondea, 0);
    klog(KLOG_INFO, "usb", "OHCI operativa (etapa 1: sin teclado aun)");
    return 0;
}
