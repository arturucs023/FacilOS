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
char *strdup(const char *s) {
    unsigned n = 0;
    char *d;
    if (!s) return 0;
    while (s[n]) n++;
    d = (char *)malloc(n + 1u);
    if (!d) return 0;
    for (n = 0; s[n]; n++) d[n] = s[n];
    d[n] = 0;
    return d;
}
char *strchr(const char *s, int c) {
    if (!s) return 0;
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (char)c ? 0 : (char *)s;
}

/* strtol/strtoul 32 bit (long i386 = 32). Base 0 autodetecta. */
static unsigned long xdig(char c, int base, int *ok) {
    unsigned v;
    if (c >= '0' && c <= '9') v = (unsigned)(c - '0');
    else if (c >= 'a' && c <= 'z') v = (unsigned)(c - 'a') + 10u;
    else if (c >= 'A' && c <= 'Z') v = (unsigned)(c - 'A') + 10u;
    else { *ok = 0; return 0; }
    if (v >= (unsigned)base) { *ok = 0; return 0; }
    *ok = 1;
    return v;
}
unsigned long strtoul(const char *s, char **fin, int base) {
    unsigned long v = 0;
    const char *ini = s;
    int neg = 0, dig;
    if (!s) {
        if (fin) *fin = (char *)s;
        return 0;
    }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (!base) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;
        else if (s[0] == '0') base = 8;
        else base = 10;
    }
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    dig = 0;
    while (*s) {
        int good = 0;
        unsigned d = xdig(*s, base, &good);
        if (!good) break;
        if (v > (0xfffffffful - d) / (unsigned)base) {
            v = 0xfffffffful;
            dig = 1;
            while (*++s) {
                xdig(*s, base, &good);
                if (!good) break;
            }
            break;
        }
        v = v * (unsigned)base + d;
        dig = 1;
        s++;
    }
    if (!dig) s = ini;   /* sin digitos: no avanza */
    if (fin) *fin = (char *)s;
    return neg ? 0u - v : v;
}
long strtol(const char *s, char **fin, int base) {
    int neg = 0;
    const char *p = s;
    unsigned long v;
    if (!s) {
        if (fin) *fin = (char *)s;
        return 0;
    }
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '-') neg = 1;
    v = strtoul(s, fin, base);
    if (neg) {
        if (v > 0x80000000ul) return (long)0x80000000ul;
        return -(long)v;
    }
    if (v > 0x7ffffffful) return 0x7fffffffl;
    return (long)v;
}
int abs(int v) { return v < 0 ? -v : v; }

/* ---- busqueda binaria (aliases) y reloj (time() para BearSSL) ---- */
long long time(long long *t) {
    unsigned int v6[6];
    long y, m, d, hh, mi, ss, era, yoe, doy, doe, days;
    long long secs;
    if (u_reloj(v6)) return -1;
    y = (long)v6[0];
    m = (long)v6[1];
    d = (long)v6[2];
    hh = (long)v6[3];
    mi = (long)v6[4];
    ss = (long)v6[5];
    /* El CMOS da el ano con 2 digitos (ver sc_44): 2000-2099. */
    if (y < 100) y += 2000;
    if (y < 1970 || y > 2105 || m < 1 || m > 12 || d < 1 || d > 31 ||
        hh < 0 || hh > 23 || mi < 0 || mi > 59 || ss < 0 || ss > 60)
        return -1;
    /* days_from_civil (Hinnant), todo 32 bits con signo. */
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    days = era * 146097 + doe - 719468;
    secs = (long long)days * 86400LL + (long long)hh * 3600LL +
           (long long)mi * 60LL + (long long)ss;
    if (t) *t = secs;
    return secs;
}

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

/* ---- stdin/errno dummy (flex/lex y nsutils los nombran) ---- */
static FILE ns_in;
FILE *__ns_stdin = &ns_in;
int errno = 0;
void exit(int n) {
    u_print("F32-NetSurf: exit\n");
    sys_salir(n);
    for (;;) {
    }
}
int getc(FILE *f) {
    /* El lexer solo escanea cadenas; leer de fichero no existe. */
    (void)f;
    abort();
    return -1;
}

/* ---- snprintf/vsnprintf/sprintf (buffer; %d%i%u%x%X%s%c%%,
   ancho con espacios, .N/.* para %s, modificador l) ----
   Devuelve la longitud que habria escrito (como ANSI) aunque
   trunque o d sea 0/NULL; escribe NUL si n > 0.
   fread/ferror/clearerr: solo para que enlace el lexer flex
   (lee de cadena; si alguna vez leyera de fichero, aborta). */
unsigned fread(void *b, unsigned tam, unsigned n, FILE *f) {
    (void)b;
    (void)tam;
    (void)n;
    (void)f;
    abort();
    return 0;
}
unsigned fwrite(const void *p, unsigned tam, unsigned n, FILE *f) {
    /* Solo consola (stdout/stderr); otro FILE se descarta.
       Existe para el ECHO por defecto del lexer flex. */
    if ((f == __ns_stdout || f == __ns_stderr) && p && tam && n) {
        unsigned i, tot = tam * n;
        for (i = 0; i < tot; i++) {
            char c[2];
            c[0] = ((const char *)p)[i];
            c[1] = 0;
            u_print(c);
        }
        return n;
    }
    return 0;
}
int ferror(FILE *f) {
    (void)f;
    return 0;
}
void clearerr(FILE *f) { (void)f; }
static void bv_ch(char *d, unsigned n, unsigned *t, char c) {
    if (n > *t + 1u) d[*t] = c;
    (*t)++;
}
static void bv_txt(char *d, unsigned n, unsigned *t, const char *s,
                   int max) {
    int i = 0;
    if (!s) s = "(null)";
    while (s[i] && (max < 0 || i < max)) {
        bv_ch(d, n, t, s[i]);
        i++;
    }
}
static void bv_un(char *d, unsigned n, unsigned *t, unsigned v,
                  unsigned base, int mayus, int ancho) {
    char b[12];
    int i = 0, k;
    if (!v) b[i++] = '0';
    while (v && i < 11) {
        unsigned x = v % base;
        b[i++] = (char)(x < 10 ? '0' + x : (mayus ? 'A' : 'a') + x - 10);
        v /= base;
    }
    while (i < ancho && i < 11) b[i++] = ' ';
    for (k = i - 1; k >= 0; k--) bv_ch(d, n, t, b[k]);
}
int vsnprintf(char *d, unsigned n, const char *fmt,
              __builtin_va_list ap) {
    unsigned t = 0;
    int i = 0;
    if (!fmt) return -1;
    while (fmt[i]) {
        if (fmt[i] != '%') {
            bv_ch(d, n, &t, fmt[i++]);
            continue;
        }
        i++;
        {
            int ancho = 0, prec = -1, es_l = 0;
            while (fmt[i] >= '0' && fmt[i] <= '9') {
                ancho = ancho * 10 + (fmt[i] - '0');
                i++;
            }
            if (fmt[i] == '.') {
                i++;
                if (fmt[i] == '*') {
                    prec = __builtin_va_arg(ap, int);
                    i++;
                } else {
                    prec = 0;
                    while (fmt[i] >= '0' && fmt[i] <= '9') {
                        prec = prec * 10 + (fmt[i] - '0');
                        i++;
                    }
                }
            }
            while (fmt[i] == 'l') { es_l = 1; i++; }
            (void)es_l;   /* long i386 = 32 bit: igual que int */
            if (fmt[i] == 's') {
                bv_txt(d, n, &t, __builtin_va_arg(ap, const char *),
                       prec);
                i++;
            } else if (fmt[i] == 'd' || fmt[i] == 'i') {
                int v = __builtin_va_arg(ap, int);
                if (v < 0) {
                    bv_ch(d, n, &t, '-');
                    v = -v;
                }
                bv_un(d, n, &t, (unsigned)v, 10u, 0, ancho);
                i++;
            } else if (fmt[i] == 'u') {
                bv_un(d, n, &t, __builtin_va_arg(ap, unsigned), 10u,
                      0, ancho);
                i++;
            } else if (fmt[i] == 'x') {
                bv_un(d, n, &t, __builtin_va_arg(ap, unsigned), 16u,
                      0, ancho);
                i++;
            } else if (fmt[i] == 'X') {
                bv_un(d, n, &t, __builtin_va_arg(ap, unsigned), 16u,
                      1, ancho);
                i++;
            } else if (fmt[i] == 'c') {
                bv_ch(d, n, &t, (char)__builtin_va_arg(ap, int));
                i++;
            } else if (fmt[i] == '%') {
                bv_ch(d, n, &t, '%');
                i++;
            } else {
                bv_ch(d, n, &t, '%');
                if (fmt[i]) bv_ch(d, n, &t, fmt[i++]);
            }
        }
    }
    if (n > 0u) d[t < n ? t : n - 1u] = 0;
    return (int)t;
}
int snprintf(char *d, unsigned n, const char *fmt, ...) {
    __builtin_va_list ap;
    int r;
    __builtin_va_start(ap, fmt);
    r = vsnprintf(d, n, fmt, ap);
    __builtin_va_end(ap);
    return r;
}
int sprintf(char *d, const char *fmt, ...) {
    __builtin_va_list ap;
    int r;
    __builtin_va_start(ap, fmt);
    r = vsnprintf(d, 0x7fffffffu, fmt, ap);
    __builtin_va_end(ap);
    return r;
}

/* ---- division 64 bit (el link de programas no lleva -lgcc;
   solo aritmetica del compilador, sin llamadas al SO).
   Divisor 0 = 0 (en F32 OS no hay SIGFPE). ---- */
unsigned long long __udivdi3(unsigned long long a,
                             unsigned long long b) {
    unsigned long long q = 0, r = 0;
    int i;
    if (!b) return 0;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1u);
        if (r >= b) {
            r -= b;
            q |= (1ull << i);
        }
    }
    return q;
}
unsigned long long __umoddi3(unsigned long long a,
                             unsigned long long b) {
    unsigned long long r = 0;
    int i;
    if (!b) return 0;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1u);
        if (r >= b) r -= b;
    }
    return r;
}
long long __divdi3(long long a, long long b) {
    int neg = 0;
    unsigned long long q;
    if (a < 0) { neg = !neg; a = -a; }
    if (b < 0) { neg = !neg; b = -b; }
    q = __udivdi3((unsigned long long)a, (unsigned long long)b);
    return neg ? -(long long)q : (long long)q;
}
long long __moddi3(long long a, long long b) {
    int neg = 0;
    unsigned long long r;
    if (a < 0) { neg = 1; a = -a; }
    if (b < 0) b = -b;
    r = __umoddi3((unsigned long long)a, (unsigned long long)b);
    return neg ? -(long long)r : (long long)r;
}
