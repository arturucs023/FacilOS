#ifndef F32_RED_H
#define F32_RED_H
#include <stdint.h>

/* v2.3-red F2: nucleo de red (configuracion, rutas, entrada).
 * Las tramas entran por red_rx (drv_red) y pasan por red_bombea():
 * lo que es para la pila se procesa (ARP) y TODO lo entrante deja
 * copia en el deposito de diagnostico que lee la 54 cruda.
 * Asi la 54 sigue viendo el trafico (t_red F1 intacto) y la pila
 * no pierde su trafico. */

/* Direcciones: u32 con el primer octeto en el byte bajo
 * (p. ej. 10.0.2.2 = 0x0202000A). Siempre acceso por bytes. */
typedef uint32_t red_ip_t;

/* Configuracion L3 (syscall 55 op 0). */
typedef struct {
    uint32_t ip;
    uint32_t mascara;
    uint32_t puerta;
    uint32_t dns;
} red_cfg_t;   /* 16 B */

/* Ruta (syscall 63). gw=0 significa entrega directa. */
typedef struct {
    uint32_t dst;
    uint32_t mask;
    uint32_t gw;
} red_ruta_t;  /* 12 B */

/* Consulta ARP/tipo (syscall 56 tipo 0 y 2). */
typedef struct {
    uint32_t ip;       /* entrada: destino */
    uint8_t mac[6];    /* salida: resuelta */
    uint16_t espera;   /* ms de espera maxima */
} red_consulta_t;  /* 12 B */

/* Sonda IP (syscall 56 tipo 2, diagnostico F2). */
typedef struct {
    uint32_t ip;       /* destino */
    uint8_t ttl;       /* TTL a usar */
    uint8_t proto;     /* protocolo (p. ej. 17) */
    uint16_t espera;   /* reservado (sin espera en F2) */
} red_sonda_t;   /* 8 B */

void red_nucleo_init(void);   /* config vacia, rutas vacias, KAT */
void red_bombea(void);        /* anillo -> entrada + deposito */
int red_hay_crudo(void);      /* 1 si hay trama en deposito */
/* Saca una trama del deposito (max = capacidad). >0 tam, 0 vacio. */
int red_saca_crudo(uint8_t *dst, int max);

/* Configuracion. */
int red_cfg_fijar(int campo, uint32_t v);  /* 1=ip 2=mask 3=gw 4=dns */
void red_cfg_leer(red_cfg_t *c);
uint32_t red_cfg_ip(void);
/* Rutas derivadas (conectada + defecto). Devuelve cuantas (<=max). */
int red_rutas(uint8_t *dst, int max);
/* 0 via puerta (gw), 1 directa, -1 sin ruta. */
int red_ruta_para(uint32_t dst, uint32_t *gw);

#endif
