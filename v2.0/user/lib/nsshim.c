/* nsshim.c: libc minima freestanding para NetSurf en F32 OS.
 * Firmas ANSI (ver nslibc/*.h); todo delega en u_* o es bucle propio.
 * Sin UNIX/BSD/Linux/Windows: ni CRT, ni POSIX, ni libstdc++. */
#include "ulib.h"
#include "sys.h"

/* ---- heap ---- */
void *malloc(unsigned n) { return u_malloc((int)n); }
void *calloc(unsigned nm, unsigned tam) {
    unsigned long long t = (unsigned long long)nm * tam;
    if (tam && t / tam != nm) return 0;
    return u_calloc((int)nm, (int)tam);
}
void *realloc(void *p, unsigned n) { return u_realloc(p, (int)n); }
void free(void *p) { u_free(p); }
void abort(void) {
    u_print("F32-NetSurf: abort\n");
    sys_salir(99);
    for (;;) {
    }
}
void ns_abort(void) { abort(); }

/* ---- cadenas/memoria ---- */
void *memcpy(void *d, const void *s, unsigned n) {
    return u_memcpy(d, s, (int)n);
}
void *memmove(void *d, const void *s, unsigned n) {
    return u_memmove(d, s, (int)n);
}
int memcmp(const void *a, const void *b, unsigned n) {
    return u_memcmp(a, b, (int)n);
}
void *memset(void *d, int c, unsigned n) {
    return u_memset(d, c, (int)n);
}
void *memchr(const void *s, int c, unsigned n) {
    const unsigned char *p = (const unsigned char *)s;
    unsigned i;
    for (i = 0; i < n; i++)
        if (p[i] == (unsigned char)c) return (void *)(p + i);
    return 0;
}
unsigned strlen(const char *s) { return (unsigned)u_strlen(s); }
int strcmp(const char *a, const char *b) { return u_strcmp(a, b); }
int strncmp(const char *a, const char *b, unsigned n) {
    return u_strncmp(a, b, (int)n);
}
char *strncpy(char *d, const char *s, unsigned n) {
    u_strncpy(d, s, (int)n);
    return d;
}
static int baja(unsigned c) {
    return (c >= 'A' && c <= 'Z') ? (int)(c + ('a' - 'A')) : (int)c;
}
int strncasecmp(const char *a, const char *b, unsigned n) {
    unsigned i;
    for (i = 0; i < n; i++) {
        int ca = baja((unsigned char)a[i]);
        int cb = baja((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
    return 0;
}
int strcasecmp(const char *a, const char *b) {
    return strncasecmp(a, b, 0x7fffffffu);
}
int tolower(int c) { return baja((unsigned)c); }
int toupper(int c) {
    return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

/* ---- busqueda binaria (aliases) ---- */
void *bsearch(const void *key, const void *base, unsigned n,
              unsigned tam,
              int (*cmp)(const void *, const void *)) {
    unsigned lo = 0, hi = n;
    const unsigned char *b = (const unsigned char *)base;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2u;
        int r = cmp(key, b + mid * tam);
        if (r < 0)
            hi = mid;
        else if (r > 0)
            lo = mid + 1;
        else
            return (void *)(b + mid * tam);
    }
    return 0;
}

/* ---- salida minima (solo debug NDEBUG-fuera; el codigo
   productivo no la usa) ---- */
typedef struct {
    int _ns_fd;
} FILE;

static FILE ns_out;
static FILE ns_err;
FILE *__ns_stdout = &ns_out;
FILE *__ns_stderr = &ns_err;

static void pon_u(unsigned v, unsigned base, int mayus) {
    char b[12];
    int i = 0;
    if (!v) {
        u_print("0");
        return;
    }
    while (v && i < 11) {
        unsigned d = v % base;
        b[i++] = (char)(d < 10 ? '0' + d : (mayus ? 'A' : 'a') + d - 10);
        v /= base;
    }
    while (i > 0) {
        char c[2];
        i--;
        c[0] = b[i];
        c[1] = 0;
        u_print(c);
    }
}

static void pon_txt(const char *s, int n) {
    int i;
    for (i = 0; i < n && s[i]; i++) {
        char c[2];
        c[0] = s[i];
        c[1] = 0;
        u_print(c);
    }
}

int printf(const char *fmt, ...) {
    __builtin_va_list ap;
    int i = 0;
    if (!fmt) return -1;
    __builtin_va_start(ap, fmt);
    while (fmt[i]) {
        if (fmt[i] != '%') {
            char c[2];
            c[0] = fmt[i++];
            c[1] = 0;
            u_print(c);
            continue;
        }
        i++;
        if (fmt[i] == 's') {
            const char *s = __builtin_va_arg(ap, const char *);
            u_print(s ? s : "(null)");
            i++;
        } else if (fmt[i] == 'd' || fmt[i] == 'i') {
            int v = __builtin_va_arg(ap, int);
            if (v < 0) {
                u_print("-");
                v = -v;
            }
            pon_u((unsigned)v, 10u, 0);
            i++;
        } else if (fmt[i] == 'u') {
            pon_u(__builtin_va_arg(ap, unsigned), 10u, 0);
            i++;
        } else if (fmt[i] == 'x') {
            pon_u(__builtin_va_arg(ap, unsigned), 16u, 0);
            i++;
        } else if (fmt[i] == 'X') {
            pon_u(__builtin_va_arg(ap, unsigned), 16u, 1);
            i++;
        } else if (fmt[i] == 'c') {
            char c[2];
            c[0] = (char)__builtin_va_arg(ap, int);
            c[1] = 0;
            u_print(c);
            i++;
        } else if (fmt[i] == 'p') {
            u_print("0x");
            pon_u(__builtin_va_arg(ap, unsigned), 16u, 0);
            i++;
        } else if (fmt[i] == '.' && fmt[i + 1] == '*' &&
                   fmt[i + 2] == 's') {
            int n = __builtin_va_arg(ap, int);
            const char *s = __builtin_va_arg(ap, const char *);
            pon_txt(s ? s : "", n);
            i += 3;
        } else if (fmt[i] == '%') {
            u_print("%");
            i++;
        } else {
            char c[2];
            c[0] = '%';
            c[1] = 0;
            u_print(c);
            if (fmt[i]) {
                c[0] = fmt[i++];
                u_print(c);
            }
        }
    }
    __builtin_va_end(ap);
    return 0;
}

int fprintf(FILE *f, const char *fmt, ...) {
    __builtin_va_list ap;
    int i = 0;
    (void)f;
    if (!fmt) return -1;
    __builtin_va_start(ap, fmt);
    while (fmt[i]) {
        if (fmt[i] != '%') {
            char c[2];
            c[0] = fmt[i++];
            c[1] = 0;
            u_print(c);
            continue;
        }
        i++;
        if (fmt[i] == 's') {
            const char *s = __builtin_va_arg(ap, const char *);
            u_print(s ? s : "(null)");
            i++;
        } else if (fmt[i] == 'd' || fmt[i] == 'i') {
            int v = __builtin_va_arg(ap, int);
            if (v < 0) {
                u_print("-");
                v = -v;
            }
            pon_u((unsigned)v, 10u, 0);
            i++;
        } else if (fmt[i] == 'u') {
            pon_u(__builtin_va_arg(ap, unsigned), 10u, 0);
            i++;
        } else if (fmt[i] == 'x') {
            pon_u(__builtin_va_arg(ap, unsigned), 16u, 0);
            i++;
        } else if (fmt[i] == 'c') {
            char c[2];
            c[0] = (char)__builtin_va_arg(ap, int);
            c[1] = 0;
            u_print(c);
            i++;
        } else if (fmt[i] == 'p') {
            u_print("0x");
            pon_u(__builtin_va_arg(ap, unsigned), 16u, 0);
            i++;
        } else if (fmt[i] == '%') {
            u_print("%");
            i++;
        } else {
            u_print("%");
            if (fmt[i]) {
                char c[2];
                c[0] = fmt[i++];
                u_print(c);
            }
        }
    }
    __builtin_va_end(ap);
    return 0;
}
