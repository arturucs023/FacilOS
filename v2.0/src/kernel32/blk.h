#ifndef F32_BLK_H
#define F32_BLK_H
#include <stdint.h>

/* Capa de bloques v2.0-etapa3 (E2): abstracción propia sobre ATA PIO.
 * No cambia ata.c (su PIO sigue igual); añade detección, capacidad,
 * validación de rangos y flush. Errores: 0 ok, -1 error/ausente/rango.
 * Sin disco, init devuelve -1 y el sistema sigue en RAM (diseño). */

int blk_init(void);                 /* detecta + IDENTIFY; imprime resumen */
int blk_presente(void);
uint32_t blk_sectores(void);        /* 0 si ausente */
int blk_leer(uint32_t lba, void *buf, uint32_t nsec);
int blk_escribir(uint32_t lba, const void *buf, uint32_t nsec);
int blk_flush(void);

#endif
