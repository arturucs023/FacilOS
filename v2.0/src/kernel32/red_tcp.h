#ifndef F32_RED_TCP_H
#define F32_RED_TCP_H
#include <stdint.h>

/* v2.3-red F5: TCP propio (sin sockets). Subconjunto RFC 793/1122:
   apertura activa/pasiva, SYN/SYN-ACK/ACK, seq/ack 32 bits,
   datos ordenados, checksum con pseudo-cabecera, retransmision
   con RTO y backoff, ventana, cierre ordenado y RST, timeouts.
   Sin opciones, sin reensamblado (OOO se descarta y recupera por
   retransmision), sin 2MSL (se documenta), sin keepalive.
   Direcciones: u32 con el primer octeto en el byte bajo. */

#define TCP_MSS 536        /* carga util maxima por segmento Tx */
#define TCP_RCV 2048       /* ventana de recepcion por conexion */
#define TCP_N 4            /* conexiones simultaneas */

/* Estados (op 7 los devuelve). */
#define TCP_CERRADO 0
#define TCP_ESCUCHA 1
#define TCP_SYN_TX 2
#define TCP_SYN_RX 3
#define TCP_ESTABLECIDO 4
#define TCP_FIN_TX 5       /* FIN nuestro enviado, esperando FIN ajeno */
#define TCP_FIN_RX 6       /* FIN ajeno recibido, falta cerrar() */

/* Ops de la syscall 61 (una sola syscall F5, multiplexada como la
   55/56: solo quedaban 61-62 libres y 62 queda reservada). */
#define TCP_OP_ABRIR 0     /* ptr{ip:4,puerto:2,local:2} -> id | neg */
#define TCP_OP_ESCUCHAR 1  /* ptr{puerto:2} -> id escucha | neg */
#define TCP_OP_ACEPTAR 2   /* ptr{id:4,espera:2} -> id hijo, 0 espera */
#define TCP_OP_ENVIAR 3    /* ptr{id:4}+buf+len -> len | neg */
#define TCP_OP_RECIBIR 4   /* ptr{id:4,espera:2}+buf+max -> B,0,-12 EOF */
#define TCP_OP_CERRAR 5    /* ptr{id:4} -> 0 | neg */
#define TCP_OP_ABORTAR 6   /* ptr{id:4} -> 0 (RST + libera) */
#define TCP_OP_ESTADO 7    /* ptr{id:4} -> estado 1..6 | neg */

/* Errores (comunes con IP/UDP donde aplican): -1 sin adaptador,
   -2 args, -3 sin ruta, -4 ARP agotado, -5 fallo Tx, -6 tabla
   llena o en uso, -7 timeout/conexion (SYN sin respuesta,
   reintentos agotados, RST del par), -8 estado invalido para la
   operacion, -9 conexion abortada (RST), -12 fin remoto (EOF con
   todo entregado). */

/* Abre conexion activa (bloquea acotado ~10 s hasta ESTABLECIDO).
   local 0 = efimero. Devuelve id 1..TCP_N o negativo. */
int red_tcp_abrir(uint32_t ip, uint16_t puerto, uint16_t local,
                  uint32_t pid);
/* Escucha pasiva no bloqueante (backlog: 1 hijo pendiente).
   Devuelve id o negativo. */
int red_tcp_escuchar(uint16_t puerto, uint32_t pid);
/* Acepta hijo de una escucha (bloquea hasta espera_ms, tope 10 s).
   Devuelve id hijo, 0 si agota la espera, o negativo. */
int red_tcp_aceptar(int id_esc, uint32_t pid, uint32_t espera_ms);
/* Envia (bloquea acotado hasta su ACK; stop-and-wait por segmento).
   Devuelve n o negativo. */
int red_tcp_enviar(int id, uint32_t pid, const uint8_t *d, int n);
/* Recibe (bloquea hasta espera_ms). Devuelve bytes, 0 si agota la
   espera, -12 si fin remoto con todo entregado, o negativo. */
int red_tcp_recibir(int id, uint32_t pid, uint8_t *dst, int max,
                    uint32_t espera_ms);
/* Cierre ordenado (FIN; espera acotada ~5 s; libera siempre).
   Devuelve 0 o negativo. */
int red_tcp_cerrar(int id, uint32_t pid);
/* Abortado (RST + libera inmediato). Devuelve 0 o negativo. */
int red_tcp_abortar(int id, uint32_t pid);
/* Estado 1..6 o negativo (-2 sin conexion). */
int red_tcp_estado(int id, uint32_t pid);

/* Entrada desde red_ip_entrada (proto 6). Valida longitud, offset
   y checksum; despacha por puerto+tupla; temporiza. */
void red_tcp_entrada(const uint8_t *m, int tam);

/* Libera las conexiones del proceso (al terminar). Intenta RST
   solo si ruta+ARP estan en cache (sin bloquear); si no, libera
   en silencio. Devuelve cuantas libero. */
int red_tcp_limpia_proceso(uint32_t pid);

/* Autoprueba con captura de Tx (sin trafico al cable): checksum,
   handshake activo/pasivo, datos, retransmision, RST, FIN/EOF,
   segmentos malos. */
void red_tcp_kat(void);

void red_tcp_init(void);

#endif
