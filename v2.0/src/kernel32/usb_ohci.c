#include "usb_ohci.h"
#include "pci.h"
#include "cpu.h"
#include "klog.h"
#include "ktimer.h"
#include "irq.h"
#include "pag.h"
#include "teclado.h"
#include "ps2.h"

/* FacilOS32 v2.4-USB: OHCI (USB 1.x) por sondeo. Sin IRQ nueva (el
 * bucle kt a 10 ms revisa los HID; la recepcion de red ya es por
 * sondeo). DMA por identidad: .bss del nucleo <4 GB = fisica directa.
 * Dos puestos HID (teclado + raton); sin hubs ni retiradas. */

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

/* HcControl: bits usados. */
#define HC_PLE   (1u << 2)
#define HC_CLE   (1u << 4)
#define HC_OPERA (2u << 6)
#define HC_SKIP  (1u << 14)   /* ED: saltar (sin dispositivo) */

/* HCCA: 32 cabezas periodicas + marco + cabeza de hechos (256 B). */
typedef struct {
    uint32_t inter[32];
    uint16_t marco;
    uint16_t relleno;
    uint32_t hecho;
} ohci_hcca_t;

/* ED (16 B, alineado a 16). */
typedef struct {
    uint32_t ctrl;      /* FA|EN|DIR|VELOCIDAD|SKIP|MPS */
    uint32_t cola;      /* TailP */
    uint32_t cabeza;    /* HeadP (+Halted) */
    uint32_t sig;       /* NextED */
} ohci_ed_t;

/* TD (16 B, alineado a 16). */
typedef struct {
    uint32_t ctrl;      /* R|DP|DI|T|EC|CC */
    uint32_t buf;       /* CurrentBufferPointer */
    uint32_t sig;       /* NextTD */
    uint32_t fin;       /* BufferEnd */
} ohci_td_t;

static ohci_hcca_t hcca __attribute__((aligned(256)));
static ohci_ed_t ed_ctrl __attribute__((aligned(16)));

static volatile ohci_td_t td_setup __attribute__((aligned(16)));
static volatile ohci_td_t td_dato[8] __attribute__((aligned(16)));
static volatile ohci_td_t td_fin __attribute__((aligned(16)));
static volatile ohci_td_t td_cola __attribute__((aligned(16)));
static uint8_t usb_pet[8] __attribute__((aligned(16)));
static uint8_t usb_datos[256] __attribute__((aligned(16)));

/* Dos puestos HID: 0 = primer dispositivo, 1 = segundo. */
#define USB_PUESTOS 2
static int usb_tipo[USB_PUESTOS];      /* 0 libre, 1 teclado, 2 raton */
static int usb_ok[USB_PUESTOS];
static int usb_fa[USB_PUESTOS];
static int usb_lento[USB_PUESTOS];
static int usb_mps[USB_PUESTOS] = {8, 8};   /* MPS del EP0 */
static int usb_ep[USB_PUESTOS];
static int usb_mps_ep[USB_PUESTOS];
static int usb_tgl[USB_PUESTOS];
static int usb_err[USB_PUESTOS];
static ohci_ed_t ed_int[USB_PUESTOS] __attribute__((aligned(16)));
static volatile ohci_td_t
    td_int[USB_PUESTOS] __attribute__((aligned(16)));
static uint8_t rep_int[USB_PUESTOS][8] __attribute__((aligned(16)));
static uint8_t rep_ant[USB_PUESTOS][8];

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
 * fa/lento/mps del destinatario. Devuelve 0 ok, <0 error. */
static int ohci_control(int fa, int lento, int mps, uint8_t bmReq,
                        uint8_t bReq, uint16_t valor, uint16_t indice,
                        uint16_t largo) {
    int nd, i, dir_in, dp_stat;
    uint32_t vel = lento ? (1u << 13) : 0u;
    uint32_t mp = (uint32_t)mps << 16;
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
        int resto = (int)largo, tgl = 3;
        while (resto > 0 && nd < 8) {
            int trozo = resto > mps ? mps : resto;
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
    /* Estado: direccion contraria, siempre DATA1. El HC para en
       TailP, que apunta al dummy (el ultimo real SI se procesa). */
    dp_stat = dir_in ? 1 : 2;
    td_fin.ctrl = td_ctrl(0, dp_stat, 7, 3);
    td_fin.buf = 0;
    td_fin.fin = 0;
    td_setup.sig = nd ? (uint32_t)&td_dato[0] : (uint32_t)&td_fin;
    for (i = 0; i < nd; i++)
        td_dato[i].sig = (i + 1 < nd) ?
            (uint32_t)&td_dato[i + 1] : (uint32_t)&td_fin;
    td_fin.sig = (uint32_t)&td_cola;
    ed_ctrl.ctrl = (uint32_t)fa | (0u << 7) | (0u << 11) | vel | mp;
    ed_ctrl.cola = (uint32_t)&td_cola;
    ed_ctrl.cabeza = (uint32_t)&td_setup;
    wl(OH_CTRLHEAD, (uint32_t)&ed_ctrl);
    wl(OH_CMD, rl(OH_CMD) | (1u << 1));   /* CLF: hay trabajo */
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

/* Entrega un informe de teclado HID-boot a la cola de teclas. */
static void usb_tecla_informa(int s) {
    int i, mods = (int)rep_int[s][0];
    if (mods != (int)rep_ant[s][0])
        teclado_usb_tecla((uint8_t)mods, 0, 0);
    for (i = 2; i < 8; i++) {
        int k = (int)rep_int[s][i], j, estaba = 0;
        if (!k || k == 1) continue;   /* vacio o error */
        for (j = 2; j < 8; j++)
            if ((int)rep_ant[s][j] == k) { estaba = 1; break; }
        if (!estaba)
            teclado_usb_tecla((uint8_t)mods, (uint8_t)k, 1);
    }
    for (i = 0; i < 8; i++) rep_ant[s][i] = rep_int[s][i];
}

/* Entrega un informe de raton HID-boot a la cola del raton. */
static void usb_raton_informa(int s) {
    int i, b = (int)rep_int[s][0] & 0x07;
    int dx = (int)(int8_t)rep_int[s][1];
    int dy = (int)(int8_t)rep_int[s][2];
    int w = usb_mps_ep[s] > 3 ? (int)(int8_t)rep_int[s][3] : 0;
    int pb = (int)rep_ant[s][0] & 0x07;
    if (!dx && !dy && b == pb && !w) {
        for (i = 0; i < 8; i++) rep_ant[s][i] = rep_int[s][i];
        return;
    }
    ps2_inyecta_usb(dx, dy, b, w);
    for (i = 0; i < 8; i++) rep_ant[s][i] = rep_int[s][i];
}

/* Repone el TD de interrupcion de un puesto y lo engancha al ED. */
static void usb_rearma(int s) {
    int ln = usb_mps_ep[s] > 8 ? 8 : usb_mps_ep[s];
    if (ln < 1) ln = 1;
    td_int[s].ctrl = td_ctrl(1, 2, 7, usb_tgl[s]);
    td_int[s].buf = (uint32_t)rep_int[s];
    td_int[s].fin = (uint32_t)(rep_int[s] + ln - 1);
    td_int[s].sig = 0;
    ed_int[s].cabeza = (uint32_t)&td_int[s];
}

void usb_sondea(void) {
    int s;
    if (!oh_hay) return;
    for (s = 0; s < USB_PUESTOS; s++) {
        if (!usb_ok[s]) continue;
        if ((td_int[s].ctrl >> 28) == 0xFu) continue;   /* en vuelo */
        if ((td_int[s].ctrl >> 28) != 0) {
            /* Error: rearma en DATA0 y sigue (un aviso). */
            if (!usb_err[s]) {
                usb_err[s] = 1;
                klog_hex(KLOG_WARN, "usb", "cc-int",
                         td_int[s].ctrl >> 28);
            }
            usb_tgl[s] = 2;
        } else {
            usb_err[s] = 0;
            if (usb_tipo[s] == 1)
                usb_tecla_informa(s);
            else if (usb_tipo[s] == 2)
                usb_raton_informa(s);
            usb_tgl[s] = (usb_tgl[s] == 2) ? 3 : 2;
        }
        usb_rearma(s);
    }
}

/* v2.4-USB: resetea el puerto y enumera (direccion = puesto+1).
 * Devuelve tipo HID (1 teclado, 2 raton) o <0. */
static int enumera_puerto(int p) {
    uint32_t pst, k;
    int wt, i, s = -1;
    int ep = 0, mps_ep = 8, iface = 0, prot = 0;
    for (i = 0; i < USB_PUESTOS; i++) {
        if (!usb_tipo[i]) { s = i; break; }
    }
    if (s < 0) return -11;   /* sin puestos */
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
    usb_lento[s] = (pst & (1u << 9)) ? 1 : 0;
    usb_fa[s] = 0;   /* direccion 0 hasta SET_ADDRESS */
    /* Descriptor de dispositivo (8 B: importa el MPS). */
    if (ohci_control(usb_fa[s], usb_lento[s], 8,
                     0x80, 6, 0x0100, 0, 8)) {
        klog_hex(KLOG_INFO, "usb", "cc-setup", td_setup.ctrl >> 28);
        klog_hex(KLOG_INFO, "usb", "cc-dato", td_dato[0].ctrl >> 28);
        klog_hex(KLOG_INFO, "usb", "cc-fin", td_fin.ctrl >> 28);
        klog_hex(KLOG_INFO, "usb", "hc-control", rl(OH_CONTROL));
        return -2;
    }
    {
        int mps = (int)usb_datos[7];
        if (mps != 8 && mps != 16 && mps != 32 && mps != 64) mps = 8;
        usb_mps[s] = mps;
    }
    /* SET_ADDRESS (direccion = puesto+1). */
    if (ohci_control(usb_fa[s], usb_lento[s], usb_mps[s],
                     0x00, 5, (uint16_t)(s + 1), 0, 0))
        return -3;
    usb_fa[s] = s + 1;
    espera_ms(10);
    /* Descriptor completo (18). */
    if (ohci_control(usb_fa[s], usb_lento[s], usb_mps[s],
                     0x80, 6, 0x0100, 0, 18))
        return -4;
    klog_hex(KLOG_INFO, "usb", "vid",
             (uint32_t)usb_datos[8] | ((uint32_t)usb_datos[9] << 8));
    klog_hex(KLOG_INFO, "usb", "pid",
             (uint32_t)usb_datos[10] | ((uint32_t)usb_datos[11] << 8));
    /* Configuracion: 9 B para el largo total, luego entera. */
    if (ohci_control(usb_fa[s], usb_lento[s], usb_mps[s],
                     0x80, 6, 0x0200, 0, 9))
        return -5;
    wt = (int)usb_datos[2] | ((int)usb_datos[3] << 8);
    if (wt < 9 || wt > 256) return -6;
    if (ohci_control(usb_fa[s], usb_lento[s], usb_mps[s],
                     0x80, 6, 0x0200, 0, (uint16_t)wt))
        return -7;
    /* Busca interfaz HID-boot (teclado 1 / raton 2) + IN interrupt. */
    i = 0;
    while (i + 2 < wt) {
        int lb = (int)usb_datos[i];
        int tp = (int)usb_datos[i + 1];
        if (lb < 2 || i + lb > wt) break;
        if (tp == 4 && lb >= 9 &&
            usb_datos[i + 5] == 3 && usb_datos[i + 6] == 1 &&
            (usb_datos[i + 7] == 1 || usb_datos[i + 7] == 2)) {
            iface = (int)usb_datos[i + 2];
            prot = (int)usb_datos[i + 7];
        }
        if (tp == 5 && lb >= 7 && (usb_datos[i + 2] & 0x80) &&
            (usb_datos[i + 3] & 0x03) == 0x03 && !ep) {
            int me;
            ep = (int)(usb_datos[i + 2] & 0x0Fu);
            me = (int)usb_datos[i + 4] | ((int)usb_datos[i + 5] << 8);
            if (me < 1 || me > 64) me = 8;
            mps_ep = me;
        }
        i += lb;
    }
    klog_hex(KLOG_INFO, "usb", "iface", (uint32_t)iface);
    klog_hex(KLOG_INFO, "usb", "prot", (uint32_t)prot);
    klog_hex(KLOG_INFO, "usb", "epin", (uint32_t)ep);
    if (!ep || (prot != 1 && prot != 2)) return -8;
    /* SET_CONFIGURATION 1 + protocolo boot (0). */
    if (ohci_control(usb_fa[s], usb_lento[s], usb_mps[s],
                     0x00, 9, 1, 0, 0))
        return -9;
    if (ohci_control(usb_fa[s], usb_lento[s], usb_mps[s],
                     0x21, 0x0B, 0, (uint16_t)iface, 0))
        return -10;
    /* Puesto en marcha: ED periodico + primer TD. */
    usb_ep[s] = ep;
    usb_mps_ep[s] = mps_ep > 8 ? 8 : mps_ep;
    usb_tipo[s] = prot;
    usb_tgl[s] = 2;
    usb_err[s] = 0;
    {
        int j;
        for (j = 0; j < 8; j++) {
            rep_int[s][j] = 0;
            rep_ant[s][j] = 0;
        }
    }
    ed_int[s].ctrl = (uint32_t)usb_fa[s] |
                     ((uint32_t)ep << 7) | (2u << 11) |
                     (usb_lento[s] ? (1u << 13) : 0u) |
                     ((uint32_t)usb_mps_ep[s] << 16);
    ed_int[s].cola = 0;
    ed_int[s].cabeza = 0;
    ed_int[s].sig = 0;
    /* Cadena periodica: tabla -> ed[0] -> ed[1]; sin Saltar. */
    hcca.inter[0] = (uint32_t)&ed_int[0];
    {
        int q;
        for (q = 1; q < 32; q++) hcca.inter[q] = (uint32_t)&ed_int[0];
    }
    ed_int[0].sig = usb_tipo[1] ? (uint32_t)&ed_int[1] : 0;
    ed_int[0].ctrl &= ~HC_SKIP;
    ed_int[1].sig = 0;
    if (usb_tipo[1]) ed_int[1].ctrl &= ~HC_SKIP;
    usb_rearma(s);
    wl(OH_CONTROL, rl(OH_CONTROL) | HC_PLE);   /* periodica en marcha */
    usb_ok[s] = 1;
    klog(KLOG_INFO, "usb",
         prot == 1 ? "teclado HID-boot listo" : "raton HID-boot listo");
    return prot;
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
    /* EDs periodicos: Saltar hasta que haya dispositivo. */
    ed_int[0].ctrl = HC_SKIP;
    ed_int[0].cola = 0;
    ed_int[0].cabeza = 1u;
    ed_int[0].sig = 0;
    ed_int[1].ctrl = HC_SKIP;
    ed_int[1].cola = 0;
    ed_int[1].cabeza = 1u;
    ed_int[1].sig = 0;
    rda = rl(OH_RHDESCA);
    oh_puertos = (int)(rda & 0xFFu);
    if (oh_puertos < 0 || oh_puertos > 15) oh_puertos = 0;
    klog_hex(KLOG_INFO, "usb", "puertos raiz", (uint32_t)oh_puertos);
    wl(OH_RHSTATUS, (1u << 16));   /* power global (si conmuta global) */
    /* Alimenta los puertos; si hay algo conectado, lo enumera.
       Prueba la velocidad detectada y la contraria (LSDA no fiable
       en todos los HC/emuladores). */
    for (p = 0; p < oh_puertos && p < 4; p++) {
        uint32_t pst;
        int rc, lento0;
        wl(OH_RHPORT + (uint32_t)p * 4u, (1u << 8));   /* PPS */
        espera_ms(120);
        pst = rl(OH_RHPORT + (uint32_t)p * 4u);
        klog_hex(KLOG_INFO, "usb", "puerto", pst);
        if (!(pst & 0x01u)) continue;   /* sin CCS */
        lento0 = (pst & (1u << 9)) ? 1 : 0;
        usb_lento[0] = lento0;
        usb_lento[1] = lento0;
        rc = enumera_puerto(p);
        if (rc < 0 && rc != -11 && rc != -1) {
            usb_lento[0] = !lento0;
            usb_lento[1] = !lento0;
            rc = enumera_puerto(p);
        }
        klog_hex(KLOG_INFO, "usb", "enumera", (uint32_t)(rc & 0xFFFF));
    }
    oh_hay = 1;
    /* Sondeo a 10 ms (kt_tick es IRQ0: usb_sondea no bloquea). */
    kt_cada(1, (kt_fn)usb_sondea, 0);
    klog(KLOG_INFO, "usb", "OHCI operativa");
    return 0;
}
