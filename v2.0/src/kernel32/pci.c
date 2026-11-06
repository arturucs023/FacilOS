#include "pci.h"
#include "cpu.h"

#define PCI_DIR 0xCF8u
#define PCI_DAT 0xCFCu

static uint32_t pci_dir(int bus, int dev, int fun, int off) {
    uint32_t d = 0x80000000u | ((uint32_t)(bus & 0xFF) << 16) |
                 ((uint32_t)(dev & 0x1F) << 11) |
                 ((uint32_t)(fun & 0x07) << 8) |
                 ((uint32_t)(off & 0xFC));
    return d;
}

uint32_t pci_cfg_lee(int bus, int dev, int fun, int off) {
    outl(PCI_DIR, pci_dir(bus, dev, fun, off));
    io_espera();
    return inl(PCI_DAT);
}

void pci_cfg_pon(int bus, int dev, int fun, int off, uint32_t v) {
    outl(PCI_DIR, pci_dir(bus, dev, fun, off));
    io_espera();
    outl(PCI_DAT, v);
}

int pci_busca(uint16_t vendor, uint16_t device,
              int *bus, int *dev, int *fun) {
    int b, d;
    /* F1: basta el bus 0 (QEMU pc/i440FX cuelga la NIC ahi). */
    for (b = 0; b < 1; b++) {
        for (d = 0; d < 32; d++) {
            uint32_t id = pci_cfg_lee(b, d, 0, 0);
            if (id == 0xFFFFFFFFu) continue;
            if ((uint16_t)(id & 0xFFFFu) == vendor &&
                (uint16_t)((id >> 16) & 0xFFFFu) == device) {
                if (bus) *bus = b;
                if (dev) *dev = d;
                if (fun) *fun = 0;
                return 0;
            }
            /* Multifuncion: prueba funciones 1..7. */
            if (pci_cfg_lee(b, d, 0, 0x0E) & 0x00800000u) {
                int f;
                for (f = 1; f < 8; f++) {
                    id = pci_cfg_lee(b, d, f, 0);
                    if (id == 0xFFFFFFFFu) continue;
                    if ((uint16_t)(id & 0xFFFFu) == vendor &&
                        (uint16_t)((id >> 16) & 0xFFFFu) == device) {
                        if (bus) *bus = b;
                        if (dev) *dev = d;
                        if (fun) *fun = f;
                        return 0;
                    }
                }
            }
        }
    }
    return -1;
}
