#include "irq.h"
#include "libc.h"
#include "cpu.h"

typedef struct {
    uint16_t bajo;
    uint16_t sel;
    uint8_t cero;
    uint8_t flags;
    uint16_t alto;
} __attribute__((packed)) idt_ent_t;

uint32_t idt_tabla[256 * 2];
extern uint32_t isr_tabla[32];
extern uint32_t irq_tabla[16];

static void pon_puerta(int n, uint32_t dir) {
    idt_ent_t *e = (idt_ent_t *)&idt_tabla[n * 2];
    e->bajo = (uint16_t)(dir & 0xFFFF);
    e->sel = 0x08;
    e->cero = 0;
    e->flags = 0x8E;
    e->alto = (uint16_t)((dir >> 16) & 0xFFFF);
}

/* Puerta 0x80: tipo INTERRUPCION con DPL3 (0xEE limpia IF al entrar,
   bien para syscalls no reentrantes; el que espera hace sti, como
   kbd_tecla). No es trampa aunque el nombre historico lo diga. */
static void pon_trampa(int n, uint32_t dir) {
    idt_ent_t *e = (idt_ent_t *)&idt_tabla[n * 2];
    e->bajo = (uint16_t)(dir & 0xFFFF);
    e->sel = 0x08;
    e->cero = 0;
    e->flags = 0xEE;
    e->alto = (uint16_t)((dir >> 16) & 0xFFFF);
}

extern void idt_cargar(void);

void idt_init(void) {
    for (int i = 0; i < 32; i++) pon_puerta(i, isr_tabla[i]);
    for (int i = 0; i < 16; i++) pon_puerta(32 + i, irq_tabla[i]);
    {
        extern void isr128(void);
        pon_trampa(0x80, (uint32_t)isr128);
    }
    /* PIC: maestro 0x20, esclavo 0x28; solo timer+teclado */
    outb(0x20, 0x11); io_espera();
    outb(0xA0, 0x11); io_espera();
    outb(0x21, 0x20); io_espera();
    outb(0xA1, 0x28); io_espera();
    outb(0x21, 0x04); io_espera();
    outb(0xA1, 0x02); io_espera();
    outb(0x21, 0x01); io_espera();
    outb(0xA1, 0x01); io_espera();
    outb(0x21, 0xFC); io_espera();
    outb(0xA1, 0xFF); io_espera();
    /* PIT canal 0 a 100 Hz: 1193182/100 = 11931 */
    outb(0x43, 0x34); io_espera();
    outb(0x40, 11931 & 0xFF); io_espera();
    outb(0x40, (11931 >> 8) & 0xFF); io_espera();
    idt_cargar();
}
