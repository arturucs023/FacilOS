#include "cd.h"
#include "bootinfo.h"

/* La copia en RAM empieza en el LBA que el arranque dejo en 0x7010
   (cat_lba). Todo lo demas son LBA correlativos de la misma imagen. */
static uint32_t base_lba(void) {
    return *BI_CAT ? (uint32_t)*BI_CAT : 1;
}

const uint8_t *cd_sector(uint32_t lba) {
    if (lba < base_lba()) return 0;
    uint32_t off = (lba - base_lba()) * 512u;
    if (off > 0x300000u) return 0;          /* tope de seguridad */
    return (const uint8_t *)(CD_RAM_BASE + off);
}
