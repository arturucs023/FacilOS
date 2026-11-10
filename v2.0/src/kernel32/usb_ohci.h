#ifndef F32_USB_OHCI_H
#define F32_USB_OHCI_H
#include <stdint.h>

/* FacilOS32 v2.4-USB: OHCI (USB 1.x) por sondeo, sin IRQ nueva.
 * Vale para QEMU (pci-ohci), VirtualBox y HW real. Teclado HID boot
 * (el raton llega despues). DMA por identidad (<4 GB). */

/* Inicializa (0 ok aunque no haya USB; el PS/2 sigue). Idempotente. */
int usb_init(void);
/* 1 si hay controladora operativa. */
int usb_hay(void);
/* Sondeo periodico (10 ms, via kt; no bloquea, no asigna). */
void usb_sondea(void);

#endif
