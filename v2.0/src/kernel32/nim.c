#include "nim.h"
#include "drv_red.h"
#include "pci.h"
#include "red_arp.h"
#include "red_ip.h"
#include "klog.h"

/* F8: tabla minima. activo=puerta SW; mtu filtra Tx; caps fijas
 * (la 8139 es PIO+sondeo: sin IRQ/DMA/MMIO en F1). */

static int presente[NIM_N] = { 0, 0 };
static int activo[NIM_N] = { 0, 0 };
static uint16_t mtu[NIM_N] = { 1500, 1500 };
static uint8_t mac1[6] = { 0, 0, 0, 0, 0, 0 };
static int visto1 = 0;

void nim_init(void) {
    int bus, dev, fun;
    presente[0] = red_hay() ? 1 : 0;
    activo[0] = presente[0] ? 1 : 0;
    mtu[0] = 1500;
    /* Segunda 8139: solo deteccion (sin gestion HW en F8). */
    if (pci_busca_n(0x10EC, 0x8139, 1, &bus, &dev, &fun) == 0) {
        presente[1] = 1;
        activo[1] = 0;   /* detectada, apagada por defecto */
        mtu[1] = 1500;
        visto1 = 1;
        klog(KLOG_INFO, "nim", "2a 8139 detectada (sin gestion F8)");
    } else {
        presente[1] = 0;
        activo[1] = 0;
    }
}

int nim_n(void) {
    int n = 0;
    for (int i = 0; i < NIM_N; i++) if (presente[i]) n++;
    return n;
}

int nim_presente(int idx) {
    if (idx < 0 || idx >= NIM_N) return 0;
    if (idx == 1 && presente[1]) return 1;
    if (idx == 0) return red_hay() ? 1 : 0;
    return 0;
}

int nim_activo(int idx) {
    if (idx < 0 || idx >= NIM_N) return 0;
    if (idx == 0) return (red_hay() && activo[0]) ? 1 : 0;
    return activo[idx] ? 1 : 0;
}

int nim_up(int idx) {
    if (idx < 0 || idx >= NIM_N || !presente[idx]) return -1;
    activo[idx] = 1;
    return 0;
}

int nim_down(int idx) {
    if (idx < 0 || idx >= NIM_N || !presente[idx]) return -1;
    activo[idx] = 0;
    return 0;
}

int nim_mtu(int idx, uint16_t v) {
    if (idx < 0 || idx >= NIM_N || !presente[idx]) return -1;
    if (v < 68 || v > 1500) return -2;
    mtu[idx] = v;
    return 0;
}

uint16_t nim_mtu_de(int idx) {
    if (idx < 0 || idx >= NIM_N) return 0;
    return mtu[idx];
}

int nim_info(int idx, red_if_t *f) {
    int i;
    if (!f) return -1;
    if (idx < 0 || idx >= NIM_N || !presente[idx]) return -1;
    if (idx == 0) {
        if (red_info(f)) return -1;
        f->presente = (uint8_t)(nim_activo(0) ? 1 : 0);
        f->mtu = mtu[0];
        return 0;
    }
    for (i = 0; i < 6; i++) f->mac[i] = mac1[i];
    f->presente = 0;   /* detectada pero no gestionada: se informa */
    f->irq = 0;
    f->io_base = 0;
    f->mtu = mtu[1];
    f->rx_ok = f->tx_ok = f->rx_err = f->tx_err = 0;
    f->rsv[0] = f->rsv[1] = f->rsv[2] = f->rsv[3] = 0;
    (void)visto1;
    return -1;   /* sin gestion HW: no presentarla como usable */
}

int nim_tx(int idx, const uint8_t *marco, int tam) {
    if (idx < 0 || idx >= NIM_N) return -1;
    if (idx != 0) return -1;   /* F8: solo la 0 transmite */
    if (!nim_activo(0)) return -1;
    if (tam > 14 + (int)mtu[0]) return -2;
    return red_tx(marco, tam);
}

int nim_rx(int idx, uint8_t *marco, int max) {
    if (idx < 0 || idx >= NIM_N) return -1;
    if (idx != 0) return -1;
    if (!nim_activo(0)) return -1;
    return red_rx(marco, max);
}
