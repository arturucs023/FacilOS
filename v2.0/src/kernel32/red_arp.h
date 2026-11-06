#ifndef F32_RED_ARP_H
#define F32_RED_ARP_H
#include <stdint.h>

/* v2.3-red F2: ARP (solicitud/respuesta, cache con caducidad). */

void red_arp_init(void);
/* Procesa una trama Ethernet recibida (EtherType ya 0x0806). */
void red_arp_entrada(const uint8_t *m, int tam);
/* Resuelve IP->MAC (emite solicitud si no esta en cache).
   0 ok (mac rellena), 1 espera agotada, -1 sin adaptador. */
int red_arp_resolver(uint32_t ip, int espera_ms, uint8_t *mac);
/* Anuncio gratuito al fijar IP (si hay adaptador e IP valida). */
void red_arp_anuncia(void);
/* Entradas validas en cache (diagnostico). */
int red_arp_n(void);

#endif
