#ifndef F32_GFX_H
#define F32_GFX_H
#include <stdint.h>

void gfx_init(void);
int gfx_activo(void);
void gfx_clear(void);
void gfx_putc(char c, uint8_t attr);
void gfx_puts(const char *s);
uint16_t gfx_w(void);
uint16_t gfx_h(void);

/* Primitivas v1.5 (requieren gfx activo). Coords con signo, clipping. */
void g_pixel(int x, int y, uint8_t color);
void g_rect(int x, int y, int w, int h, uint8_t color);
void g_linea(int x1, int y1, int x2, int y2, uint8_t color);
void g_texto(int x, int y, const char *s, uint8_t color);
void g_limpiar(uint8_t color);

/* v1.6: dibujo con destino (0 frente, 1 dorso en 0xA00000) + blit.
   Por defecto frente (compat v1.5). La consola siempre va al frente. */
void g_destino(int d);
void g_presentar(int x, int y, int w, int h);
void g_dorso_limpia(void);
void g_sprite(int x, int y, int w, int h, const uint32_t *pix);
void g_sprite_t(int x, int y, int w, int h, const uint32_t *pix,
                uint32_t clave);
void g_blit(int x0, int y0, int w, int h, int x1, int y1);
uint32_t g_leer_pixel(int x, int y);

#endif
