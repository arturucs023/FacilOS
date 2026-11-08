#include "cd.h"
#include "bootinfo.h"

/* La copia en RAM empieza en el LBA que el arranque dejo en 0x7010
   (cat_lba). Todo lo demas son LBA correlativos de la misma imagen.
   F7-C: la pila de arranque (64 KB en [0x90000,0xA0000)) mas EBDA/VGA
   no retienen: la copia baja cubre [base, CORTE); la cola [CORTE, ...)
   vive en CD_ALTO_BASE (copiada por el arranque en irreal). */
#define CD_CORTE_LBA 993u   /* = 225 + (0x90000 - 0x30000)/512 */
#define CD_ALTO_BASE 0x1000000u
#define CD_ALTO_MAX 0x800000u /* F8: cola maxima 8 MB (ISO sin tope
                               1,44 MB; el tamano lo dicta el contenido) */

static uint32_t base_lba(void) {
    return *BI_CAT ? (uint32_t)*BI_CAT : 1;
}

const uint8_t *cd_sector(uint32_t lba) {
    uint32_t base = base_lba();
    if (lba < base) return 0;
    if (lba < CD_CORTE_LBA) {
        /* F8: la cota real es la capacidad de la copia baja, no un
         * tope generico: (CORTE-base)*512. Con base=1 (fallback) la
         * vieja cota 0x300000 apuntaba a pila/VGA. */
        uint32_t cap = (CD_CORTE_LBA > base) ?
            (CD_CORTE_LBA - base) * 512u : 0;
        uint32_t off = (lba - base) * 512u;
        if (off >= cap) return 0;
        return (const uint8_t *)(CD_RAM_BASE + off);
    }
    {
        /* F8: guarda anti-envoltorio antes de multiplicar. */
        if (lba - CD_CORTE_LBA > CD_ALTO_MAX / 512u) return 0;
        {
            uint32_t off2 = (lba - CD_CORTE_LBA) * 512u;
            if (off2 >= CD_ALTO_MAX) return 0;
            return (const uint8_t *)(CD_ALTO_BASE + off2);
        }
    }
}
