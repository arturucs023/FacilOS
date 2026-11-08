#include "hal.h"
#include "cpu.h"
#include "apic.h"
#include "bootinfo.h"
#include "klog.h"

/* F8: implementacion HAL. Tablas propias con nombre; EOI segun ruta;
 * arranque como vista de solo lectura del bloque BIOS. */

static hal_irq_fn hal_tab[16] = { 0 };
static const char *hal_nom[16] = { 0 };
static int hal_raton_apic = 0;
static volatile uint32_t hal_ticks_v = 0;

const hal_boot_t *hal_arranque(void) {
    /* El bloque 0x7000 ya es la ABI con el cargador; se expone tal cual. */
    return (const hal_boot_t *)0x7000;
}

uint16_t hal_cat_lba(void) { return *(volatile uint16_t *)0x7010; }
uint16_t hal_dat_lba(void) { return *(volatile uint16_t *)0x7018; }

uint32_t hal_mem_baja_fin(void) { return 0x90000u; }
uint32_t hal_pila_arranque(void) { return 0xA0000u; }

uint32_t hal_ticks(void) { return hal_ticks_v; }
uint32_t hal_hz(void) { return 100; }

void hal_tick(void) { hal_ticks_v++; }

int hal_irq_registra(uint8_t irq, hal_irq_fn h, const char *nombre) {
    uint32_t fl;
    if (irq >= 16 || !h) return -1;
    fl = cpu_eflags();
    cli();
    if (hal_tab[irq]) {
        cpu_if_restaura(fl);
        klog(KLOG_WARN, "hal", "irq ya registrada");
        return -2;   /* conflicto real, no se pisa */
    }
    hal_tab[irq] = h;
    hal_nom[irq] = nombre ? nombre : "?";
    cpu_if_restaura(fl);
    return 0;
}

int hal_irq_libera(uint8_t irq) {
    uint32_t fl;
    if (irq >= 16) return -1;
    fl = cpu_eflags();
    cli();
    hal_tab[irq] = 0;
    hal_nom[irq] = 0;
    cpu_if_restaura(fl);
    return 0;
}

const char *hal_irq_nombre(uint8_t irq) {
    if (irq >= 16 || !hal_tab[irq]) return 0;
    return hal_nom[irq];
}

hal_irq_fn hal_irq_manej(uint8_t irq) {
    if (irq >= 16) return 0;
    return hal_tab[irq];
}

void hal_irq_ruta_raton(int por_apic) { hal_raton_apic = por_apic ? 1 : 0; }

void hal_irq_fin(uint8_t irq) {
    if (irq == 12 && hal_raton_apic) {
        apic_eoi();
        return;
    }
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

int hal_fb_valido(void) {
    const hal_boot_t *b = hal_arranque();
    return (b->vmode == 1 && b->bpp == 32 && b->fb_w >= 640 &&
            b->fb_h >= 400 && (b->fb_lo | (b->fb_hi << 16))) ? 1 : 0;
}

uint32_t hal_fb_base(void) {
    const hal_boot_t *b = hal_arranque();
    return b->fb_lo | (b->fb_hi << 16);
}

uint32_t hal_fb_w(void) { return hal_arranque()->fb_w; }
uint32_t hal_fb_h(void) { return hal_arranque()->fb_h; }
