#ifndef F32_PS2_H
#define F32_PS2_H
#include <stdint.h>
#include "irq.h"

/* Raton PS/2 (etapa 4, E4): driver i8042/aux, IRQ12, paquetes de 3 B.
 * Modelo como el teclado: cola global de instantaneas + lectura con
 * bloqueo. Un unico consumidor (el servidor) por diseno v1. */

typedef struct {
    int x, y;       /* 0..799, 0..599, ya recortados */
    int botones;    /* bit0 izq, bit1 der, bit2 central */
} raton_t;

int ps2_init(void);          /* entrada de drivers; 0 ok (hay raton) */
int raton_hay(void);         /* instantaneas pendientes (no bloquea) */
int raton_lee(raton_t *dst); /* 0 ok (bloquea si vacia) */

/* Handlers syscalls 38/39 (firma de tabla). */
void sc_raton(regs_t *r, int di);
void sc_raton_hay(regs_t *r, int di);

#endif
