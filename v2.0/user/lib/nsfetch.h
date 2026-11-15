/* nsfetch.h: fetch HTTP/HTTPS para NetSurf (Hito 2).
 * Sobre u_http/u_tcp + BearSSL. Sin red propia nueva.
 * Cuerpo devuelto en estatico interno: valido hasta la proxima
 * llamada (no liberar). Sin hilos: un fetch cada vez. */
#ifndef NSFETCH_H
#define NSFETCH_H

#define NSFETCH_OK 0
#define NSFETCH_E_RED 1     /* sin red/TCP/TLS/timeout */
#define NSFETCH_E_URL 2     /* URL invalida o esquema no soportado */
#define NSFETCH_E_DNS 3
#define NSFETCH_E_REDIR 4   /* demasiadas redirecciones (max 3) */
#define NSFETCH_E_CUERPO 5  /* cuerpo > 256K */

typedef struct {
    const char *cuerpo;     /* cuerpo HTTP (NUL tras tam) */
    int tam;                /* bytes de cuerpo */
    int codigo;             /* 100..599 (0 si no hubo respuesta) */
    unsigned suite;         /* suite TLS (0 si http o sin handshake) */
    int detalle;            /* codigo BearSSL/u_http en fallos */
} nsfetch_res_t;

int nsfetch_get(const char *url, nsfetch_res_t *r);

/* GET con galletas (v2.8): manda Cookie del tarro (por host) y guarda
   los Set-Cookie de cada respuesta (incluidas redirecciones).
   Mismos codigos que nsfetch_get. */
int nsfetch_get_ck(const char *url, nsfetch_res_t *r);

/* POST con galletas (v2.8): igual, una sola peticion sin seguir
   redirecciones. extra son cabeceras "N: v\r\n" (p. ej.
   Content-Type) que viajan tras la Cookie. Mismos codigos que
   nsfetch_get. */
int nsfetch_post_ck(const char *url, const unsigned char *post,
                    int npost, const char *extra, nsfetch_res_t *r);

/* Trae http/https en el buffer del llamador (una peticion, sin
   redirecciones: como u_http_trae_x). Mismos codigos que
   u_http_trae_x. suite/detalle solo en https (pueden ser 0). */
int nsfetch_trae_x(const char *url, const char *metodo, const char *extra,
                   const unsigned char *post, int npost, u_http_res_t *r,
                   unsigned char *cuerpo, int cap, unsigned espera,
                   unsigned *psuite, int *pdetalle);

#endif
