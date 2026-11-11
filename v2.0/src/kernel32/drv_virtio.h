#ifndef F32_DRV_VIRTIO_H
#define F32_DRV_VIRTIO_H
#include <stdint.h>
#include "drv_red.h"

/* Backend virtio-net (transporte PCI legacy). Lo prueba red_init
   solo si no hay RTL8139. 0 = adaptador listo (ya iniciado). */
int vio_probe(void);
int vio_tx(const uint8_t *marco, int tam);
int vio_rx(uint8_t *marco, int max);
int vio_mac(uint8_t *dst);
int vio_info(red_if_t *f);
int vio_hay(void);
/* Diagnostico aunque no haya adaptador (etapa/id/loc/qn del probe). */
void vio_debug(uint32_t *etapa, uint32_t *id, uint32_t *loc,
               uint32_t *qn);

#endif
