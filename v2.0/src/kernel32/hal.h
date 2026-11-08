#ifndef F32_HAL_H
#define F32_HAL_H
#include <stdint.h>

/* F8: HAL propia de FacilOS32. No es una capa vacia: concentra lo que
 * hoy esta disperso (puertos, EOI PIC/IOAPIC, registro IRQ con
 * nombre, ticks, info de arranque y framebuffer). Los subsistemas
 * usan estas interfaces; los detalles QEMU/SeaBIOS quedan aqui. */

/* ---- arranque (solo lectura, relleno por el cargador BIOS) ----
 * Los campos coinciden con bootinfo_t en 0x7000; cat/dat/fuente se
 * leen por separado (0x7010/0x7018/0x701A) como hoy. */
typedef struct {
    uint8_t drive;      /* DL (0xFF = marca UEFI futura) */
    uint8_t e820n;
    uint8_t vmode;      /* 1 = grafico LFB */
    uint8_t bpp;
    uint16_t fb_w, fb_h, fb_paso;
    uint16_t fb_lo, fb_hi;
} hal_boot_t;

const hal_boot_t *hal_arranque(void);   /* foto de 0x7000 */
uint16_t hal_cat_lba(void);             /* 0x7010 */
uint16_t hal_dat_lba(void);             /* 0x7018 */
uint32_t hal_mem_baja_fin(void);       /* 0x90000: fin copia baja */
uint32_t hal_pila_arranque(void);      /* 0xA0000: tope pila arranque */

/* ---- tiempo ---- */
uint32_t hal_ticks(void);   /* 100 Hz nominal */
uint32_t hal_hz(void);      /* 100 */

/* ---- interrupciones ---- */
typedef void (*hal_irq_fn)(void);
int hal_irq_registra(uint8_t irq, hal_irq_fn h, const char *nombre);
int hal_irq_libera(uint8_t irq);
const char *hal_irq_nombre(uint8_t irq);
void hal_irq_fin(uint8_t irq);   /* EOI PIC o IOAPIC segun ruta */
/* F8: registro central con nombre (la IRQ ya no se cablea a mano). */
hal_irq_fn hal_irq_manej(uint8_t irq);
void hal_tick(void);
/* Llamado por el nucleo al arrancar para fijar la ruta del raton. */
void hal_irq_ruta_raton(int por_apic);

/* ---- framebuffer ---- */
int hal_fb_valido(void);
uint32_t hal_fb_base(void);
uint32_t hal_fb_w(void);
uint32_t hal_fb_h(void);

#endif
