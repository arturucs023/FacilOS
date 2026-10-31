#ifndef F32_DRV_H
#define F32_DRV_H

/* Registro de controladores (v2.0, entrega 6): contratos minimos.
 * Cada driver expone nombre + init; el nucleo los inicializa en orden y
 * resume el resultado en serie. Sin capas intermedias: los drivers usan
 * directamente sus primitivas (IRQ, puertos, memoria).
 * El futuro entorno grafico necesitara eventos desacoplados: ver diseño
 * en INFORME-SEGURIDAD/ARQUITECTURA (etapa posterior, no implementado). */

typedef struct {
    const char *nombre;
    int (*init)(void);   /* 0 ok, <0 error (no aborta el arranque) */
} driver_t;

int drv_registra(const char *nombre, int (*init)(void));
int drv_init_todos(void);   /* 0 si todos ok; imprime tabla por serie */

#endif
