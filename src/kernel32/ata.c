#include "ata.h"
#include "cpu.h"

#define D_DATOS 0x1F0
#define D_ERROR 0x1F1
#define D_SECT  0x1F2
#define D_LBA0  0x1F3
#define D_LBA1  0x1F4
#define D_LBA2  0x1F5
#define D_SEL   0x1F6
#define D_CMD   0x1F7
#define D_EST   0x1F7

static int espera(int con_drq) {
    for (int i = 0; i < 100000; i++) {
        uint8_t e = inb(D_EST);
        if (e & 0x01) return -1;
        if (!(e & 0x80)) {
            if (!con_drq || (e & 0x08)) return 0;
        }
    }
    return -1;
}

static void pon_lba(uint32_t lba, uint32_t n) {
    outb(D_SEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_espera();
    outb(D_SECT, (uint8_t)n);
    io_espera();
    outb(D_LBA0, (uint8_t)lba);
    io_espera();
    outb(D_LBA1, (uint8_t)(lba >> 8));
    io_espera();
    outb(D_LBA2, (uint8_t)(lba >> 16));
    io_espera();
}

static inline void puerto_lee(uint16_t puerto, void *dst, uint32_t n) {
    __asm__ volatile ("rep insw"
                      : "+D" (dst), "+c" (n)
                      : "d" (puerto)
                      : "memory");
}

static inline void puerto_escribe(uint16_t puerto, const void *src, uint32_t n) {
    __asm__ volatile ("rep outsw"
                      : "+S" (src), "+c" (n)
                      : "d" (puerto)
                      : "memory");
}

int ata_leer(uint32_t lba, void *buf, uint32_t nsec) {
    uint16_t *p = (uint16_t *)buf;
    for (uint32_t s = 0; s < nsec; s++) {
        if (espera(0)) return -1;
        pon_lba(lba + s, 1);
        outb(D_CMD, 0x20);
        io_espera();
        if (espera(1)) return -1;
        puerto_lee(D_DATOS, p, 256);
        p += 256;
    }
    return 0;
}

int ata_escribir(uint32_t lba, const void *buf, uint32_t nsec) {
    const uint16_t *p = (const uint16_t *)buf;
    for (uint32_t s = 0; s < nsec; s++) {
        if (espera(0)) return -1;
        pon_lba(lba + s, 1);
        outb(D_CMD, 0x30);
        io_espera();
        if (espera(1)) return -1;
        puerto_escribe(D_DATOS, p, 256);
        p += 256;
    }
    return 0;
}
