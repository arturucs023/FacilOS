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
/* F8: n-esima coincidencia (n=0 primera). Misma semantica; permite
 * detectar una segunda NIC sin cambiar pci_busca. */
int pci_busca_n(uint16_t vendor, uint16_t device, int n,
                int *bus, int *dev, int *fun);
/* F8: tamano de BAR por sizing (escribe todo-1, lee, restaura).
 * Devuelve tamano en bytes (0 si no aplicable). No toca Command. */
uint32_t pci_bar_tam(int bus, int dev, int fun, int bar_off);

#endif
