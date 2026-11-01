#include "blk.h"
#include "ata.h"
#include "cpu.h"
#include "libc.h"

#define R_EST 0x1F7
#define R_CMD 0x1F7
#define R_SEL 0x1F6

static int presente = 0;
static uint32_t nsectores = 0;

static int espera_cond(int con_drq) {
    for (int i = 0; i < 100000; i++) {
        uint8_t e = inb(R_EST);
        if (e == 0xFF) return -2;          /* bus flotante: sin dispositivo */
        if (e & 0x01) return -1;           /* ERR */
        if (!(e & 0x80)) {
            if (!con_drq || (e & 0x08)) return 0;
        }
    }
    return -1;
}

int blk_init(void) {
    uint16_t id[256];
    uint16_t *p = id;
    presente = 0;
    nsectores = 0;
    outb(R_SEL, 0xE0);                     /* maestro */
    io_espera();
    /* Presencia: bus flotante (0xFF) o jamas listo (0x00) = sin disco.
       Un ATA presente responde DRDY (puede tardar: se espera). */
    {
        int i, listo = 0;
        for (i = 0; i < 100000; i++) {
            uint8_t e = inb(R_EST);
            if (e == 0xFF) break;
            if (e & 0x80) continue;        /* BSY: aun no */
            if (e & 0x40) { listo = 1; break; }
            break;                        /* 0x00 u otro: sin dispositivo */
        }
        if (!listo) {
            kprintf("blk: sin disco en primario-maestro\r\n");
            return -1;
        }
    }
    if (espera_cond(0)) {
        kprintf("blk: dispositivo no listo\r\n");
        return -1;
    }
    outb(R_CMD, 0xEC);                     /* IDENTIFY */
    io_espera();
    if (espera_cond(1)) {
        kprintf("blk: IDENTIFY sin datos\r\n");
        return -1;
    }
    {
        uint16_t *q = p;
        uint32_t n = 256;
        __asm__ volatile ("rep insw"
                          : "+D" (q), "+c" (n)
                          : "d" ((uint16_t)0x1F0)
                          : "memory");
    }
    nsectores = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    if (nsectores == 0 || nsectores > 0x0FFFFFFFu) {
        kprintf("blk: capacidad imposible\r\n");
        nsectores = 0;
        return -1;
    }
    presente = 1;
    kprintf("blk: disco ATA %u sectores (%u KB)\r\n", nsectores,
            nsectores / 2);
    return 0;
}

int blk_presente(void) { return presente; }
uint32_t blk_sectores(void) { return nsectores; }

static int rango_ok(uint32_t lba, uint32_t nsec) {
    if (!presente || nsec == 0) return 0;
    if (lba >= nsectores) return 0;
    if (nsec > nsectores - lba) return 0;
    return 1;
}

int blk_leer(uint32_t lba, void *buf, uint32_t nsec) {
    if (!rango_ok(lba, nsec) || !buf) return -1;
    return ata_leer(lba, buf, nsec);
}

int blk_escribir(uint32_t lba, const void *buf, uint32_t nsec) {
    if (!rango_ok(lba, nsec) || !buf) return -1;
    return ata_escribir(lba, buf, nsec);
}

int blk_flush(void) {
    int i;
    if (!presente) return -1;
    outb(R_CMD, 0xE7);                     /* FLUSH CACHE */
    io_espera();
    for (i = 0; i < 100000; i++) {
        uint8_t e = inb(R_EST);
        if (e == 0xFF) return -1;
        if (e & 0x01) return -1;
        if (!(e & 0x80)) return 0;
    }
    return -1;
}
