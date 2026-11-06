#include "ulib.h"

/* Sin paginacion la pila siempre esta presente: la sonda de pila de
   MinGW (marcos grandes) es un no-op. Evita enlazar libgcc. */
void __chkstk_ms(void) {
}

void __chkstk(void) {
}

/* ---- cadenas ---- */
int u_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

int u_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int u_strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
    }
    return 0;
}

void u_strcpy(char *d, const char *s) {
    while ((*d++ = *s++)) { }
}

void u_strncpy(char *d, const char *s, int n) {
    int i = 0;
    while (i < n && s[i]) { d[i] = s[i]; i++; }
    while (i < n) d[i++] = 0;
}

void u_strcat(char *d, const char *s) {
    while (*d) d++;
    while ((*d++ = *s++)) { }
}

char *u_strchr(const char *s, int c) {
    while (*s) {
        if (*s == c) return (char *)s;
        s++;
    }
    return c ? 0 : (char *)s;
}

int u_atoi(const char *s) {
    int v = 0, neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}

void u_utoa(unsigned int v, char *b, int base) {
    static const char dig[] = "0123456789abcdef";
    char r[32];
    int i = 0, n = 0;
    if (base < 2) base = 10;
    if (!v) b[n++] = '0';
    else {
        while (v && i < 31) { r[i++] = dig[v % (unsigned int)base]; v /= (unsigned int)base; }
        while (i) b[n++] = r[--i];
    }
    b[n] = 0;
}

void u_itoa(int v, char *b) {
    if (v < 0) {
        *b++ = '-';
        u_utoa((unsigned int)(-(v + 1)) + 1, b, 10);
    } else u_utoa((unsigned int)v, b, 10);
}

void *u_memcpy(void *d, const void *s, int n) {
    char *dd = d;
    const char *ss = s;
    for (int i = 0; i < n; i++) dd[i] = ss[i];
    return d;
}

void *u_memmove(void *d, const void *s, int n) {
    char *dd = d;
    const char *ss = s;
    if (dd == ss || n <= 0) return d;
    if (dd < ss) {
        for (int i = 0; i < n; i++) dd[i] = ss[i];
    } else {
        for (int i = n - 1; i >= 0; i--) dd[i] = ss[i];
    }
    return d;
}

void *u_memset(void *d, int c, int n) {
    char *dd = d;
    for (int i = 0; i < n; i++) dd[i] = (char)c;
    return d;
}

int u_memcmp(const void *a, const void *b, int n) {
    const char *x = a, *y = b;
    for (int i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

/* ---- memoria: first-fit con union, numero magico, doble-free seguro ---- */
#define UMAGIC 0x554C4942u

typedef struct ubl {
    unsigned int magic;
    int tam;
    struct ubl *sig;
    int libre;
} ubl_t;

static ubl_t *uraiz = 0;

static void *mas_memoria(int n) {
    int p = sys_memoria(n);
    return p < 0 ? 0 : (void *)p;
}

static void une_vecinos(void) {
    /* Une en ambas direcciones (la lista es LIFO: el mas nuevo, el mas
       alto, va primero; sin esto las arenas sbrk jamas se unirian). */
    int cambio = 1;
    while (cambio) {
        cambio = 0;
        ubl_t *prev = 0, *q = uraiz;
        while (q && q->sig) {
            ubl_t *s = q->sig;
            if (q->magic == UMAGIC && s->magic == UMAGIC &&
                q->libre && s->libre) {
                if ((char *)q + sizeof(ubl_t) + q->tam == (char *)s) {
                    q->tam += sizeof(ubl_t) + s->tam;
                    q->sig = s->sig;
                    cambio = 1;
                    continue;
                }
                if ((char *)s + sizeof(ubl_t) + s->tam == (char *)q) {
                    ubl_t *rest = s->sig;
                    s->tam += sizeof(ubl_t) + q->tam;
                    s->sig = rest;
                    if (prev) prev->sig = s;
                    else uraiz = s;
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

void *u_malloc(int n) {
    if (n <= 0) n = 1;
    n = (n + 3) & ~3;
    for (ubl_t *b = uraiz; b; b = b->sig) {
        if (b->magic != UMAGIC || !b->libre || b->tam < n) continue;
        if (b->tam >= n + (int)sizeof(ubl_t) + 16) {
            ubl_t *nv = (ubl_t *)((char *)b + sizeof(ubl_t) + n);
            nv->magic = UMAGIC;
            nv->tam = b->tam - n - sizeof(ubl_t);
            nv->sig = b->sig;
            nv->libre = 1;
            b->tam = n;
            b->sig = nv;
        }
        b->libre = 0;
        return (char *)b + sizeof(ubl_t);
    }
    ubl_t *b = mas_memoria(n + sizeof(ubl_t));
    if (!b) return 0;
    b->magic = UMAGIC;
    b->tam = n;
    b->sig = uraiz;
    b->libre = 0;
    uraiz = b;
    return (char *)b + sizeof(ubl_t);
}

void u_free(void *p) {
    ubl_t *b;
    if (!p) return;
    b = (ubl_t *)((char *)p - sizeof(ubl_t));
    if (b->magic != UMAGIC || b->libre) return;
    b->libre = 1;
    une_vecinos();
}

void *u_realloc(void *p, int n) {
    ubl_t *b;
    if (!p) return u_malloc(n);
    if (n <= 0) { u_free(p); return 0; }
    b = (ubl_t *)((char *)p - sizeof(ubl_t));
    if (b->magic != UMAGIC) return 0;
    n = (n + 3) & ~3;
    if (b->tam >= n) return p;
    /* crece en sitio si el vecino libre alcanza */
    {
        ubl_t *s = b->sig;
        if (s && s->magic == UMAGIC && s->libre &&
            b->tam + (int)sizeof(ubl_t) + s->tam >= n) {
            int resto = b->tam + sizeof(ubl_t) + s->tam - n;
            b->tam = n;
            b->sig = s->sig;
            if (resto >= (int)sizeof(ubl_t) + 16) {
                ubl_t *nv = (ubl_t *)((char *)b + sizeof(ubl_t) + n);
                nv->magic = UMAGIC;
                nv->tam = resto - sizeof(ubl_t);
                nv->sig = b->sig;
                nv->libre = 1;
                b->sig = nv;
            }
            return p;
        }
    }
    {
        void *np = u_malloc(n);
        if (!np) return 0;
        u_memcpy(np, p, b->tam);
        u_free(p);
        return np;
    }
}

void *u_calloc(int n, int tam) {
    int total = n * tam;
    void *p = u_malloc(total);
    if (p) u_memset(p, 0, total);
    return p;
}

void u_malloc_info(int *nbloq, int *nbytes, int *nlibre) {
    int nb = 0, by = 0, li = 0;
    for (ubl_t *b = uraiz; b; b = b->sig) {
        if (b->magic != UMAGIC) break;
        nb++;
        if (b->libre) li += b->tam;
        else by += b->tam;
    }
    if (nbloq) *nbloq = nb;
    if (nbytes) *nbytes = by;
    if (nlibre) *nlibre = li;
}

/* ---- ficheros sin bufer ---- */
#define UFDS 8
static UFILE ufds[UFDS];
static int ufs_usado[UFDS];

static UFILE *toma_slot(void) {
    for (int i = 0; i < UFDS; i++) {
        if (ufs_usado[i]) continue;
        ufs_usado[i] = 1;
        return &ufds[i];
    }
    return 0;
}

static void suelta_slot(UFILE *f) {
    for (int i = 0; i < UFDS; i++) {
        if (&ufds[i] == f) { ufs_usado[i] = 0; break; }
    }
}

UFILE *u_fopen(const char *ruta, const char *modo) {
    int esc = modo && modo[0] == 'w';
    int ap = modo && modo[0] == 'a';
    int fd = sys_abrir(ruta);
    UFILE *f;
    if (fd < 0) {
        if (!esc && !ap) return 0;
        fd = sys_crear(ruta);
        if (fd < 0) return 0;
    }
    f = toma_slot();
    if (!f) { sys_cerrar(fd); return 0; }
    f->fd = fd;
    f->tam = sys_buscar(fd, 0, BUSCAR_FIN);
    if (f->tam < 0) f->tam = 0;
    if (esc) {
        if (sys_truncar(fd, 0)) { sys_cerrar(fd); suelta_slot(f); return 0; }
        f->tam = 0;
        f->pos = 0;
    } else if (ap) {
        f->pos = f->tam;
        sys_buscar(fd, 0, BUSCAR_FIN);
    } else {
        f->pos = 0;
        sys_buscar(fd, 0, BUSCAR_INI);
    }
    return f;
}

int u_fread(void *buf, int n, UFILE *f) {
    int r;
    if (!f || n <= 0) return 0;
    if (f->pos >= f->tam) return 0;
    if (f->pos + n > f->tam) n = f->tam - f->pos;
    sys_buscar(f->fd, f->pos, BUSCAR_INI);
    r = sys_leer(f->fd, buf, n);
    if (r <= 0) return 0;
    f->pos += r;
    return r;
}

int u_fwrite(const void *buf, int n, UFILE *f) {
    int r;
    if (!f || n <= 0) return 0;
    sys_buscar(f->fd, f->pos, BUSCAR_INI);
    r = sys_escribir(f->fd, buf, n);
    if (r <= 0) return 0;
    f->pos += r;
    if (f->pos > f->tam) f->tam = f->pos;
    return r;
}

int u_fseek(UFILE *f, int off, int base) {
    int np;
    if (!f || (base != 0 && base != 1 && base != 2)) return -1;
    np = sys_buscar(f->fd, off, base);
    if (np < 0) return -1;
    f->pos = np;
    return 0;
}

int u_ftell(UFILE *f) {
    return f ? f->pos : -1;
}

int u_fsize(UFILE *f) {
    int t, p;
    if (!f) return -1;
    t = sys_buscar(f->fd, 0, BUSCAR_FIN);
    if (t < 0) return -1;
    f->tam = t;
    p = sys_buscar(f->fd, f->pos, BUSCAR_INI);
    if (p < 0) return -1;
    return t;
}

int u_fijar_tam(UFILE *f, int tam) {
    if (!f || tam < 0) return -1;
    if (sys_truncar(f->fd, tam)) return -1;
    f->tam = tam;
    if (f->pos > tam) f->pos = tam;
    return 0;
}

int u_fclose(UFILE *f) {
    if (!f) return -1;
    sys_cerrar(f->fd);
    suelta_slot(f);
    return 0;
}

int u_remove(const char *ruta) {
    return sys_borrar(ruta);
}

/* ---- salida ---- */
void u_print(const char *s) {
    sys_imprimir(s, u_strlen(s));
}

void u_print_num(int v) {
    char b[12];
    u_itoa(v, b);
    u_print(b);
}

void u_print_hex(unsigned int v) {
    char b[12];
    u_utoa(v, b, 16);
    u_print(b);
}

void u_printf(const char *fmt, ...) {
    __builtin_va_list ap;
    char sal[256];
    int n = 0;
    __builtin_va_start(ap, fmt);
    while (*fmt && n < 250) {
        if (*fmt != '%') { sal[n++] = *fmt++; continue; }
        fmt++;
        if (*fmt == 'd') {
            char b[12];
            u_itoa(__builtin_va_arg(ap, int), b);
            for (int i = 0; b[i] && n < 250; i++) sal[n++] = b[i];
        } else if (*fmt == 'u') {
            char b[12];
            u_utoa(__builtin_va_arg(ap, unsigned int), b, 10);
            for (int i = 0; b[i] && n < 250; i++) sal[n++] = b[i];
        } else if (*fmt == 'x') {
            char b[12];
            u_utoa(__builtin_va_arg(ap, unsigned int), b, 16);
            for (int i = 0; b[i] && n < 250; i++) sal[n++] = b[i];
        } else if (*fmt == 's') {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s) s = "(nulo)";
            while (*s && n < 250) sal[n++] = *s++;
        } else if (*fmt == 'c') {
            sal[n++] = (char)__builtin_va_arg(ap, int);
        } else if (*fmt == '%') {
            sal[n++] = '%';
        } else {
            sal[n++] = '%';
            if (n < 250) sal[n++] = *fmt;
        }
        if (*fmt) fmt++;
    }
    __builtin_va_end(ap);
    if (n) sys_imprimir(sal, n);
}

int u_tecla(void) {
    return sys_leer_tecla();
}

const char *u_args(void) {
    return (const char *)0x2F0000;
}

void u_lee_linea(char *d, int max) {
    int n = 0;
    for (;;) {
        int t = sys_leer_tecla();
        if (t == 13) {
            u_print("\n");
            d[n] = 0;
            return;
        } else if (t == 8) {
            if (n > 0) {
                n--;
                u_print("\b \b");
            }
        } else if (t >= 32 && t < 127) {
            if (n < max - 1) {
                d[n++] = (char)t;
                sys_imprimir((const char *)&t, 1);
            }
        }
    }
}

/* ---- graficos ---- */
void u_destino(int d) {
    sys_destino(d);
}

void u_presentar(int x, int y, int w, int h) {
    sys_presentar(x, y, w, h);
}

void u_pixel(int x, int y, int color) {
    sys_pixel(x, y, color);
}

void u_linea(int x1, int y1, int x2, int y2, int color) {
    sys_linea(x1, y1, x2, y2, color);
}

void u_rectangulo(int x, int y, int w, int h, int color) {
    sys_rectangulo(x, y, w, h, color);
}

void u_texto(int x, int y, const char *s, int color) {
    int n = u_strlen(s);
    if (n > 256) n = 256;
    sys_texto(x, y, s, n, color);
}

void u_limpiar(int color) {
    sys_limpiar_pantalla(color);
}

void u_sprite(int x, int y, const USPRITE *s) {
    if (s) sys_sprite(x, y, s);
}

void u_sprite_t(int x, int y, const USPRITE *s, unsigned int clave) {
    if (s) sys_sprite_t(x, y, s, clave);
}

void u_blit(int x0, int y0, int w, int h, int x1, int y1) {
    int op[6];
    op[0] = x0; op[1] = y0; op[2] = w; op[3] = h; op[4] = x1; op[5] = y1;
    sys_blit(op);
}

unsigned int u_punto(int x, int y) {
    return (unsigned int)sys_leer_pixel(x, y);
}

/* ---- tiempo ---- */
int u_ticks(void) {
    return sys_ticks();
}

int u_dormir(int ms) {
    return sys_dormir(ms);
}

int u_hay_tecla(void) {
    return sys_hay_tecla();
}

int u_ejecutar(const char *ruta, const char *args) {
    return sys_ejecutar(ruta, args ? args : "");
}

int u_terminal(void) {
    return sys_terminal();
}

/* ---- procesos v1.9 ------------------------------------------------- */

int u_crear(const char *nombre, const char *args) {
    return sys_crear_proceso(nombre, args ? args : "");
}

int u_mi_pid(void) { return sys_mi_pid(); }

int u_listar(uproc_t *buf, int max) {
    if (max <= 0) return 0;
    if (max > 1 + 8) max = 1 + 8;
    return sys_listar(buf, max);
}

int u_esperar(int pid) { return sys_esperar(pid); }
int u_ceder(void) { return sys_ceder(); }
int u_info_memoria(unsigned int *v5) { return sys_info_memoria(v5); }
int u_info_memoria2(unsigned int *v8) { return sys_info_memoria2(v8); }
int u_proc_stat(int pid, unsigned int *v5) {
    return sys_proc_stat(pid, v5);
}
int u_ipc_enviar(int dst, const void *ptr, int len) {
    return sys_ipc_enviar(dst, ptr, len);
}
int u_ipc_recibir(void *buf, int max, int *emisor) {
    return sys_ipc_recibir(buf, max, emisor);
}
int u_ipc_hay(void) {
    return sys_ipc_hay();
}
int u_raton(unsigned int *v4) {
    return sys_raton(v4);
}
int u_hay_raton(void) {
    return sys_hay_raton();
}
int u_dir_hijo(int dir, int n, udir_t *dst) {
    unsigned char crudo[48];
    int t, j;
    if (!dst) return -1;
    t = sys_dir_hijo(dir, n, crudo);
    if (t < 0) return -1;
    dst->tipo = (int)((unsigned)crudo[0] | ((unsigned)crudo[1] << 8) |
                      ((unsigned)crudo[2] << 16) | ((unsigned)crudo[3] << 24));
    for (j = 0; j < 32; j++) dst->nombre[j] = (char)crudo[4 + j];
    dst->nombre[32] = 0;
    dst->tam = (unsigned)crudo[40] | ((unsigned)crudo[41] << 8) |
               ((unsigned)crudo[42] << 16) | ((unsigned)crudo[43] << 24);
    dst->idx = (int)((unsigned)crudo[44] | ((unsigned)crudo[45] << 8) |
                     ((unsigned)crudo[46] << 16) | ((unsigned)crudo[47] << 24));
    return dst->tipo;
}
int u_dir_crear(const char *ruta) {
    return sys_dir_crear(ruta);
}
int u_renombrar(const char *ruta, const char *nuevo) {
    return sys_renombrar(ruta, nuevo);
}
int u_reloj(unsigned int *v6) {
    return sys_reloj(v6);
}
int u_video_modo(int w, int h) {
    return sys_video_modo(w, h);
}
int u_video_info(unsigned int *v4) {
    return sys_video_info(v4);
}
int u_teclado_mod(void) {
    int r = sys_teclado_mod();
    return r < 0 ? 0 : r;
}
void u_apagar(void) {
    sys_apagar();
}
void u_reiniciar(void) {
    sys_reiniciar();
}
/* ---- red propia (v2.3-red F1) ---- */
int u_red_interfaz(red_if_t *f) {
    return sys_red_interfaz(f);
}
int u_red_trama_enviar(const void *marco, int tam) {
    return sys_red_trama_enviar(marco, tam);
}
int u_red_trama_recibir(void *marco, int max, int espera) {
    return sys_red_trama_recibir(marco, max, espera);
}

const char *u_estado_nombre(int e) {
    switch (e) {
    case PROC_NUEVO: return "nuevo";
    case PROC_LISTO: return "listo";
    case PROC_EJECUTO: return "ejecutando";
    case PROC_BLOQ: return "bloqueado";
    case PROC_FIN: return "terminado";
    default: return "?";
    }
}
