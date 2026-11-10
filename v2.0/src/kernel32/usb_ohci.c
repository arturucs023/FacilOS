#include "usb_ohci.h"
#include "pci.h"
#include "cpu.h"
#include "klog.h"
#include "ktimer.h"
#include "irq.h"
#include "pag.h"

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

void usb_sondea(void) {
    /* v2.4-USB etapa 1: solo presencia (el teclado llega al final). */
    if (!oh_hay) return;
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
    /* Alimenta los puertos y anota quien tiene algo conectado. */
    for (p = 0; p < oh_puertos && p < 4; p++) {
        uint32_t pst;
        wl(OH_RHPORT + (uint32_t)p * 4u, (1u << 8));   /* PPS */
        espera_ms(120);
        pst = rl(OH_RHPORT + (uint32_t)p * 4u);
        klog_hex(KLOG_INFO, "usb", "puerto", pst);
    }
    oh_hay = 1;
    /* Sondeo a 10 ms (kt_tick es IRQ0: usb_sondea no bloquea). */
    kt_cada(1, (kt_fn)usb_sondea, 0);
    klog(KLOG_INFO, "usb", "OHCI operativa (etapa 1: sin teclado aun)");
    return 0;
}
