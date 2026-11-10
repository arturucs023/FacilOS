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
/* v2.2-editor: modificadores codificados en la tecla por el driver
   (sin carreras de consulta): bit 0x200 May, 0x400 Ctrl sobre el
   codigo base (p.ej. May+Arriba = 0x301, Ctrl+Arriba = 0x501).
   Ctrl+letra = 0xE100+idx (0xE200+idx con May); Ctrl+Tab = 0xE120
   (0xE220 con May); May+Tab = 0xE121 (F32 Lang v0.1);
   Ctrl+Retroceso = 0x7F. La syscall 49 queda como lectura
   complementaria. */
#define TEC_MAY  0x200
#define TEC_CTRL 0x400
#define TEC_CTRL_A 0xE100  /* +0..25: Ctrl+A..Z (minuscula plegada) */
#define TEC_CTRL_MAYA 0xE200
#define TEC_CTRL_TAB 0xE120
#define TEC_CTRL_MAY_TAB 0xE220
#define TEC_MAY_TAB 0xE121

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
/* v2.2-editor: modificadores (bit0 Shift, bit1 Ctrl, bit2 AltGr, bit3 Caps). */
int teclado_mod(void);
/* v2.4-USB: inyecta tecla HID-boot como si fuera PS/2 (misma cola y
   despertar; mod = byte de modificadores HID, uso = usage ID;
   baja = 1 al pulsar). Reutiliza distribucion/emite(). */
void teclado_usb_tecla(uint8_t mod, uint8_t uso, int baja);

#endif
