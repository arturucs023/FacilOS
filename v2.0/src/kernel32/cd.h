#ifndef F32_CD_H
#define F32_CD_H
#include <stdint.h>

/* v2.0: la copia en RAM empieza en 0x32000 (antes 0x30000: el
   nucleo por encima de 128 KB la pisaba al crecer KERN_SECS).
   El arranque (en modo real) copio de la ISO el catalogo de
   programas y sus datos a 0x32000. El nucleo no llama al BIOS:
   desde 32 bits el int 0x13 peta (#UD) y ademas el CD es de solo
   lectura. cd_leer devuelve un puntero al sector (LBA) dentro de
   esa copia. */
#define CD_RAM_BASE ((uint32_t)0x32000)

/* Puntero al sector LBA de la copia en RAM, o 0 si esta fuera. */
const uint8_t *cd_sector(uint32_t lba);

#endif
