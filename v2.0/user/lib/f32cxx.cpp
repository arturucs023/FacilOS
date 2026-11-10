/* Runtime C++ minimo de FacilOS (F32 C++, v2.4.2).
 * Compila con g++ -ffreestanding -nostdlib -fno-exceptions -fno-rtti.
 * Sin libstdc++, sin libc anfitriona, sin POSIX: todo sobre u_*.
 * Excepciones y RTTI desactivados por diseno (ver docs/development/F32-CPP.md).
 * new devuelve 0 si no hay memoria (sin excepciones).
 * Destructores globales: __cxa_atexit/__cxa_finalize (tabla fija 64).
 * Estaticos locales: guardas Itanium monohilo (sin reentrancia). */
#include "ulib.h"
#include "sys.h"

/* ---- memoria global: delega en el monticulo de usuario ---- */
void *operator new(__SIZE_TYPE__ n) { return u_malloc((int)n); }
void *operator new[](__SIZE_TYPE__ n) { return u_malloc((int)n); }
void operator delete(void *p) { u_free(p); }
void operator delete[](void *p) { u_free(p); }
/* sized-delete (C++14): el llamante ignora el tamano. */
void operator delete(void *p, __SIZE_TYPE__) { u_free(p); }
void operator delete[](void *p, __SIZE_TYPE__) { u_free(p); }

/* memcpy/memmove/memcmp/memset globales: el codegen de g++ los emite
   (copias de agregados, inicializacion). Sin libc: envuelven u_*. */
extern "C" {
void *memcpy(void *d, const void *s, unsigned n) {
    return u_memcpy(d, s, (int)n);
}
void *memmove(void *d, const void *s, unsigned n) {
    return u_memmove(d, s, (int)n);
}
int memcmp(const void *a, const void *b, unsigned n) {
    return u_memcmp(a, b, (int)n);
}
void *memset(void *d, int c, unsigned n) { return u_memset(d, c, (int)n); }
unsigned strlen(const char *s) { return (unsigned)u_strlen(s); }
}

/* ---- llamada virtual pura: aborta con codigo 99 ---- */
extern "C" void __cxa_pure_virtual(void) {
    u_print("F32-C++: llamada virtual pura\n");
    sys_salir(99);
    for (;;) {
    }
}

/* ---- finalizacion: tabla de destructores (atexit) ---- */
struct F32Dtor {
    void (*f)(void *);
    void *arg;
};
static F32Dtor f32_dtors[64];
static int f32_ndtor = 0;
void *__dso_handle = 0;

extern "C" int __cxa_atexit(void (*f)(void *), void *arg, void *dso) {
    (void)dso;
    if (f32_ndtor >= 64) return -1;
    f32_dtors[f32_ndtor].f = f;
    f32_dtors[f32_ndtor].arg = arg;
    f32_ndtor++;
    return 0;
}

/* atexit: MinGW emite atexit (no __cxa_atexit) aunque se pida
   -fuse-cxa-atexit. Tabla propia; fini corre ambas en inverso. */
static void (*f32_ax[64])(void);
static int f32_nax = 0;

extern "C" int atexit(void (*f)(void)) {
    if (f32_nax >= 64) return -1;
    f32_ax[f32_nax++] = f;
    return 0;
}

extern "C" void __cxa_finalize(void *dso) {
    (void)dso;
    while (f32_nax > 0) {
        f32_nax--;
        f32_ax[f32_nax]();
    }
    while (f32_ndtor > 0) {
        f32_ndtor--;
        f32_dtors[f32_ndtor].f(f32_dtors[f32_ndtor].arg);
    }
}

/* ---- guardas de estaticos locales (Itanium, monohilo) ---- */
typedef unsigned long long F32Guard;
extern "C" int __cxa_guard_acquire(F32Guard *g) {
    return (*(volatile unsigned char *)g == 0);
}
extern "C" void __cxa_guard_release(F32Guard *g) {
    *(volatile unsigned char *)g = 1;
}
extern "C" void __cxa_guard_abort(F32Guard *) {}

/* ---- init/fini: recorre .init_array (simbolos del prog.ld) ----
 * OJO decoracion MinGW: el C antepone `_`; los simbolos del script
 * se referencian con etiqueta asm literal para no depender de eso. */
typedef void (*F32Ctor)(void);
extern "C" F32Ctor __init_array_start[] __asm__("__init_array_start");
extern "C" F32Ctor __init_array_end[] __asm__("__init_array_end");

static int f32_init_done = 0;

extern "C" void __f32_cxx_init(void) {
    if (f32_init_done) return;
    f32_init_done = 1;
    for (F32Ctor *p = __init_array_start; p < __init_array_end; p++)
        (*p)();
}

/* __main: g++ (MinGW) emite `call __main` al inicio de main para la
   init estatica. Aqui ES la init real (idempotente). Las apps C++
   no deben definir __main (las C conservan su stub vacio). */
extern "C" void __main(void) { __f32_cxx_init(); }

extern "C" void __f32_cxx_fini(void) { __cxa_finalize(0); }

/* Anula la dependencia de libgcc en division 64-bit: no existe en
   FacilOS. Si algun programa la necesita, el enlazado falla con
   mensaje claro en vez de colarse un objeto del anfitrion. */
