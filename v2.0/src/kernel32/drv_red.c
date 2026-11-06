#include "drv_red.h"
#include "pci.h"
#include "cpu.h"
#include "libc.h"
#include "irq.h"

/* v2.3-red F1: RTL8139 (10EC:8139) por PIO.
 * Diseno: un descriptor Tx reutilizado (1 trama en vuelo),
 * anillo Rx de 16 KB, sondeo (sin IRQ), reinicio suave ante
 * errores. Todo error vuelve al llamador; nada cuelga. */

/* --- registros RTL8139 (desplazamientos PIO) --- */
#define R_IDR0   0x00
#define R_TSD0   0x10
#define R_TSAD0  0x20
#define R_RBSTART 0x30
#define R_CR     0x37
#define R_CAPR   0x38
#define R_IMR    0x3C
#define R_ISR    0x3E
#define R_TCR    0x40
#define R_RCR    0x44
#define R_CONFIG1 0x52

/* CR */
#define CR_RST 0x10
#define CR_RE  0x08
#define CR_TE  0x04

/* ISR/IMR */
#define INT_ROK 0x01
#define INT_RER 0x02
#define INT_TOK 0x04
#define INT_TER 0x08

/* TSD */
#define TSD_OWN 0x2000u   /* 1 = trama en manos del chip */

/* RCR: APM+AB (solo la nuestra + difusion), WRAP, MXDMA max,
   anillo 16K. */
#define RCR_VAL 0x0F8Au

#define RX_ANILLO (16 * 1024)
#define RX_MASCARA (RX_ANILLO - 1)
#define TX_MAX 1536

static int presente = 0;
static uint16_t iob = 0;
static uint8_t mi_mac[6];
static uint8_t irq_lin = 0;

static uint8_t *rx_anillo = 0;   /* 16K alineado a 8K */
static int rx_pos = 0;           /* desplazamiento de lectura */

static uint8_t tx_buf[TX_MAX];

static uint32_t c_rx_ok = 0, c_tx_ok = 0, c_rx_err = 0, c_tx_err = 0;

static uint16_t inw_b(int off) { return inw((uint16_t)(iob + off)); }
static void outw_b(int off, uint16_t v) {
    outw((uint16_t)(iob + off), v);
}
static uint32_t inl_b(int off) { return inl((uint16_t)(iob + off)); }
static void outl_b(int off, uint32_t v) {
    outl((uint16_t)(iob + off), v);
}
static void outb_b(int off, uint8_t v) {
    outb((uint16_t)(iob + off), v);
}

/* Espera acotada a que (reg & mascara) == 0. Solo sondea hardware
   (progresa sin interrupciones); acotada por ITERACIONES porque los
   ticks estan congelados dentro de una syscall (IF=0). 0 ok, -1 no. */
static int espera_cero(int off, uint32_t mascara) {
    unsigned i;
    for (i = 0; i < 1000000u; i++) {
        if (!(inl_b(off) & mascara)) return 0;
        if ((i & 255u) == 0) pause_cpu();
    }
    return -1;
}

static void isr_limpia(void) {
    uint16_t s = inw_b(R_ISR);
    if (s) {
        outw_b(R_ISR, s);
        if (s & INT_TOK) c_tx_ok++;
        if (s & INT_ROK) { /* se cuenta al extraer */ }
        if (s & (INT_RER | INT_TER)) { /* se cuenta donde toca */ }
    }
}

/* Reinicio suave del receptor (desbordes): offsets a cero. */
static void rx_repara(void) {
    uint8_t cr;
    c_rx_err++;
    cr = inb((uint16_t)(iob + R_CR));
    outb_b(R_CR, (uint8_t)(cr & (uint8_t)~CR_RE));
    rx_pos = 0;
    outw_b(R_CAPR, 0);
    isr_limpia();
    cr = inb((uint16_t)(iob + R_CR));
    outb_b(R_CR, (uint8_t)(cr | CR_RE));
}

int red_init(void) {
    int bus, dev, fun;
    uint32_t bar0, cmd;
    uint8_t *crudo;
    int i;
    kprintf("red: buscando RTL8139 (10EC:8139)...\r\n");
    if (pci_busca(0x10EC, 0x8139, &bus, &dev, &fun)) {
        kprintf("red: sin adaptador (arranque sin red)\r\n");
        presente = 0;
        return 0;
    }
    bar0 = pci_cfg_lee(bus, dev, fun, 0x10);
    if (!(bar0 & 1u)) {
        kprintf("red: BAR de memoria no soportado en F1\r\n");
        presente = 0;
        return 0;
    }
    iob = (uint16_t)(bar0 & 0xFFFFFFFCu);
    if (!iob) {
        kprintf("red: BAR PIO vacio\r\n");
        presente = 0;
        return 0;
    }
    /* IO + bus mastering, sin tocar lo demas. */
    cmd = pci_cfg_lee(bus, dev, fun, 0x04);
    pci_cfg_pon(bus, dev, fun, 0x04, cmd | 0x05u);
    irq_lin = (uint8_t)(pci_cfg_lee(bus, dev, fun, 0x3C) & 0xFFu);
    for (i = 0; i < 6; i++)
        mi_mac[i] = inb((uint16_t)(iob + R_IDR0 + i));
    /* Encendido y reset con espera acotada. */
    outb_b(R_CONFIG1, 0x00);
    outb_b(R_CR, CR_RST);
    if (espera_cero(R_CR, CR_RST)) {
        kprintf("red: reset sin respuesta\r\n");
        presente = 0;
        return 0;
    }
    /* Anillo Rx 16K alineado a 8K (kheap identidad: virt==fis). */
    crudo = (uint8_t *)kmalloc(RX_ANILLO + 8192);
    if (!crudo) {
        kprintf("red: sin memoria para anillo Rx\r\n");
        presente = 0;
        return 0;
    }
    rx_anillo = (uint8_t *)(((uint32_t)crudo + 8191u) & ~8191u);
    for (i = 0; i < RX_ANILLO; i++) rx_anillo[i] = 0;
    rx_pos = 0;
    outl_b(R_RBSTART, (uint32_t)rx_anillo);
    outl_b(R_RCR, RCR_VAL);
    outw_b(R_IMR, (uint16_t)(INT_ROK | INT_RER | INT_TOK | INT_TER));
    isr_limpia();
    outb_b(R_CR, (uint8_t)(CR_RE | CR_TE));
    presente = 1;
    kprintf("red: rtl8139 io=");
    kprintf("%x", iob);
    kprintf(" irq=");
    kprintf("%u", irq_lin);
    kprintf(" mac=");
    for (i = 0; i < 6; i++) {
        kprintf("%x", mi_mac[i]);
        if (i < 5) kprintf(":");
    }
    kprintf("\r\n");
    return 0;
}

int red_hay(void) { return presente; }

int red_mac(uint8_t *dst) {
    int i;
    if (!presente) return -1;
    if (!dst) return -1;
    for (i = 0; i < 6; i++) dst[i] = mi_mac[i];
    return 0;
}

int red_info(red_if_t *f) {
    int i;
    if (!f) return -1;
    for (i = 0; i < 6; i++) f->mac[i] = mi_mac[i];
    f->presente = presente ? 1 : 0;
    f->irq = irq_lin;
    f->io_base = iob;
    f->mtu = 1500;
    f->rx_ok = c_rx_ok;
    f->tx_ok = c_tx_ok;
    f->rx_err = c_rx_err;
    f->tx_err = c_tx_err;
    for (i = 0; i < 4; i++) f->rsv[i] = 0;
    return presente ? 0 : -1;
}

int red_tx(const uint8_t *marco, int tam) {
    int i, orig = tam;
    if (!presente) return -1;
    if (!marco || tam < 14 || tam > 1514) return -2;
    /* Espera a descriptor libre (1 trama en vuelo en F1). */
    if (espera_cero(R_TSD0, TSD_OWN)) {
        c_tx_err++;
        return -3;
    }
    for (i = 0; i < tam; i++) tx_buf[i] = marco[i];
    /* La 8139 no rellena sola: minimo 60 B en el cable (+ CRC). */
    while (tam < 60) tx_buf[tam++] = 0;
    outl_b(R_TSAD0, (uint32_t)tx_buf);
    outl_b(R_TSD0, (uint32_t)tam);
    /* Espera a fin de transmision. */
    if (espera_cero(R_TSD0, TSD_OWN)) {
        c_tx_err++;
        return -3;
    }
    isr_limpia();
    return orig;
}

/* Lee 'n' bytes del anillo con vuelta. */
static void rx_copia(int off, uint8_t *dst, int n) {
    int i;
    for (i = 0; i < n; i++)
        dst[i] = rx_anillo[(off + i) & RX_MASCARA];
}

int red_rx(uint8_t *marco, int max) {
    uint8_t cab[4];
    int estado, tam, util, cons, i;
    uint16_t s;
    if (!presente) return -1;
    if (!marco || max <= 0) return -2;
    s = inw_b(R_ISR);
    if (s) outw_b(R_ISR, s);
    if (s & INT_RER) {
        rx_repara();
        return 0;
    }
    rx_copia(rx_pos, cab, 4);
    estado = cab[0] | (cab[1] << 8);
    tam = cab[2] | (cab[3] << 8);   /* incluye CRC (4 B) */
    if (!(estado & 0x01)) return 0; /* anillo vacio */
    if (tam < 14 + 4 || tam > 1514 + 4) {
        c_rx_err++;
        cons = (4 + tam + 3) & ~3;
        rx_pos = (rx_pos + cons) & RX_MASCARA;
        outw_b(R_CAPR, (uint16_t)((rx_pos + RX_ANILLO - 16) & RX_MASCARA));
        return 0;
    }
    util = tam - 4;   /* sin CRC final */
    if (util > max) {
        /* No cabe: se descarta (estadistica) y se avanza. */
        c_rx_err++;
        cons = (4 + tam + 3) & ~3;
        rx_pos = (rx_pos + cons) & RX_MASCARA;
        outw_b(R_CAPR, (uint16_t)((rx_pos + RX_ANILLO - 16) & RX_MASCARA));
        return 0;
    }
    for (i = 0; i < util; i++)
        marco[i] = rx_anillo[(rx_pos + 4 + i) & RX_MASCARA];
    cons = (4 + tam + 3) & ~3;
    rx_pos = (rx_pos + cons) & RX_MASCARA;
    outw_b(R_CAPR, (uint16_t)((rx_pos + RX_ANILLO - 16) & RX_MASCARA));
    c_rx_ok++;
    return util;
}
