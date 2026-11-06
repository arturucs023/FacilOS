#ifndef F32_RED_UDP_H
#define F32_RED_UDP_H
#include <stdint.h>

/* v2.3-red F4: UDP propio (sin sockets). Datagramas IPv4 (proto 17)
   con demultiplexor por puerto destino y colas por liga.
   Direcciones: u32 con el primer octeto en el byte bajo. */

#define UDP_CAB 8
#define UDP_TX_MAX 1472     /* carga util maxima de envio (1500-20-8) */
#define UDP_COLA_DATOS 512  /* carga util maxima encolable (DNS cabe) */
#define UDP_LIGAS 4         /* ligas simultaneas */
#define UDP_COLA 4          /* datagramas pendientes por liga */
#define UDP_EFEM_INI 49152  /* puertos efimeros: 49152..65535 */

/* Envia un datagrama. Devuelve n (>0) o negativo (-1 sin adaptador,
   -2 args, -3 sin ruta, -4 ARP agotado, -5 fallo Tx). sport 0 =
   envio sin ligar (la respuesta no tendra cola). TTL 64 fijo. */
int red_udp_enviar(uint32_t dst, uint16_t dpuerto, uint16_t spuerto,
                   const uint8_t *datos, int n);

/* Liga un puerto al proceso pid (0 = efimero automatico). Devuelve
   el puerto ligado o negativo (-2 args, -6 en uso por otro proceso
   o tabla llena con pendientes). La tabla es logica pura (sin
   exigir adaptador, asi el KAT la ejercita); la syscall 58 exige
   adaptador (-1) antes de llamar.
   Ligar el propio puerto es idempotente. Tabla llena: desaloja la
   liga con cola VACIA mas antigua (preferencia al propio pid);
   nunca desaloja una liga con recepcion pendiente. */
int red_udp_ligar(uint16_t puerto, uint32_t pid);

/* Saca el datagrama mas antiguo de la liga (sin espera). Devuelve
   bytes copiados (>0), 0 si vacia, -2 args o puerto sin ligar,
   -6 liga de otro proceso. La syscall 60 exige adaptador (-1).
   Copia {src:4, sport:2, len:2} + hasta max-8 datos (trunca). */
int red_udp_sacar(uint16_t puerto, uint32_t pid, uint32_t *src,
                  uint16_t *sport, uint8_t *dst, int max);

/* Libera las ligas del proceso (al terminar; sus pendientes mueren
   con el: el dueno ya no existe). Devuelve cuantas libero. */
int red_udp_limpia_proceso(uint32_t pid);

/* Entrada desde red_ip_entrada (proto 17). Valida longitud y
   checksum (campo 0 = sin checksum, se acepta); encola por puerto
   destino o descarta (sin receptor, malformado, cola llena). */
void red_udp_entrada(const uint8_t *m, int tam);

/* Contadores: recibidos encolados y errores (malformados, sin
   receptor, cola llena, descartes). */
void red_udp_contadores(uint32_t *ok, uint32_t *err);

/* Autoprueba (serie): vectores de checksum, longitudes, colas,
   desalojo y titularidad. */
void red_udp_kat(void);

void red_udp_init(void);

#endif
