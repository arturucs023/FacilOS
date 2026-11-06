#ifndef F32_DRV_RED_H
#define F32_DRV_RED_H
#include <stdint.h>

/* v2.3-red F1: adaptador Ethernet (RTL8139) + E/S cruda de tramas.
 * DMA por identidad: todo bufer del nucleo <4 GB vale (kheap
 * 0x800000 con identidad; virt==fis). Sin interrupciones en F1:
 * transmision con espera acotada y recepcion por sondeo. */

/* Ficha para Ring3 (red_interfaz). Copia identica en user/lib/ulib.h. */
typedef struct {
    uint8_t mac[6];
    uint8_t presente;   /* 1 si hay adaptador y esta activo */
    uint8_t irq;        /* linea PCI (informativa; F1 no usa IRQ) */
    uint16_t io_base;   /* base PIO del RTL8139 */
    uint16_t mtu;       /* 1500 */
    uint32_t rx_ok;
    uint32_t tx_ok;
    uint32_t rx_err;
    uint32_t tx_err;
    uint8_t rsv[4];   /* rsv[0]: entradas ARP en cache (F2) */
} red_if_t; /* 32 B */

/* Inicializa PCI+RTL8139. Siempre 0 (arranque sin red si no hay
   adaptador: lo indica por serie; no bloquea). */
int red_init(void);

/* 1 si hay adaptador activo. */
int red_hay(void);

/* Copia la MAC a dst[6]. 0 ok, -1 sin adaptador. */
int red_mac(uint8_t *dst);

/* Rellena la ficha. 0 ok, -1 sin adaptador. */
int red_info(red_if_t *f);

/* Transmite una trama Ethernet completa (14 + carga, sin CRC: lo
   pone el chip). Tam 14..1514. Devuelve tam o -1 sin adaptador,
   -2 tam invalido, -3 espera agotada. */
int red_tx(const uint8_t *marco, int tam);

/* Sondea una trama recibida (sin CRC final). Si hay: la copia
   (max = capacidad) y devuelve su tam; si no: 0. -1 sin adaptador,
   -2 parametros invalidos. Nunca bloquea. */
int red_rx(uint8_t *marco, int max);

#endif
