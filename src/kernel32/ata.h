#ifndef F32_ATA_H
#define F32_ATA_H
#include <stdint.h>

/* ATA PIO LBA28, primario maestro. 0 ok, -1 error/timeout. */
int ata_leer(uint32_t lba, void *buf, uint32_t nsec);
int ata_escribir(uint32_t lba, const void *buf, uint32_t nsec);

#endif
