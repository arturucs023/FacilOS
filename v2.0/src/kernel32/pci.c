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
    return pci_busca_n(vendor, device, 0, bus, dev, fun);
}

int pci_busca_n(uint16_t vendor, uint16_t device, int n,
                int *bus, int *dev, int *fun) {
    int b, d, hall = 0;
    /* F8: se mantiene bus 0 (QEMU pc/i440FX cuelga la NIC ahi).
     * Documentado como limite, no como enumeracion general. */
    for (b = 0; b < 1; b++) {
        for (d = 0; d < 32; d++) {
            uint32_t id = pci_cfg_lee(b, d, 0, 0);
            if (id == 0xFFFFFFFFu) continue;
            if ((uint16_t)(id & 0xFFFFu) == vendor &&
                (uint16_t)((id >> 16) & 0xFFFFu) == device) {
                if (hall == n) {
                    if (bus) *bus = b;
                    if (dev) *dev = d;
                    if (fun) *fun = 0;
                    return 0;
                }
                hall++;
            }
            /* Multifuncion: prueba funciones 1..7. */
            if (pci_cfg_lee(b, d, 0, 0x0E) & 0x00800000u) {
                int f;
                for (f = 1; f < 8; f++) {
                    id = pci_cfg_lee(b, d, f, 0);
                    if (id == 0xFFFFFFFFu) continue;
                    if ((uint16_t)(id & 0xFFFFu) == vendor &&
                        (uint16_t)((id >> 16) & 0xFFFFu) == device) {
                        if (hall == n) {
                            if (bus) *bus = b;
                            if (dev) *dev = d;
                            if (fun) *fun = f;
                            return 0;
                        }
                        hall++;
                    }
                }
            }
        }
    }
    return -1;
}

uint32_t pci_bar_tam(int bus, int dev, int fun, int bar_off) {
    uint32_t orig, sz;
    if (bar_off < 0x10 || bar_off > 0x24) return 0;
    orig = pci_cfg_lee(bus, dev, fun, bar_off);
    pci_cfg_pon(bus, dev, fun, bar_off, 0xFFFFFFFFu);
    sz = pci_cfg_lee(bus, dev, fun, bar_off);
    pci_cfg_pon(bus, dev, fun, bar_off, orig);
    if ((orig & 1u) == 0) {
        /* Memoria: bits 4..31. */
        sz &= 0xFFFFFFF0u;
        if (!sz) return 0;
        return (~sz) + 1u;
    }
    /* PIO: bits 2..31. */
    sz &= 0xFFFFFFFCu;
    if (!sz) return 0;
    return ((~sz) + 1u) & 0xFFFFu;
}
