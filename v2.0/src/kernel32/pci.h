#ifndef F32_PCI_H
#define F32_PCI_H
#include <stdint.h>

/* v2.3-red F1: PCI por puertos 0xCF8/0xCFC (mecanismo 1).
 * Sin enumeracion previa en FacilOS32: se anade lo minimo para
 * localizar el adaptador de red y leer su BAR e IRQ. */

/* Lee/escribe un registro de configuracion (off: desplazamiento en
   bytes; se alinea a dword internamente). */
uint32_t pci_cfg_lee(int bus, int dev, int fun, int off);
void pci_cfg_pon(int bus, int dev, int fun, int off, uint32_t v);

/* Localiza vendor:device (barre bus 0, funciones si multifuncion).
 * 0 si lo encuentra (rellena bus/dev/fun), -1 si no existe. */
int pci_busca(uint16_t vendor, uint16_t device,
              int *bus, int *dev, int *fun);

#endif
