#include "drv.h"
#include "libc.h"
#include "klog.h"

#define DRV_MAX 16

static driver_t tabla[DRV_MAX];
static int ndrv = 0;

/* F8: recursos declarados por driver para detectar conflictos
 * (PIO/MMIO/IRQ). Sin quitar la API vieja: registra_ex suma. */
static int solapa(uint32_t a, uint32_t na, uint32_t b, uint32_t nb) {
    if (!na || !nb) return 0;
    return (a < b + nb) && (b < a + na);
}

int drv_registra_ex(const char *nombre, int (*init)(void),
                    uint8_t irq, int usa_irq,
                    uint16_t pio, uint16_t pio_len,
                    uint32_t mmio, uint32_t mmio_len) {
    if (!nombre || !init || ndrv >= DRV_MAX) return -1;
    for (int i = 0; i < ndrv; i++) {
        if (tabla[i].nombre && !libc_strcmp(tabla[i].nombre, nombre))
            return -2;   /* duplicado */
        if (usa_irq && tabla[i].usa_irq && tabla[i].irq == irq)
            return -3;   /* IRQ en conflicto */
        if (solapa(tabla[i].pio, tabla[i].pio_len, pio, pio_len))
            return -3;
        if (solapa(tabla[i].mmio, tabla[i].mmio_len, mmio, mmio_len))
            return -3;
    }
    tabla[ndrv].nombre = nombre;
    tabla[ndrv].init = init;
    tabla[ndrv].irq = irq;
    tabla[ndrv].usa_irq = usa_irq;
    tabla[ndrv].pio = pio;
    tabla[ndrv].pio_len = pio_len;
    tabla[ndrv].mmio = mmio;
    tabla[ndrv].mmio_len = mmio_len;
    tabla[ndrv].estado = 0;
    ndrv++;
    return 0;
}

int drv_registra(const char *nombre, int (*init)(void)) {
    return drv_registra_ex(nombre, init, 0, 0, 0, 0, 0, 0);
}

int drv_estado(const char *nombre) {
    for (int i = 0; i < ndrv; i++) {
        if (tabla[i].nombre && !libc_strcmp(tabla[i].nombre, nombre))
            return tabla[i].estado;
    }
    return -1;
}

int drv_init_todos(void) {
    int fallos = 0;
    kprintf("drivers: %d registrados\r\n", ndrv);
    for (int i = 0; i < ndrv; i++) {
        int r = tabla[i].init ? tabla[i].init() : -1;
        tabla[i].estado = r;
        kprintf("  drv ");
        kprintf(tabla[i].nombre);
        kprintf(r == 0 ? " : ok\r\n" : " : FALLO\r\n");
        if (r) {
            fallos++;
            klog(KLOG_WARN, "drv", tabla[i].nombre);
        }
    }
    return fallos ? -1 : 0;
}
