#ifndef F32_TECLADO_H
#define F32_TECLADO_H
#include <stdint.h>

/* Teclas extendidas (flechas). kbd_tecla devuelve ASCII, codepoint
   Unicode o una de estas. */
/* v2.2: Unicode/direcciones: SUPR/INICIO/FIN/REPAG/AVPA + TAB (0x0F). */
#define TEC_ARRIBA 0x101
#define TEC_ABAJO  0x102
#define TEC_IZQ    0x103
#define TEC_DER    0x104
/* v2.0-etapa5: tecla menu (E0 5B/5D). */
#define TEC_MENU   0x105
#define TEC_SUPR   0x106
#define TEC_INICIO 0x107
#define TEC_FIN    0x108
#define TEC_REPAG  0x109
#define TEC_AVPA   0x10A

/* Lee una tecla (bloquea). Requiere IRQ1 con teclado_es() registrado. */
int kbd_tecla(void);
/* Hay tecla pendiente (no bloquea; para bucles con temporizador). */
int kbd_hay(void);
/* Traduce scancode a ES/US. Llamar desde el manejador IRQ1. */
void teclado_irq(void);
/* 1 = espanol (defecto), 0 = americano */
extern int teclado_es;
void teclado_pon(int es);
int teclado_init(void);
/* v2.2: distribucion 0=US, 1=ES, 2=CA (persiste en cabecera). */
int distribucion_actual(void);
void distribucion_pon(int d);

#endif
