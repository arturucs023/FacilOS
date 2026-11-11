#include "drv_red.h"
#include "pci.h"
#include "cpu.h"
#include "libc.h"
#include "klog.h"

/* Backend virtio-net (transporte PCI legacy) para drv_red.
 * Solo se usa si no hay RTL8139 (sondeo diferido, sin IRQ: igual
 * que el backend RTL, todo por sondeo con esperas acotadas).
 *
 * Legacy PCI (BAR0 PIO): features(0/4), QueuePFN(8), QueueNum(12),
 * QueueSel(14), QueueNotify(16), Status(18), ISR(19), config(20+).
 * Se negocia SOLO MAC (bit 5): tramas planas sin offloads, cabecera
 * de transporte de 10 B que se antepone (TX) y se pela (RX).
 * Colas: 128 entradas (se exige >=128), RX con 64 bufers de 2048 B.
 * Interrupciones suprimidas (avail.flags = NO_NOTIFY); el nucleo
 * sondea el used ring. DMA por identidad (kheap: virt==fis). */

/* --- registros legacy (desplazamientos PIO sobre BAR0) --- */
#define V_IO_FEAT_H  0x00
#define V_IO_FEAT_G  0x04
#define V_IO_QPFN    0x08
#define V_IO_QNUM    0x0C
#define V_IO_QSEL    0x0E
#define V_IO_QNOT    0x10
#define V_IO_STATUS  0x12
#define V_IO_CFG     0x14

/* bits de Status */
#define VS_ACK    0x01
#define VS_DRV    0x02
#define VS_DOK    0x04
#define VS_FOK    0x08
#define VS_FAILED 0x80

#define VF_MAC (1u << 5)

#define VQ_RX 0
#define VQ_TX 1
#define VQ_MAX 256           /* tope de entradas (se usa lo ofrecido) */
#define VQ_RXBUF 64
#define VQ_BUFTAM 2048
#define VQ_HDR 10           /* cabecera de transporte legacy */
#define TX_MAX 1536

#define VRING_AVAIL_F_NO_NOTIFY 1u
#define VRING_DESC_F_NEXT 1u
#define VRING_DESC_F_WRITE 2u

/* Una cola con el layout EXACTO que el dispositivo calcula a partir
   de QueueNum: desc[n] + avail, used en la pagina siguiente alineada
   a 4K. Usar otro tamano desincroniza el used ring (el device escribe
   los completados en otro sitio y el sondeo no avanza nunca). */
typedef struct {
    volatile uint8_t *desc;   /* n * 16 B */
    volatile uint8_t *avail;  /* 4 + 2*n + 2 */
    volatile uint8_t *used;   /* 4 + 8*n + 2, pagina propia */
    uint16_t n;               /* tamano ofrecido (<=VQ_MAX) */
    uint16_t avail_idx;       /* sombra local (el device ve avail->idx) */
    uint16_t used_idx;        /* ultimo consumido */
} vq_t;

static vq_t vq_rx, vq_tx;
static uint8_t *rx_buf[VQ_RXBUF];
static uint8_t tx_marco[TX_MAX + 16];
static uint8_t tx_cab[VQ_HDR + 2];

/* Desplazamientos dentro de los anillos (little-endian x86). */
static void vst16(volatile uint8_t *b, uint32_t o, uint16_t v) {
    b[o] = (uint8_t)(v & 0xFFu);
    b[o + 1] = (uint8_t)((v >> 8) & 0xFFu);
}
static void vst32(volatile uint8_t *b, uint32_t o, uint32_t v) {
    b[o] = (uint8_t)(v & 0xFFu);
    b[o + 1] = (uint8_t)((v >> 8) & 0xFFu);
    b[o + 2] = (uint8_t)((v >> 16) & 0xFFu);
    b[o + 3] = (uint8_t)((v >> 24) & 0xFFu);
}
static uint16_t vld16(volatile uint8_t *b, uint32_t o) {
    return (uint16_t)b[o] | ((uint16_t)b[o + 1] << 8);
}
static uint32_t vld32(volatile uint8_t *b, uint32_t o) {
    return (uint32_t)b[o] | ((uint32_t)b[o + 1] << 8) |
           ((uint32_t)b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24);
}
/* desc[i]: addr(0:8) len(8:4) flags(12:2) next(14:2) */
static void vdesc_pon(vq_t *q, uint16_t i, uint32_t addr, uint32_t len,
                      uint16_t flags, uint16_t next) {
    volatile uint8_t *d = q->desc + (uint32_t)i * 16u;
    vst32(d, 0, addr);
    vst32(d, 4, 0);
    vst32(d, 8, len);
    vst16(d, 12, flags);
    vst16(d, 14, next);
}
/* avail: flags(0:2) idx(2:2) ring[4+2*i] */
static void vavail_pon(vq_t *q, uint16_t id) {
    vst16(q->avail, 4 + (uint32_t)(q->avail_idx % q->n) * 2u, id);
    q->avail_idx++;
    vst16(q->avail, 2, q->avail_idx);
}
/* used: flags(0:2) idx(2:2) elem[4+8*i]: id(0:4) len(4:4) */

static int vio_ok = 0;
static uint16_t vio_io = 0;
static uint8_t vio_hw[6];
static uint8_t vio_irq = 0;
static uint32_t c_rx_ok = 0, c_tx_ok = 0, c_rx_err = 0, c_tx_err = 0;

static uint8_t inb_v(int off) { return inb((uint16_t)(vio_io + off)); }
static void outb_v(int off, uint8_t v) {
    outb((uint16_t)(vio_io + off), v);
}
static uint16_t inw_v(int off) { return inw((uint16_t)(vio_io + off)); }
static void outw_v(int off, uint16_t v) {
    outw((uint16_t)(vio_io + off), v);
}
static uint32_t inl_v(int off) { return inl((uint16_t)(vio_io + off)); }
static void outl_v(int off, uint32_t v) {
    outl((uint16_t)(vio_io + off), v);
}

static void patada(int cola) { outw_v(V_IO_QNOT, (uint16_t)cola); }

static void *alinea_arriba(void *p, uint32_t a) {
    uint32_t v = (uint32_t)p;
    v = (v + a - 1u) & ~(a - 1u);
    return (void *)v;
}

/* Reserva una cola con el tamano ofrecido qn (<=VQ_MAX).
   0 ok, -1 sin memoria. */
static int vq_crea(vq_t *q, int qn) {
    uint8_t *base, *crudo;
    uint32_t off_used, total, i, n;
    volatile uint32_t *w;
    if (qn < 32 || qn > VQ_MAX) return -1;
    q->n = (uint16_t)qn;
    off_used = (16u * (uint32_t)qn + 6u + 2u * (uint32_t)qn + 4095u)
               & ~4095u;
    total = off_used + 6u + 8u * (uint32_t)qn;
    crudo = (uint8_t *)kmalloc(total + 4096);
    if (!crudo) return -1;
    base = (uint8_t *)alinea_arriba(crudo, 4096);
    n = (total + 4096) / 4u;
    w = (volatile uint32_t *)crudo;
    for (i = 0; i < n; i++) w[i] = 0;
    q->desc = (volatile uint8_t *)base;
    q->avail = (volatile uint8_t *)(base + 16u * (uint32_t)qn);
    q->used = (volatile uint8_t *)(base + off_used);
    q->avail_idx = 0;
    q->used_idx = 0;
    vst16(q->avail, 0, VRING_AVAIL_F_NO_NOTIFY);
    return 0;
}

/* Publica un descriptor en avail (sin patada; el llamador patea). */
static void vq_pon(vq_t *q, uint16_t id) {
    vst16(q->avail, 4 + (uint32_t)(q->avail_idx % q->n) * 2u, id);
    q->avail_idx++;
    vst16(q->avail, 2, q->avail_idx);
}
/* Lee used->idx (lo escribe el dispositivo). */
static uint16_t vq_usado_n(vq_t *q) {
    return vld16(q->used, 2);
}

int vio_probe(void) {
    int bus, dev, fun, i, qn0, qn1, nrx;
    uint32_t bar0, cmd, feat;
    if (vio_ok) return 0;
    /* legacy 1AF4:1000 primero (VirtualBox), luego transicional
       1AF4:1041 (QEMU): ambos exponen la interfaz legacy. */
    if (pci_busca(0x1AF4, 0x1000, &bus, &dev, &fun) &&
        pci_busca(0x1AF4, 0x1041, &bus, &dev, &fun)) {
        return -1;
    }
    bar0 = pci_cfg_lee(bus, dev, fun, 0x10);
    if (!(bar0 & 1u) || !(bar0 & 0xFFFFFFFCu)) return -1;
    vio_io = (uint16_t)(bar0 & 0xFFFFFFFCu);
    cmd = pci_cfg_lee(bus, dev, fun, 0x04);
    pci_cfg_pon(bus, dev, fun, 0x04, cmd | 0x05u);
    vio_irq = (uint8_t)(pci_cfg_lee(bus, dev, fun, 0x3C) & 0xFFu);
    /* reset + ACK + DRIVER */
    outb_v(V_IO_STATUS, 0);
    io_espera();
    outb_v(V_IO_STATUS, VS_ACK);
    io_espera();
    outb_v(V_IO_STATUS, VS_ACK | VS_DRV);
    io_espera();
    feat = inl_v(V_IO_FEAT_H);
    if (!(feat & VF_MAC)) return -1;   /* sin MAC no sirve */
    outl_v(V_IO_FEAT_G, VF_MAC);
    outb_v(V_IO_STATUS, VS_ACK | VS_DRV | VS_FOK);
    io_espera();
    if (!(inb_v(V_IO_STATUS) & VS_FOK)) {
        outb_v(V_IO_STATUS, VS_FAILED);
        return -1;
    }
    for (i = 0; i < 6; i++)
        vio_hw[i] = inb_v(V_IO_CFG + i);
    /* colas: se usa el tamano ofrecido (hasta VQ_MAX); el layout
       del used depende de el */
    outw_v(V_IO_QSEL, VQ_RX);
    io_espera();
    qn0 = inw_v(V_IO_QNUM);
    outw_v(V_IO_QSEL, VQ_TX);
    io_espera();
    qn1 = inw_v(V_IO_QNUM);
    if (qn0 < 32 || qn0 > VQ_MAX || qn1 < 32 || qn1 > VQ_MAX) {
        outb_v(V_IO_STATUS, VS_FAILED);
        return -1;
    }
    if (vq_crea(&vq_rx, qn0) || vq_crea(&vq_tx, qn1)) {
        outb_v(V_IO_STATUS, VS_FAILED);
        return -1;
    }
    /* RX: descriptores de solo-escritura con bufer propio */
    nrx = VQ_RXBUF < vq_rx.n - 1 ? VQ_RXBUF : vq_rx.n - 1;
    for (i = 0; i < nrx; i++) {
        uint8_t *b = (uint8_t *)kmalloc(VQ_BUFTAM);
        if (!b) {
            outb_v(V_IO_STATUS, VS_FAILED);
            return -1;
        }
        rx_buf[i] = b;
        vdesc_pon(&vq_rx, (uint16_t)i, (uint32_t)b, VQ_BUFTAM,
                  VRING_DESC_F_WRITE, 0);
        vq_pon(&vq_rx, (uint16_t)i);
    }
    outw_v(V_IO_QSEL, VQ_RX);
    outl_v(V_IO_QPFN, (uint32_t)vq_rx.desc >> 12);
    outw_v(V_IO_QSEL, VQ_TX);
    outl_v(V_IO_QPFN, (uint32_t)vq_tx.desc >> 12);
    outb_v(V_IO_STATUS, VS_ACK | VS_DRV | VS_FOK | VS_DOK);
    io_espera();
    if (inb_v(V_IO_STATUS) & VS_FAILED) return -1;
    patada(VQ_RX);   /* tras DRIVER_OK: anuncia los bufers RX */
    vio_ok = 1;
    kprintf("red: virtio-net io=");
    kprintf("%x", vio_io);
    kprintf(" irq=");
    kprintf("%u", vio_irq);
    kprintf(" mac=");
    for (i = 0; i < 6; i++) {
        kprintf("%x", vio_hw[i]);
        if (i < 5) kprintf(":");
    }
    kprintf("\r\n");
    return 0;
}

int vio_hay(void) { return vio_ok; }

int vio_mac(uint8_t *dst) {
    int i;
    if (!vio_ok || !dst) return -1;
    for (i = 0; i < 6; i++) dst[i] = vio_hw[i];
    return 0;
}

int vio_info(red_if_t *f) {
    int i;
    if (!f || !vio_ok) return -1;
    for (i = 0; i < 6; i++) f->mac[i] = vio_hw[i];
    f->presente = 1;
    f->irq = vio_irq;
    f->io_base = vio_io;
    f->mtu = 1500;
    f->rx_ok = c_rx_ok;
    f->tx_ok = c_tx_ok;
    f->rx_err = c_rx_err;
    f->tx_err = c_tx_err;
    return 0;
}

int vio_tx(const uint8_t *marco, int tam) {
    int orig = tam, i;
    uint16_t head, n;
    unsigned k;
    static uint16_t tx_cabeza = 0;   /* cabezal rotativo (2 desc/cadena) */
    if (!vio_ok) return -1;
    if (!marco || tam < 14 || tam > 1514) return -2;
    if (tam < 60) tam = 60;   /* minimo Ethernet en el cable */
    n = vq_tx.n;
    for (i = 0; i < tam; i++)
        tx_marco[i] = i < orig ? marco[i] : 0;
    for (i = 0; i < VQ_HDR; i++) tx_cab[i] = 0;
    head = tx_cabeza;
    /* cabezales pares < n-1 (n par en la practica; robusto si no) */
    tx_cabeza = (uint16_t)((head + 2u) % (n & (uint16_t)~1u));
    vdesc_pon(&vq_tx, head, (uint32_t)tx_cab, VQ_HDR,
              VRING_DESC_F_NEXT, (uint16_t)(head + 1u));
    vdesc_pon(&vq_tx, (uint16_t)(head + 1u), (uint32_t)tx_marco,
              (uint32_t)tam, 0, 0);
    vq_pon(&vq_tx, head);
    patada(VQ_TX);
    /* Espera SU completado (drena posibles restos de un envio
       anterior fallido; una trama en vuelo como el RTL). */
    for (k = 0; k < 2000000u; k++) {
        while (vld16(vq_tx.used, 2) != vq_tx.used_idx) {
            uint32_t base = 4 + (uint32_t)(vq_tx.used_idx % n) * 8u;
            uint32_t id = vld32(vq_tx.used, base);
            vq_tx.used_idx++;
            if (id == head) {
                c_tx_ok++;
                return orig;
            }
        }
        if ((k & 255u) == 0) pause_cpu();
    }
    c_tx_err++;
    return -3;
}

int vio_rx(uint8_t *marco, int max) {
    int vuelta = 0;
    if (!vio_ok) return -1;
    if (!marco || max <= 0) return -2;
    for (;;) {
        uint32_t id, len, base;
        int marco_tam, i;
        if (vld16(vq_rx.used, 2) == vq_rx.used_idx) {
            if (vuelta) patada(VQ_RX);
            return 0;
        }
        if (++vuelta > (int)vq_rx.n + 1) return 0;  /* nunca bloquea */
        base = 4 + (uint32_t)(vq_rx.used_idx % vq_rx.n) * 8u;
        id = vld32(vq_rx.used, base);
        len = vld32(vq_rx.used, base + 4);
        vq_rx.used_idx++;
        if (id >= VQ_RXBUF || !rx_buf[id] || len < VQ_HDR + 14 ||
            len > VQ_HDR + 1514) {
            c_rx_err++;
            if (id < VQ_RXBUF && rx_buf[id]) vq_pon(&vq_rx, (uint16_t)id);
            continue;
        }
        marco_tam = (int)len - VQ_HDR;
        if (marco_tam > max) {
            c_rx_err++;
            vq_pon(&vq_rx, (uint16_t)id);
            continue;
        }
        {
            uint8_t *src = rx_buf[id] + VQ_HDR;
            for (i = 0; i < marco_tam; i++) marco[i] = src[i];
        }
        vq_pon(&vq_rx, (uint16_t)id);
        c_rx_ok++;
        patada(VQ_RX);
        return marco_tam;
    }
}
