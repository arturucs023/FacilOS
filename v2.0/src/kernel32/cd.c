#include "cd.h"
#include "bootinfo.h"

/* La copia en RAM empieza en el LBA que el arranque dejo en 0x7010
   (cat_lba). Todo lo demas son LBA correlativos de la misma imagen.
   F7-B: el hueco VGA (0xA0000) no retiene: la copia baja cubre
   [base, CORTE); la cola [CORTE, ...) vive en CD_ALTO_BASE
   (copiada por el arranque en irreal; ver boot32.asm). */
#define CD_CORTE_LBA 1121u   /* = 225 + (0xA0000 - 0x30000)/512 */
#define CD_ALTO_BASE 0x1000000u
#define CD_ALTO_MAX 0x40000u /* cola maxima: 256 KB */

static uint32_t base_lba(void) {
    return *BI_CAT ? (uint32_t)*BI_CAT : 1;
}

const uint8_t *cd_sector(uint32_t lba) {
    if (lba < base_lba()) return 0;
    if (lba < CD_CORTE_LBA) {
        uint32_t off = (lba - base_lba()) * 512u;
        if (off > 0x300000u) return 0;      /* tope de seguridad */
        return (const uint8_t *)(CD_RAM_BASE + off);
    }
    {
        uint32_t off2 = (lba - CD_CORTE_LBA) * 512u;
        if (off2 >= CD_ALTO_MAX) return 0;
        return (const uint8_t *)(CD_ALTO_BASE + off2);
    }
}
