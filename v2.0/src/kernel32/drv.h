#ifndef F32_DRV_H
#define F32_DRV_H
#include <stdint.h>

/* Registro de controladores (v2.0, entrega 6): contratos minimos.
 * Cada driver expone nombre + init; el nucleo los inicializa en orden y
 * resume el resultado en serie. Sin capas intermedias: los drivers usan
 * directamente sus primitivas (IRQ, puertos, memoria).
 * El futuro entorno grafico necesitara eventos desacoplados: ver diseño
 * en INFORME-SEGURIDAD/ARQUITECTURA (etapa posterior, no implementado). */

typedef struct {
    const char *nombre;
    int (*init)(void);   /* 0 ok, <0 error (no aborta el arranque) */
    /* F8: recursos declarados (0 = no aplica). Sirven para detectar
     * conflictos al registrar; el driver sigue usando sus primitivas. */
    uint8_t irq;
    int usa_irq;
    uint16_t pio;
    uint16_t pio_len;
    uint32_t mmio;
    uint32_t mmio_len;
    int estado;          /* ultimo rc de init */
} driver_t;

int drv_registra(const char *nombre, int (*init)(void));
int drv_registra_ex(const char *nombre, int (*init)(void),
                    uint8_t irq, int usa_irq,
                    uint16_t pio, uint16_t pio_len,
                    uint32_t mmio, uint32_t mmio_len);
int drv_estado(const char *nombre);   /* 0 ok, <0 error, -1 ausente */
int drv_init_todos(void);   /* 0 si todos ok; imprime tabla por serie */

#endif
