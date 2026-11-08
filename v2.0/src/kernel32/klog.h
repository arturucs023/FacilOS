#ifndef F32_KLOG_H
#define F32_KLOG_H
#include <stdint.h>

/* F8: diagnostico centralizado del nucleo (propio, sin Ring3/gfx).
 * Niveles DEBUG/INFO/WARN/ERROR/FATAL. Emisores: serie (siempre),
 * consola (si disponible) y buffer circular interno.
 * Seguro en fallo: sin malloc/paginacion/gfx; polling UART acotado;
 * FATAL vuelca y detiene. El modo emergencia funciona sin gfx. */

#define KLOG_DEBUG 0
#define KLOG_INFO 1
#define KLOG_WARN 2
#define KLOG_ERROR 3
#define KLOG_FATAL 4

void klog_init(void);
void klog_consola(int ok);
void klog_umbral(int nivel);
int klog_nivel(void);
void klog(int nivel, const char *subs, const char *msg);
void klog_hex(int nivel, const char *subs, const char *tit, uint32_t v);
/* Vuelca el buffer circular por serie (diagnostico post-mortem). */
void klog_volcar(void);

#endif
