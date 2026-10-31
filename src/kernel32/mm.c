#include "libc.h"

/* Monticulo fijo 2 MB en 0x800000 (tras el monton de usuario v1.6).
   Requiere >=16 MB de RAM. First-fit con union. */
#define HEAP_BASE ((uint32_t)0x800000)
#define HEAP_TAM  ((uint32_t)0x200000)

typedef struct bloque {
    uint32_t tam;
    struct bloque *sig;
    uint8_t libre;
} bloque_t;

static bloque_t *primero = 0;

void mm_init(void) {
    primero = (bloque_t *)HEAP_BASE;
    primero->tam = HEAP_TAM - sizeof(bloque_t);
    primero->sig = 0;
    primero->libre = 1;
}

void *kmalloc(uint32_t n) {
    if (!n) n = 1;
    n = (n + 3) & ~3u;
    for (bloque_t *b = primero; b; b = b->sig) {
        if (!b->libre || b->tam < n) continue;
        if (b->tam >= n + sizeof(bloque_t) + 16) {
            bloque_t *nuevo = (bloque_t *)((uint8_t *)b + sizeof(bloque_t) + n);
            nuevo->tam = b->tam - n - sizeof(bloque_t);
            nuevo->sig = b->sig;
            nuevo->libre = 1;
            b->tam = n;
            b->sig = nuevo;
        }
        b->libre = 0;
        return (uint8_t *)b + sizeof(bloque_t);
    }
    return 0;
}

void kfree(void *p) {
    if (!p) return;
    bloque_t *b = (bloque_t *)((uint8_t *)p - sizeof(bloque_t));
    b->libre = 1;
    /* Une en ambas direcciones (mismo motivo que ulib: lista LIFO). */
    int cambio = 1;
    while (cambio) {
        cambio = 0;
        bloque_t *prev = 0, *q = primero;
        while (q && q->sig) {
            bloque_t *s = q->sig;
            if (q->libre && s->libre) {
                if ((uint8_t *)q + sizeof(bloque_t) + q->tam == (uint8_t *)s) {
                    q->tam += sizeof(bloque_t) + s->tam;
                    q->sig = s->sig;
                    cambio = 1;
                    continue;
                }
                if ((uint8_t *)s + sizeof(bloque_t) + s->tam == (uint8_t *)q) {
                    bloque_t *rest = s->sig;
                    s->tam += sizeof(bloque_t) + q->tam;
                    s->sig = rest;
                    if (prev) prev->sig = s;
                    else primero = s;
                    cambio = 1;
                    q = s;
                    continue;
                }
            }
            prev = q;
            q = q->sig;
        }
    }
}
