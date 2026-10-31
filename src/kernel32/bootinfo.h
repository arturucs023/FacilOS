#ifndef F32_BOOTINFO_H
#define F32_BOOTINFO_H
#include <stdint.h>

/* Rellenado por boot32 en 0x7000. vmode: 0=texto 80x25, 1=grafico LFB.
   0x7010 = LBA del catalogo de programas en la ISO, 0x7018 = LBA de los
   datos.
   v1.9: e820n (0x7001) = entradas validas del mapa E820 (24 B cada una en
   0x5000). 0x701A/0x701C = segmento:offset de la fuente 8x16 del BIOS
   (el arranque ya no la copia por falta de sitio en el sector: la copia
   el nucleo a 0x6000). */
typedef struct {
    uint8_t drive;
    uint8_t e820n;
    uint8_t vmode;
    uint8_t bpp;
    uint16_t w, h, pitch, lfb_lo, lfb_hi;
} bootinfo_t;

static bootinfo_t *const BI = (bootinfo_t *)0x7000;
static uint8_t *const FONT_BIN = (uint8_t *)0x6000;
static uint16_t *const BI_CAT = (uint16_t *)0x7010;
static uint16_t *const BI_DAT = (uint16_t *)0x7018;
static uint16_t *const BI_FONT_SEG = (uint16_t *)0x701A;
static uint16_t *const BI_FONT_OFF = (uint16_t *)0x701C;

#endif
