#include "libc.h"

/* Monticulo fijo 2 MB en 0x800000 (tras el monton de usuario v1.6).
   Requiere >=16 MB de RAM. First-fit con union (v2.0: con canarios de
   integridad y contabilidad; el diseño first-fit se mantiene por D3). */
#define HEAP_BASE ((uint32_t)0x800000)
#define HEAP_TAM  ((uint32_t)0x200000)
#define KMAGIC 0x4B484541u   /* "KHEA" */

typedef struct bloque {
    uint32_t magic;
    uint32_t tam;               /* bytes de datos (sin contar pie) */
    struct bloque *sig;
    uint8_t libre;
} bloque_t;
/* Pie al final de los datos: detecta escrituras mas alla del bloque. */
#define PIE(b) (*(uint32_t *)((uint8_t *)(b) + sizeof(bloque_t) + (b)->tam))

static bloque_t *primero = 0;
static uint32_t k_usados = 0;   /* bytes de datos ocupados */
static uint32_t k_pico = 0;
static uint32_t k_dobles = 0;   /* kfree sobre bloque ya libre */
static uint32_t k_corr = 0;     /* canarios rotos detectados */

static int bloque_ok(bloque_t *b) {
    if (!b) return 0;
    if (b->magic != KMAGIC) return 0;
    if ((uint32_t)b < HEAP_BASE ||
        (uint32_t)b + sizeof(bloque_t) + b->tam + 4 > HEAP_BASE + HEAP_TAM)
        return 0;
    if (PIE(b) != KMAGIC) return 0;
    return 1;
}

void mm_init(void) {
    primero = (bloque_t *)HEAP_BASE;
    primero->magic = KMAGIC;
    primero->tam = HEAP_TAM - sizeof(bloque_t) - 4;
    primero->sig = 0;
    primero->libre = 1;
    PIE(primero) = KMAGIC;
    k_usados = k_pico = k_dobles = k_corr = 0;
}

void *kmalloc(uint32_t n) {
    if (!n) n = 1;
    n = (n + 3) & ~3u;
    for (bloque_t *b = primero; b; b = b->sig) {
        if (!bloque_ok(b)) { k_corr++; return 0; }
        if (!b->libre || b->tam < n) continue;
        if (b->tam >= n + sizeof(bloque_t) + 16 + 4) {
            uint32_t resto = b->tam - n - sizeof(bloque_t) - 4;
            bloque_t *nuevo = (bloque_t *)((uint8_t *)b + sizeof(bloque_t) +
                                           n + 4);
            nuevo->magic = KMAGIC;
            nuevo->tam = resto;
            nuevo->sig = b->sig;
            nuevo->libre = 1;
            PIE(nuevo) = KMAGIC;
            b->tam = n;
            PIE(b) = KMAGIC;
            b->sig = nuevo;
        }
        b->libre = 0;
        k_usados += b->tam;
        if (k_usados > k_pico) k_pico = k_usados;
        return (uint8_t *)b + sizeof(bloque_t);
    }
    return 0;
}

void kfree(void *p) {
    if (!p) return;
    bloque_t *b = (bloque_t *)((uint8_t *)p - sizeof(bloque_t));
    if (!bloque_ok(b)) { k_corr++; return; }
    if (b->libre) { k_dobles++; return; }
    b->libre = 1;
    if (k_usados >= b->tam) k_usados -= b->tam;
    else k_usados = 0;
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

void kmalloc_info(uint32_t *usados, uint32_t *pico, uint32_t *dobles,
                  uint32_t *corr) {
    if (usados) *usados = k_usados;
    if (pico) *pico = k_pico;
    if (dobles) *dobles = k_dobles;
    if (corr) *corr = k_corr;
}

/* Autoprueba del heap en arranque: asigna, ensucia, verifica y libera;
   devuelve 0 si todo cuadra (usados vuelve a 0, canarios intactos). */
int mm_autoprueba(void) {
    uint8_t *a = (uint8_t *)kmalloc(64);
    uint8_t *b = (uint8_t *)kmalloc(1024);
    uint8_t *c = (uint8_t *)kmalloc(7);
    uint32_t i;
    if (!a || !b || !c) return -1;
    for (i = 0; i < 64; i++) a[i] = (uint8_t)(i * 3 + 1);
    for (i = 0; i < 1024; i++) b[i] = (uint8_t)(i * 5 + 2);
    for (i = 0; i < 7; i++) c[i] = (uint8_t)(0xA0 + i);
    for (i = 0; i < 64; i++) if (a[i] != (uint8_t)(i * 3 + 1)) return -2;
    for (i = 0; i < 1024; i++) if (b[i] != (uint8_t)(i * 5 + 2)) return -3;
    for (i = 0; i < 7; i++) if (c[i] != (uint8_t)(0xA0 + i)) return -4;
    kfree(b);
    b = (uint8_t *)kmalloc(512);
    if (!b) return -5;
    for (i = 0; i < 512; i++) b[i] = (uint8_t)i;
    for (i = 0; i < 512; i++) if (b[i] != (uint8_t)i) return -6;
    kfree(a);
    kfree(b);
    kfree(c);
    if (k_usados != 0 || k_corr != 0) return -7;
    return 0;
}
