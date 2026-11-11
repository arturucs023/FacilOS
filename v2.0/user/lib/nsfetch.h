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

#endif
