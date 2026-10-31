#ifndef F32_IRQ_H
#define F32_IRQ_H
#include <stdint.h>

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_, ebx, edx, ecx, eax;
    uint32_t num, err;
} regs_t;

void idt_init(void);
void irq_escucha(uint8_t irq, void (*h)(void));
uint32_t irq_ticks(void);

/* v1.6: PIT a 100 Hz (10 ms por tick) para temporizacion estable. */
#define TICKS_SEG 100

#endif
