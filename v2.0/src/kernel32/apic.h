#ifndef F32_APIC_H
#define F32_APIC_H
#include <stdint.h>

/* APIC (v2.0-etapa4): ruta de IRQ del raton por IOAPIC.
 *
 * Por que existe: en el QEMU usado, la cascada del i8259 no entrega el
 * esclavo a la CPU (IRR esclavo con IRQ12 pendiente, mascara abierta,
 * puerta IDT verificada: la CPU nunca vectoriza; ver INFORME-RATON).
 * El IOAPIC (siempre presente en la maquina pc) recibe la misma linea
 * ISA y puede entregar el vector directamente via LAPIC. El resto de
 * IRQ (0/1) siguen por el i8259 sin cambios.
 */

/* 1 si la IRQ12 del raton va por IOAPIC (lo pone ps2_init). */
extern int apic_irq12;

/* 1 si hay LAPIC habilitado en su base estandar + IOAPIC vivo. */
int apic_hay(void);

/* Programa GSI12 -> vector 44 (fijo, flanco, activo-alto, CPU0).
   0 ok, -1 sin APIC (no toca nada). */
int apic_ruta_raton(void);

/* Fin de interrupcion al LAPIC local. */
void apic_eoi(void);

#endif
