#ifndef F32_RED_ICMP_H
#define F32_RED_ICMP_H
#include <stdint.h>

/* v2.3-red F3: ICMP (echo request/reply). Sin reenvio ni errores
   de destino (eso es F6 con trazar_ruta). Una peticion a la vez. */

void red_icmp_init(void);

/* Entrada: trama Ethernet completa con IPv4 ya validada.
   Responde echo a nuestra IP; casa replies con la espera. */
void red_icmp_entrada(const uint8_t *m, int tam);

/* Emite un echo request y espera su reply.
   Devuelve 1 con rtt_ms/ttl si llega, 0 si agota la espera,
   negativo en error (-1 sin adaptador, -2 args). */
int red_icmp_ping(uint32_t dst, uint16_t ident, uint16_t seq,
                  uint8_t ttl, uint32_t espera_ms,
                  uint16_t *rtt_ms, uint8_t *pttl);

/* Contadores para `red estado` (rsv[1]=ok, rsv[2]=err). */
void red_icmp_contadores(uint32_t *ok, uint32_t *err);

/* Autoprueba (serie). */
void red_icmp_kat(void);

#endif
