#include "drv.h"
#include "libc.h"

#define DRV_MAX 16

static driver_t tabla[DRV_MAX];
static int ndrv = 0;

int drv_registra(const char *nombre, int (*init)(void)) {
    if (!nombre || !init || ndrv >= DRV_MAX) return -1;
    tabla[ndrv].nombre = nombre;
    tabla[ndrv].init = init;
    ndrv++;
    return 0;
}

int drv_init_todos(void) {
    int fallos = 0;
    kprintf("drivers: %d registrados\r\n", ndrv);
    for (int i = 0; i < ndrv; i++) {
        int r = tabla[i].init ? tabla[i].init() : -1;
        kprintf("  drv ");
        kprintf(tabla[i].nombre);
        kprintf(r == 0 ? " : ok\r\n" : " : FALLO\r\n");
        if (r) fallos++;
    }
    return fallos ? -1 : 0;
}
