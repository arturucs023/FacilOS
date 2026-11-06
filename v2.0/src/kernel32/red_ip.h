#ifndef F32_RED_IP_H
#define F32_RED_IP_H
#include <stdint.h>

/* v2.3-red F2: IPv4 (construccion, checksum, TTL, rutas).
 * Direcciones: u32 con el primer octeto en el byte bajo. */

#define IP_TTL_DEF 64
#define IP_CAB 20
#define IP_MTU_CARGA 1480

/* Suma de comprobacion (RFC 1071). h: cabecera con campo checksum
   a cero para calcular; con checksum incluido para validar. */
uint16_t red_ip_cksum(const uint8_t *h, int n);

/* 0 cabecera valida, -1 invalida (version/IHL/longitud/checksum). */
int red_ip_valida(const uint8_t *h, int n);

/* Envia carga por IPv4 (construye cabecera con TTL+checksum,
   resuelve MAC por ruta+ARP). Devuelve ncarga o negativo:
   -1 sin adaptador, -2 args, -3 sin ruta, -4 ARP, -5 Tx. */
int red_ip_enviar(uint32_t dst, uint8_t proto,
                  const uint8_t *carga, int ncarga, uint8_t ttl);

/* Entrada desde red_entrada (solo valida y cuenta en F2). */
void red_ip_entrada(const uint8_t *m, int tam);

/* Autoprueba con vectores conocidos (serie). */
void red_ip_kat(void);

#endif
