#ifndef F32_NIM_H
#define F32_NIM_H
#include <stdint.h>
#include "drv_red.h"   /* red_if_t (32 B, ABI Ring3 intacta) */

/* F8: Network Interface Manager propio (minimo y real).
 * No es una segunda pila: gestiona interfaces sobre la pila actual.
 * F8: 2 ranuras; la 0 es la RTL8139 de siempre; la 1 solo se
 * detecta (no se gestiona HW aun). Sin Wi-Fi. */

#define NIM_N 2

void nim_init(void);   /* tras el probe: fija presente/activo/MAC */
int nim_n(void);                       /* interfaces presentes */
int nim_presente(int idx);             /* 1 si hay HW */
int nim_activo(int idx);               /* 1 si habilitada por el SO */
/* up/down: puerta por software (no toca energia del chip en F8).
 * down bloquea Tx/Rx de la pila; up la reabre. 0 ok, -1 sin if. */
int nim_up(int idx);
int nim_down(int idx);
/* MTU 68..1500 (solo guarda y filtra; la RTL soporta). */
int nim_mtu(int idx, uint16_t v);
uint16_t nim_mtu_de(int idx);
/* Ficha Ring3 (igual formato que red_info) con estado NIM. */
int nim_info(int idx, red_if_t *f);
/* Envio/recepcion por interfaz (gating + HW de la 0). Mismos
 * codigos que red_tx/red_rx. */
int nim_tx(int idx, const uint8_t *marco, int tam);
int nim_rx(int idx, uint8_t *marco, int max);

#endif
