#include "apic.h"
#include "cpu.h"

#define IOAPIC_SEL ((volatile uint32_t *)0xFEC00000u)
#define IOAPIC_WIN ((volatile uint32_t *)0xFEC00010u)
#define LAPIC_EOI_R ((volatile uint32_t *)0xFEE000B0u)

#define MSR_APIC_BASE 0x1Bu
#define LAPIC_BASE_STD 0xFEE00000u

/* Mapeadas en pag_inicia (mapea_apic) antes de crear procesos: el EOI se
   ejecuta con cualquier CR3 y debe funcionar siempre. */

int apic_irq12 = 0;

static void ioapic_escribe(uint8_t reg, uint32_t v) {
    IOAPIC_SEL[0] = reg;
    IOAPIC_WIN[0] = v;
}

static uint32_t ioapic_lee(uint8_t reg) {
    IOAPIC_SEL[0] = reg;
    return IOAPIC_WIN[0];
}

int apic_hay(void) {
    uint64_t msr = cpu_rdmsr(MSR_APIC_BASE);
    uint32_t lo = (uint32_t)msr;
    if (!(lo & 0x800u)) return 0;                    /* LAPIC deshabilitado */
    if ((lo & 0xFFFFF000u) != LAPIC_BASE_STD) return 0;  /* base movida */
    if ((ioapic_lee(1) & 0xFFu) == 0) return 0;       /* sin IOAPIC */
    return 1;
}

int apic_ruta_raton(void) {
    if (!apic_hay()) return -1;
    /* Pin 12: vector 44, entrega fija, destino fisico, flanco,
       activo-alto, sin mascara; destino CPU0. */
    ioapic_escribe((uint8_t)(0x10 + 2 * 12), 44u);
    ioapic_escribe((uint8_t)(0x10 + 2 * 12 + 1), 0u);
    return 0;
}

void apic_eoi(void) {
    LAPIC_EOI_R[0] = 0;
}
