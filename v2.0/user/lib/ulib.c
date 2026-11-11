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
    /* v2.4.3: los args viven al final de la pila (0x3A0000). */
    return (const char *)0x3A0000;
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

/* Recorte de dibujo por arrendamiento (F8-compositor): limita todos
   los primitivas graficas de este proceso al rectangulo dado
   (0,0,0,0 lo desactiva). */
void u_fija_clip(int x, int y, int w, int h) {
    sys_fija_clip(x, y, w, h);
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
int u_red_configurar_leer(red_cfg_t *c) {
    return sys_red_configurar(0, (unsigned int)c);
}
int u_red_fijar(int campo, unsigned int valor) {
    return sys_red_configurar(campo, valor);
}
int u_red_consultar(int tipo, void *ptr) {
    return sys_red_consultar(tipo, ptr);
}
int u_red_rutas(void *buf, int max) {
    return sys_red_rutas(0, buf, max);
}
int u_red_comprobar(const red_peticion_t *pet, red_respuesta_t *resp,
                    int max) {
    return sys_red_comprobar(pet, resp, max);
}
int u_red_udp_ligar(int puerto) {
    return sys_red_udp_ligar(puerto);
}
int u_red_udp_enviar(const red_udp_envio_t *e, const void *datos,
                     int len) {
    return sys_red_udp_enviar(e, datos, len);
}
int u_red_udp_recibir(const red_udp_peticion_t *pet, void *buf,
                      int max) {
    return sys_red_udp_recibir(pet, buf, max);
}

/* ---- TCP propio (v2.3-red F5; syscall 61 op-multiplexada) ---- */
int u_tcp_abrir(unsigned int ip, unsigned short puerto,
                unsigned short local) {
    red_tcp_abre_t a;
    a.ip = ip;
    a.puerto = puerto;
    a.local = local;
    return sys_red_tcp(0, &a, 0, 0);
}
int u_tcp_escuchar(unsigned short puerto) {
    red_tcp_escucha_t e;
    e.puerto = puerto;
    e.res = 0;
    return sys_red_tcp(1, &e, 0, 0);
}
int u_tcp_aceptar(int id, unsigned short espera) {
    red_tcp_acepta_t a;
    a.id = (unsigned int)id;
    a.espera = espera;
    a.res = 0;
    return sys_red_tcp(2, &a, 0, 0);
}
int u_tcp_enviar(int id, const void *datos, int len) {
    red_tcp_id_t q;
    q.id = (unsigned int)id;
    return sys_red_tcp(3, &q, (void *)datos, len);
}
int u_tcp_recibir(int id, void *buf, int max, unsigned short espera) {
    red_tcp_acepta_t a;
    a.id = (unsigned int)id;
    a.espera = espera;
    a.res = 0;
    return sys_red_tcp(4, &a, buf, max);
}
int u_tcp_cerrar(int id) {
    red_tcp_id_t q;
    q.id = (unsigned int)id;
    return sys_red_tcp(5, &q, 0, 0);
}
int u_tcp_abortar(int id) {
    red_tcp_id_t q;
    q.id = (unsigned int)id;
    return sys_red_tcp(6, &q, 0, 0);
}
int u_tcp_estado(int id) {
    red_tcp_id_t q;
    q.id = (unsigned int)id;
    return sys_red_tcp(7, &q, 0, 0);
}

/* ---- DNS A/IN (v2.3-red F4; sin sockets) ---- */

static int dns_es_ldh(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-';
}

/* Arma consulta A/IN con RD. Devuelve long o negativo (-1 args,
   -2 dominio invalido, -3 sin sitio). */
int u_dns_armar(const char *dom, unsigned short xid,
                unsigned char *sal, int max) {
    int l, i = 0, ini = 0, t = 12;
    if (!dom || !sal || max < 12 + 1 + 4 + 4) return -1;
    l = u_strlen(dom);
    if (l < 1 || l > 253) return -2;
    if (dom[l - 1] == '.') l--;   /* punto final opcional */
    if (!l) return -2;
    while (i <= l) {
        if (i == l || dom[i] == '.') {
            int n = i - ini, k;
            if (n < 1 || n > 63 || t + 1 + n + 5 > max) return -2;
            sal[t++] = (unsigned char)n;
            for (k = 0; k < n; k++) {
                if (!dns_es_ldh(dom[ini + k])) return -2;
                sal[t++] = (unsigned char)dom[ini + k];
            }
            ini = i + 1;
        }
        i++;
    }
    if (t + 5 > max) return -3;
    sal[t++] = 0;
    sal[t++] = 0;
    sal[t++] = 1;   /* A */
    sal[t++] = 0;
    sal[t++] = 1;   /* IN */
    sal[0] = (unsigned char)((xid >> 8) & 0xFFu);
    sal[1] = (unsigned char)(xid & 0xFFu);
    sal[2] = 0x01;  /* RD */
    sal[3] = 0;
    sal[4] = 0;
    sal[5] = 1;    /* QDCOUNT */
    sal[6] = 0;
    sal[7] = 0;
    sal[8] = 0;
    sal[9] = 0;
    sal[10] = 0;
    sal[11] = 0;
    return t;
}

/* Salta un nombre (etiquetas o punteros) con validacion estricta.
   Devuelve 0 y el offset siguiente en *sig, o -1. */
static int dns_salta(const unsigned char *m, int n, int o, int *sig) {
    int saltos = 0, fin = -1;
    for (;;) {
        unsigned char c;
        if (o < 0 || o >= n) return -1;
        c = m[o];
        if ((c & 0xC0) == 0xC0) {
            int d;
            if (o + 1 >= n) return -1;
            d = ((c & 0x3Fu) << 8) | m[o + 1];
            if (d >= n) return -1;
            if (d < 12) return -1;   /* F6-D: jamas a la cabecera */
            if (fin < 0) fin = o + 2;
            o = d;
            if (++saltos > 16) return -1;
            continue;
        }
        if (c & 0xC0) return -1;
        if (!c) {
            if (sig) *sig = (fin >= 0 ? fin : o + 1);
            return 0;
        }
        if (c > 63 || o + 1 + c > n) return -1;
        o += 1 + c;
    }
}

/* Expande un nombre a texto con puntos (minusculas). Devuelve
   longitud (>0) o -1. Nunca escribe mas de smax-1 + NUL. */
static int dns_expande(const unsigned char *m, int n, int o,
                       char *sal, int smax) {
    int saltos = 0, t = 0, primero = 1;
    for (;;) {
        unsigned char c;
        int k;
        if (o < 0 || o >= n) return -1;
        c = m[o];
        if ((c & 0xC0) == 0xC0) {
            int d;
            if (o + 1 >= n) return -1;
            d = ((c & 0x3Fu) << 8) | m[o + 1];
            if (d >= n || d < 12) return -1;
            o = d;
            if (++saltos > 16) return -1;
            continue;
        }
        if (c & 0xC0) return -1;
        if (!c) break;
        if (c > 63 || o + 1 + c > n) return -1;
        if (!primero) {
            if (t + 1 >= smax) return -1;
            sal[t++] = '.';
        }
        primero = 0;
        for (k = 0; k < c; k++) {
            unsigned char ch = m[o + 1 + k];
            if (t + 1 >= smax) return -1;
            sal[t++] = (char)((ch >= 'A' && ch <= 'Z') ? ch + 32 : ch);
        }
        o += 1 + c;
    }
    if (!t || t + 1 > smax) return -1;
    sal[t] = 0;
    return t;
}

/* Lee respuesta DNS. Compara xid y la pregunta con la enviada
   (preg/npreg). Extrae hasta maxips A en ips[] con ttls[].
   *rcode = codigo (0..15, -1 si ni cabecera). Solo cuenta los A
   cuyo NAME es la pregunta o un destino CNAME del mismo mensaje.
   Si hay CNAME y ningun A valido, expande el primer destino a
   cname ("" si directo) y su TTL minimo a *cttl.
   Devuelve #A o negativo (-1 malformada, -2 no es nuestra,
   -4 truncada TC). */
static char dc_cn[8][256];   /* F6-D: destinos CNAME del mensaje */
int u_dns_leer2(const unsigned char *m, int n, unsigned short xid,
                const unsigned char *preg, int npreg,
                unsigned int *ips, unsigned int *ttls, int maxips,
                int *rcode, char *cname, int cnmax,
                unsigned int *cttl) {
    int band, qd, an, ns, ar, o, qfin, i, hall = 0, ncn = 0;
    char qnom[256];
    unsigned int cn_ttl = 0;
    int k;
    if (cname && cnmax > 0) cname[0] = 0;
    if (cttl) *cttl = 0;
    if (rcode) *rcode = -1;
    if (!m || n < 12 || !preg || npreg < 12 + 1 + 4 || !ips ||
        maxips < 1)
        return -1;
    if (((m[0] << 8) | m[1]) != xid) return -2;
    band = (m[2] << 8) | m[3];
    if (!(band & 0x8000)) return -1;     /* debe ser respuesta */
    if ((band & 0x7800)) return -1;      /* opcode 0 */
    if ((band & 0x0070)) return -1;      /* F6-D: Z debe ser 0 */
    if (rcode) *rcode = band & 15;
    if (band & 0x0200) return -4;        /* truncada (TCP aparte) */
    qd = (m[4] << 8) | m[5];
    an = (m[6] << 8) | m[7];
    ns = (m[8] << 8) | m[9];
    ar = (m[10] << 8) | m[11];
    if (qd != 1) return -1;
    if (dns_salta(m, n, 12, &qfin)) return -1;
    if (qfin + 4 > n) return -1;
    /* La pregunta debe ser eco exacto de la enviada (nombre+tipo+clase). */
    if (qfin + 4 - 12 != npreg - 12) return -2;
    for (k = 0; k + 12 < npreg; k++)
        if (m[12 + k] != preg[12 + k]) return -2;
    if (dns_expande(m, n, 12, qnom, (int)sizeof(qnom)) < 0)
        return -1;
    o = qfin + 4;
    /* Pasada 1: destinos CNAME (hasta 8; el resto solo se enmarca). */
    {
        int p = o, j;
        for (j = 0; j < an; j++) {
            int tipo, clase, rdlen, sig;
            unsigned int ttl;
            if (dns_salta(m, n, p, &p)) return -1;
            if (p + 10 > n) return -1;
            tipo = (m[p] << 8) | m[p + 1];
            clase = (m[p + 2] << 8) | m[p + 3];
            ttl = ((unsigned int)m[p + 4] << 24) |
                  ((unsigned int)m[p + 5] << 16) |
                  ((unsigned int)m[p + 6] << 8) | m[p + 7];
            rdlen = (m[p + 8] << 8) | m[p + 9];
            p += 10;
            if (p + rdlen > n) return -1;
            if (tipo == 5 && clase == 1 && ncn < 8) {
                char tgt[256];
                if (!dns_salta(m, n, p, &sig) && sig <= p + rdlen &&
                    dns_expande(m, n, p, tgt,
                                (int)sizeof(tgt)) > 0) {
                    int q;
                    for (q = 0; tgt[q] && q < 255; q++)
                        dc_cn[ncn][q] = tgt[q];
                    dc_cn[ncn][q] = 0;
                    if (!cn_ttl || ttl < cn_ttl) cn_ttl = ttl;
                    ncn++;
                }
            }
            p += rdlen;
        }
    }
    /* Pasada 2: A cuyo NAME es la pregunta o un destino CNAME. */
    for (i = 0; i < an; i++) {
        int tipo, clase, rdlen, onom = o;
        unsigned int ttl;
        char tnom[256];
        int j, vale = 0;
        if (dns_salta(m, n, o, &o)) return -1;
        if (o + 10 > n) return -1;
        tipo = (m[o] << 8) | m[o + 1];
        clase = (m[o + 2] << 8) | m[o + 3];
        ttl = ((unsigned int)m[o + 4] << 24) |
              ((unsigned int)m[o + 5] << 16) |
              ((unsigned int)m[o + 6] << 8) | m[o + 7];
        rdlen = (m[o + 8] << 8) | m[o + 9];
        o += 10;
        if (o + rdlen > n) return -1;
        if (tipo == 1 && clase == 1 && rdlen == 4) {
            if (dns_expande(m, n, onom, tnom,
                            (int)sizeof(tnom)) > 0) {
                if (!u_strcmp(tnom, qnom)) vale = 1;
                else {
                    for (j = 0; j < ncn; j++) {
                        if (!u_strcmp(tnom, dc_cn[j])) {
                            vale = 1;
                            break;
                        }
                    }
                }
            }
            if (vale && hall < maxips) {
                ips[hall] = (unsigned int)m[o] |
                            ((unsigned int)m[o + 1] << 8) |
                            ((unsigned int)m[o + 2] << 16) |
                            ((unsigned int)m[o + 3] << 24);
                if (ttls) ttls[hall] = ttl;
                hall++;
            }
        }
        o += rdlen;
    }
    for (i = 0; i < ns + ar; i++) {
        int rdlen;
        if (dns_salta(m, n, o, &o)) return -1;
        if (o + 10 > n) return -1;
        rdlen = (m[o + 8] << 8) | m[o + 9];
        o += 10;
        if (o + rdlen > n) return -1;
        o += rdlen;
    }
    if (o != n) return -1;   /* F6-D: sin basura final */
    if (ncn && cname && cnmax > 0) {
        int q;
        for (q = 0; dc_cn[0][q] && q + 1 < cnmax; q++)
            cname[q] = dc_cn[0][q];
        cname[q] = 0;
    }
    if (cttl) *cttl = cn_ttl;
    return hall;
}

int u_dns_leer(const unsigned char *m, int n, unsigned short xid,
               const unsigned char *preg, int npreg,
               unsigned int *ips, unsigned int *ttls, int maxips,
               int *rcode) {
    return u_dns_leer2(m, n, xid, preg, npreg, ips, ttls, maxips,
                       rcode, 0, 0, 0);
}

/* ---- F6-D: cache + resolucion completa (Ring3) ---- */

#define DNS_CMAX 32          /* entradas */
#define DNS_CNAM 128         /* nombre cacheable (mas largo: sin cache) */
#define DNS_TTL_MAX 86400u   /* tope de TTL almacenable (1 dia) */
#define DNS_TCP_MAX 4096     /* mensaje DNS/TCP maximo operativo */
#define DNS_SALTOS 8         /* CNAME seguidos como maximo */

static struct {
    char nom[DNS_CNAM];
    unsigned int ip[8];
    int n;
    unsigned int ttl;    /* segundos efectivos */
    unsigned int ins;    /* tick de insercion */
    unsigned int srv;
    unsigned short pto;
    int valido;
} dc_cache[DNS_CMAX];
static unsigned int dc_vict = 0;

static unsigned int dns_ahora(void) {
    int t = u_ticks();
    return t < 0 ? 0u : (unsigned int)t;
}

/* Predicado puro (testeable): entrada valida en el instante dado. */
int u_dns_cache_viva(unsigned int ins, unsigned int ttl,
                     unsigned int ahora) {
    int d;
    if (!ttl || ttl > DNS_TTL_MAX) return 0;
    d = (int)(ahora - ins);
    if (d < 0) return 0;
    return (unsigned int)d < ttl * 100u;
}

static void dns_min(char *d, const char *s) {
    int i = 0;
    while (s[i] && i + 1 < DNS_CNAM) {
        char c = s[i];
        d[i++] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    d[i] = 0;
}

static int dns_cache_halla(const char *nom, unsigned int srv,
                           unsigned short pto, unsigned int *ip,
                           int *n, unsigned int *ttl) {
    char k[DNS_CNAM];
    unsigned int ahora = dns_ahora();
    int i, l = u_strlen(nom);
    if (l < 1 || l >= DNS_CNAM) return 0;
    dns_min(k, nom);
    for (i = 0; i < DNS_CMAX; i++) {
        if (!dc_cache[i].valido) continue;
        if (dc_cache[i].srv != srv || dc_cache[i].pto != pto)
            continue;
        if (u_strcmp(dc_cache[i].nom, k)) continue;
        if (!u_dns_cache_viva(dc_cache[i].ins, dc_cache[i].ttl,
                              ahora)) {
            dc_cache[i].valido = 0;
            continue;
        }
        for (l = 0; l < dc_cache[i].n && l < 8; l++)
            ip[l] = dc_cache[i].ip[l];
        *n = dc_cache[i].n;
        *ttl = dc_cache[i].ttl;
        return 1;
    }
    return 0;
}

static void dns_cache_pon(const char *nom, unsigned int srv,
                          unsigned short pto, const unsigned int *ip,
                          int n, unsigned int ttl) {
    char k[DNS_CNAM];
    unsigned int ahora = dns_ahora();
    int i, v = -1, l;
    if (!ttl) return;   /* TTL 0: no cacheable */
    if (ttl > DNS_TTL_MAX) ttl = DNS_TTL_MAX;
    if (u_strlen(nom) < 1 || u_strlen(nom) >= DNS_CNAM) return;
    if (n < 1) return;
    if (n > 8) n = 8;
    dns_min(k, nom);
    for (i = 0; i < DNS_CMAX; i++) {
        if (!dc_cache[i].valido) {
            v = i;
            break;
        }
    }
    if (v < 0) {
        for (i = 0; i < DNS_CMAX; i++) {
            if (!u_dns_cache_viva(dc_cache[i].ins, dc_cache[i].ttl,
                                  ahora)) {
                dc_cache[i].valido = 0;
                v = i;
                break;
            }
        }
    }
    if (v < 0) {
        v = (int)(dc_vict % DNS_CMAX);
        dc_vict++;
    }
    for (l = 0; k[l]; l++) dc_cache[v].nom[l] = k[l];
    dc_cache[v].nom[l] = 0;
    for (l = 0; l < n; l++) dc_cache[v].ip[l] = ip[l];
    dc_cache[v].n = n;
    dc_cache[v].ttl = ttl;
    dc_cache[v].ins = ahora;
    dc_cache[v].srv = srv;
    dc_cache[v].pto = pto;
    dc_cache[v].valido = 1;
}

void u_dns_cache_limpiar(void) {
    int i;
    for (i = 0; i < DNS_CMAX; i++) dc_cache[i].valido = 0;
    dc_vict = 0;
}

int u_dns_cache_n(void) {
    unsigned int ahora = dns_ahora();
    int i, c = 0;
    for (i = 0; i < DNS_CMAX; i++) {
        if (!dc_cache[i].valido) continue;
        if (!u_dns_cache_viva(dc_cache[i].ins, dc_cache[i].ttl,
                              ahora)) {
            dc_cache[i].valido = 0;
            continue;
        }
        c++;
    }
    return c;
}

/* Intercambio DNS sobre TCP (prefijo BE16, lecturas parciales).
   Devuelve long del mensaje o negativo. Libera el TCB siempre. */
static unsigned char dns_tm[DNS_TCP_MAX];
static int dns_tcp_xchg(unsigned int srv, unsigned short pto,
                        const unsigned char *q, int qlen,
                        unsigned int fin) {
    unsigned char pre[2 + 300];
    int id, got = 0, len, r;
    if (qlen < 12 || qlen > 300) return -1;
    id = u_tcp_abrir(srv, pto, 0);
    if (id <= 0) return -1;
    pre[0] = (unsigned char)((qlen >> 8) & 0xFFu);
    pre[1] = (unsigned char)(qlen & 0xFFu);
    {
        int k;
        for (k = 0; k < qlen; k++) pre[2 + k] = q[k];
    }
    /* La consulta cabe en un segmento (<=302 B); enviar es atomico. */
    r = u_tcp_enviar(id, pre, 2 + qlen);
    if (r != 2 + qlen) {
        u_tcp_abortar(id);
        return -1;
    }
    {
        unsigned char lp[2];
        int g = 0;
        while (g < 2) {
            if ((int)(dns_ahora() - fin) >= 0) {
                u_tcp_abortar(id);
                return -1;
            }
            r = u_tcp_recibir(id, lp + g, 2 - g, 500);
            if (r < 0) {
                u_tcp_abortar(id);
                return -1;
            }
            if (!r) continue;
            g += r;
        }
        len = (lp[0] << 8) | lp[1];
    }
    if (len < 12 || len > DNS_TCP_MAX) {
        u_tcp_abortar(id);
        return -1;
    }
    while (got < len) {
        int mx = len - got;
        if (mx > 2048) mx = 2048;
        if ((int)(dns_ahora() - fin) >= 0) {
            u_tcp_abortar(id);
            return -1;
        }
        r = u_tcp_recibir(id, dns_tm + got, mx, 500);
        if (r < 0) {
            u_tcp_abortar(id);
            return -1;
        }
        if (!r) continue;
        got += r;
    }
    u_tcp_abortar(id);   /* request/response: RST tras lo completo */
    return len;
}

/* Resolucion completa: cache + UDP (3 intentos) + CNAME (<=8) +
   TCP si TC. Sin recursion; todo acotado. */
static int dns_sport = 0;   /* F6-D: una liga por proceso */
static unsigned char dns_q[300];
static unsigned char dns_ru[8 + 512];
static char dns_vis[DNS_SALTOS][256];

int u_dns_resuelve3(const char *dom, unsigned int servidor,
                    unsigned short puerto, u_dns_res_t *r,
                    unsigned int espera_ms) {
    char actual[256];
    unsigned int fin, srv = servidor, eff = 0xFFFFFFFFu;
    unsigned short pto = puerto;
    int i, q, nv = 0, saltos = 0, via_tcp = 0;
    red_cfg_t cfg;
    if (!dom || !r) return -1;
    if (!puerto) pto = 53;
    if (!espera_ms) espera_ms = 30000;
    if (espera_ms > 60000) espera_ms = 60000;
    if (!srv) {
        int j;
        for (j = 0; j < 16; j++) ((unsigned char *)&cfg)[j] = 0;
        if (u_red_configurar_leer(&cfg)) return -5;
        if (!cfg.dns) return -5;
        srv = cfg.dns;
    }
    for (i = 0; dom[i] && i < 255; i++) actual[i] = dom[i];
    actual[i] = 0;
    if (!i) return -2;
    r->ip = 0;
    r->ttl = 0;
    r->rc = -1;
    r->rcode = -1;
    r->nrec = 0;
    r->saltos = 0;
    r->via_tcp = 0;
    r->de_cache = 0;
    r->cname[0] = 0;
    fin = dns_ahora() + espera_ms / 10u;
    for (q = 0; q < 1 + DNS_SALTOS; q++) {
        unsigned int ips[8], ttls[8], cttl = 0;
        char cn[256];
        int intento, k, rc = -1;
        if ((int)(dns_ahora() - fin) >= 0) return -5;
        /* 1. Cache (nombre + servidor + puerto). */
        {
            unsigned int cip[8];
            int cn2 = 0;
            unsigned int cttl2 = 0;
            if (dns_cache_halla(actual, srv, pto, cip, &cn2,
                                &cttl2) &&
                cn2 > 0) {
                r->ip = cip[0];
                r->ttl = cttl2;
                r->rc = 0;
                r->rcode = 0;
                r->nrec = cn2;
                r->saltos = saltos;
                r->via_tcp = via_tcp;
                r->de_cache = 1;
                return 0;
            }
        }
        /* 2. UDP: 3 intentos con XID nuevo (ventana 3 s). */
        {
            int sp = dns_sport;
            if (sp <= 0) {
                sp = u_red_udp_ligar(0);
                if (sp <= 0) return -5;   /* tabla UDP llena */
                dns_sport = sp;
            }
        }
        k = -5;
        for (intento = 0; intento < 3; intento++) {
            unsigned short xid;
            int npreg;
            unsigned int fat;
            red_udp_envio_t e;
            red_udp_peticion_t pet;
            if ((int)(dns_ahora() - fin) >= 0) return -5;
            xid = (unsigned short)(((unsigned int)u_mi_pid() *
                                    7919u + dns_ahora() +
                                    (unsigned int)intento * 131u) &
                                   0xFFFFu);
            npreg = u_dns_armar(actual, xid, dns_q,
                                (int)sizeof(dns_q));
            if (npreg == -2) return -2;
            if (npreg < 0) return -5;
            e.ip = srv;
            e.dpuerto = pto;
            e.spuerto = (unsigned short)dns_sport;
            if (u_red_udp_enviar(&e, dns_q, npreg) < 0) return -5;
            {
                int rest = (int)(fin - dns_ahora());
                fat = dns_ahora() +
                      (rest < 0 ? 0u
                               : ((unsigned int)rest < 300u
                                      ? (unsigned int)rest
                                      : 300u));
            }
            for (;;) {
                int rr;
                if ((int)(dns_ahora() - fat) >= 0) break;
                if ((int)(dns_ahora() - fin) >= 0) return -5;
                pet.puerto = dns_sport;
                pet.espera = 100;
                rr = u_red_udp_recibir(&pet, dns_ru,
                                       (int)sizeof(dns_ru));
                if (rr < 0) return -5;
                if (!rr) continue;
                cn[0] = 0;
                k = u_dns_leer2(dns_ru + 8, rr - 8, xid, dns_q,
                                npreg, ips, ttls, 8, &rc, cn,
                                (int)sizeof(cn), &cttl);
                if (k == -2 || k == -1) continue; /* ajeno/ruido */
                break;
            }
            if (k == -2 || k == -1) continue;   /* siguiente XID */
            break;
        }
        if (k == -5 || k == -1 || k == -2) return -5;
        if (k == -4) {
            /* 3. TC: fallback TCP (un intento). */
            int npreg2 = u_dns_armar(actual, 0x1234, dns_q,
                                     (int)sizeof(dns_q));
            int ml;
            if (npreg2 < 0) return -2;
            via_tcp = 1;
            ml = dns_tcp_xchg(srv, pto, dns_q, npreg2, fin);
            if (ml < 0) return -6;
            cn[0] = 0;
            cttl = 0;
            k = u_dns_leer2(dns_tm, ml, 0x1234, dns_q, npreg2,
                            ips, ttls, 8, &rc, cn, (int)sizeof(cn),
                            &cttl);
            if (k == -4 || k == -2 || k == -1) return -6;
        }
        r->rcode = rc;
        if (rc == 3) {
            r->rc = 3;
            return -3;   /* NXDOMAIN: no se reintenta ni cachea */
        }
        if (rc) {
            r->rc = 4;
            return -4;   /* SERVFAIL/REFUSED/otro */
        }
        if (k > 0) {
            unsigned int tmin = ttls[0];
            int j;
            for (j = 1; j < k; j++)
                if (ttls[j] < tmin) tmin = ttls[j];
            if (cttl && cttl < tmin) tmin = cttl;
            if (tmin < eff) eff = tmin;
            if (eff != 0xFFFFFFFFu && eff > DNS_TTL_MAX)
                eff = DNS_TTL_MAX;
            if (eff) {
                dns_cache_pon(actual, srv, pto, ips,
                              k < 8 ? k : 8, eff);
            }
            if (!r->cname[0] && cn[0]) {
                int s;
                for (s = 0; cn[s] && s < 255; s++) r->cname[s] = cn[s];
                r->cname[s] = 0;
            }
            r->ip = ips[0];
            r->ttl = eff == 0xFFFFFFFFu ? 0 : eff;
            r->rc = 0;
            r->nrec = k;
            r->saltos = saltos;
            r->via_tcp = via_tcp;
            r->de_cache = 0;
            return 0;
        }
        /* k == 0: sin A. */
        if (!cn[0]) {
            r->rc = 4;
            return -8;   /* NODATA */
        }
        {
            int v;
            for (v = 0; v < nv; v++) {
                int a = 0, b = 0, ig = 1;
                while (cn[a] && dns_vis[v][b]) {
                    char ca = cn[a], cb = dns_vis[v][b];
                    if (ca >= 'A' && ca <= 'Z') ca += 32;
                    if (cb >= 'A' && cb <= 'Z') cb += 32;
                    if (ca != cb) {
                        ig = 0;
                        break;
                    }
                    a++;
                    b++;
                }
                if (ig && !cn[a] && !dns_vis[v][b]) return -7;
            }
            if (nv >= DNS_SALTOS) return -7;
            for (v = 0; cn[v] && v < 255; v++)
                dns_vis[nv][v] = cn[v];
            dns_vis[nv][v] = 0;
            nv++;
        }
        if (saltos >= DNS_SALTOS) return -7;
        saltos++;
        /* Cadena visible "a -> b -> c" (acotada a 255). */
        {
            int p = 0, s;
            while (r->cname[p]) p++;
            if (p) {
                const char *sep = " -> ";
                for (s = 0; sep[s] && p + 1 < 255; s++)
                    r->cname[p++] = sep[s];
            } else {
                for (s = 0; actual[s] && p + 1 < 255; s++)
                    r->cname[p++] = actual[s];
                for (s = 0; s < 4 && p + 1 < 255; s++)
                    r->cname[p++] = " -> "[s];
            }
            for (s = 0; cn[s] && p + 1 < 255; s++)
                r->cname[p++] = cn[s];
            r->cname[p] = 0;
        }
        if (cttl && cttl < eff) eff = cttl;
        for (i = 0; cn[i] && i < 255; i++) actual[i] = cn[i];
        actual[i] = 0;
        if (u_dns_armar(actual, 0, dns_q, (int)sizeof(dns_q)) < 0)
            return -2;   /* destino no consultable */
    }
    return -7;
}

int u_dns_resuelve(const char *dom, unsigned int servidor,
                   unsigned int *ip, unsigned int *ttl) {
    u_dns_res_t r;
    int k = u_dns_resuelve3(dom, servidor, 53, &r, 15000);
    if (k) return k;
    if (ip) *ip = r.ip;
    if (ttl) *ttl = r.ttl;
    return 0;
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

/* ---- F7-B: HTTP/1.0 Ring3 (sin sockets; GET + Connection: close) ----
   Parser incremental y reutilizable (patron del codec DNS F6-D).
   La app llama u_http_res_inicia() una vez, u_http_res_datos()
   por cada trozo recibido y u_http_res_fin() al ver EOF (-12).
   Todo acotado: sin malloc, sin recursion, sin esperas propias
   (los timeouts los pone quien llama). */

static unsigned char http_hbuf[U_HTTP_MAX_SEC];
static int http_hlen;
static char http_pet[U_HTTP_MAX_LIN * 4];

/* Adelantada: u_http_pide la usa para validar extra. */
static int http_cab_ok(const unsigned char *l, int n, char *nom,
                       char *val, int *es_te);

static int http_es_dig(char c) {
    return c >= '0' && c <= '9';
}

static int http_es_host(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '.';
}

static unsigned int http_ahora(void) {
    int t = u_ticks();
    return t < 0 ? 0u : (unsigned int)t;
}

static int http_iguales(const char *a, const char *b) {
    int i = 0;
    for (;;) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return (int)ca - (int)cb;
        if (!ca) return 0;
        i++;
    }
}

/* Decimal estricto sin espacios; tope 4294967295. 0 ok, -1 mal. */
static int http_num(const char *s, unsigned int *v) {
    int i = 0;
    unsigned int n = 0;
    if (!s[0]) return -1;
    while (s[i]) {
        unsigned int d;
        if (s[i] < '0' || s[i] > '9') return -1;
        d = (unsigned int)(s[i] - '0');
        if (n > 429496729u || (n == 429496729u && d > 5u))
            return -1;
        n = n * 10u + d;
        i++;
    }
    if (v) *v = n;
    return 0;
}

/* IPv4 punteada estricta (sin ceros a la izquierda salvo "0").
   u32 LE: primer octeto en el byte bajo (como red_cfg_t). */
static int http_ip(const char *s, unsigned int *ip) {
    int i = 0, k;
    unsigned int v = 0;
    for (k = 0; k < 4; k++) {
        int n = 0, d = 0, ini;
        if (k && s[i++] != '.') return -1;
        ini = i;
        while (s[i] >= '0' && s[i] <= '9') {
            n = n * 10 + (s[i] - '0');
            d++;
            i++;
            if (n > 255 || d > 3) return -1;
        }
        if (!d) return -1;
        if (d > 1 && s[ini] == '0') return -1;
        v |= (unsigned int)n << (k * 8);
    }
    if (s[i]) return -1;
    if (ip) *ip = v;
    return 0;
}

int u_http_url(const char *url, u_http_url_t *u) {
    int i = 0, h = 0, k, soloip = 1, puntos = 0;
    if (!url || !u) return -1;
    while (url[i]) {
        unsigned char c = (unsigned char)url[i];
        if (c < 32 || c == 127 || c == ' ') return -5;
        i++;
        if (i > U_HTTP_MAX_URL) return -5;
    }
    if (i < 7) return -2;
    {
        const char *esq = "http://";
        for (k = 0; k < 7; k++) {
            char a = url[k];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (a != esq[k]) return -2;
        }
    }
    i = 7;
    while (url[i] && url[i] != '/' && url[i] != ':') {
        if (!http_es_host(url[i])) return -3;
        if (h + 1 >= (int)sizeof(u->host)) return -5;
        {
            char c = url[i];
            u->host[h++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32)
                                                  : c;
        }
        i++;
    }
    if (!h) return -3;
    u->host[h] = 0;
    if (u->host[0] == '.' || u->host[0] == '-' ||
        u->host[h - 1] == '.' || u->host[h - 1] == '-') return -3;
    for (k = 0; k < h; k++) {
        char c = u->host[k];
        if (c == '.') {
            puntos++;
            if (k + 1 < h && u->host[k + 1] == '.') return -3;
        } else if (c < '0' || c > '9') soloip = 0;
    }
    u->es_ip = 0;
    u->ip = 0;
    if (soloip && puntos) {
        if (http_ip(u->host, &u->ip)) return -3;
        u->es_ip = 1;
    }
    u->puerto = 80;
    if (url[i] == ':') {
        unsigned int po = 0;
        int nd = 0;
        i++;
        while (url[i] >= '0' && url[i] <= '9') {
            po = po * 10u + (unsigned int)(url[i] - '0');
            if (po > 65535u) return -4;
            nd++;
            i++;
            if (nd > 5) return -4;
        }
        if (!nd || !po || po > 65535u) return -4;
        u->puerto = (unsigned short)po;
    }
    {
        int r = 0;
        if (!url[i]) {
            u->ruta[0] = '/';
            u->ruta[1] = 0;
        } else {
            if (url[i] != '/') return -5;
            while (url[i] && url[i] != '#') {
                if (r + 1 >= (int)sizeof(u->ruta)) return -5;
                u->ruta[r++] = url[i++];
            }
            u->ruta[r] = 0;
        }
    }
    return 0;
}

static int http_pon(char *sal, int smax, int *t, const char *s) {
    int k = 0;
    while (s[k]) {
        if (*t + 1 >= smax) return -1;
        sal[(*t)++] = s[k++];
    }
    return 0;
}

/* Constructor comun GET/POST (post=0 GET, 1 POST). npost solo vale
   en POST y viaja como Content-Length (el cuerpo se envia aparte). */
static int http_pide_x(int post, const u_http_url_t *u,
                       const char *extra, int npost, char *sal,
                       int smax) {
    int t = 0, k;
    if (!u || !sal || smax <= 0) return -1;
    if (!u->host[0] || !u->puerto || u->ruta[0] != '/') return -2;
    if (npost < 0) return -1;
    if (http_pon(sal, smax, &t, post ? "POST " : "GET ")) return -3;
    for (k = 0; u->ruta[k]; k++) {
        if (t + 1 >= smax) return -3;
        sal[t++] = u->ruta[k];
    }
    if (http_pon(sal, smax, &t, " HTTP/1.0\r\nHost: ")) return -3;
    for (k = 0; u->host[k]; k++) {
        if (t + 1 >= smax) return -3;
        sal[t++] = u->host[k];
    }
    if (u->puerto != 80) {
        int v = u->puerto, q = 0;
        char tmp[8];
        if (t + 1 >= smax) return -3;
        sal[t++] = ':';
        if (!v) tmp[q++] = '0';
        while (v && q < 7) {
            tmp[q++] = (char)('0' + v % 10);
            v /= 10;
        }
        while (q > 0) {
            q--;
            if (t + 1 >= smax) return -3;
            sal[t++] = tmp[q];
        }
    }
    if (http_pon(sal, smax, &t, "\r\nConnection: close\r\n"
                                "User-Agent: FacilOS32-HTTP/0.1\r\n"
                                "Accept: */*\r\n"))
        return -3;
    if (post) {
        /* POST con cuerpo urlencoded (formularios): longitud exacta,
           sin chunked (el servidor cierra tras responder). */
        int v = npost, q = 0;
        char tmp[12];
        if (http_pon(sal, smax, &t,
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: "))
            return -3;
        if (!v) tmp[q++] = '0';
        while (v && q < 11) {
            tmp[q++] = (char)('0' + v % 10);
            v /= 10;
        }
        if (v || !q) return -3;
        while (q > 0) {
            q--;
            if (t + 1 >= smax) return -3;
            sal[t++] = tmp[q];
        }
        if (http_pon(sal, smax, &t, "\r\n")) return -3;
    }
    if (extra && extra[0]) {
        /* Cabeceras adicionales validadas (sin framing/control). */
        int o = 0, nl = 0;
        char nm[U_HTTP_NOM], vl[U_HTTP_VAL];
        while (extra[o]) {
            int eol = -1, j, te = 0;
            for (j = o; extra[j]; j++) {
                if (extra[j] == '\r' && extra[j + 1] == '\n') {
                    eol = j;
                    break;
                }
                if (extra[j] == '\n') return -2;
            }
            if (eol < 0) return -2;
            if (eol - o == 0) break;
            if (eol - o > U_HTTP_MAX_LIN) return -2;
            {
                unsigned char ln[U_HTTP_MAX_LIN + 2];
                for (j = 0; j < eol - o; j++)
                    ln[j] = (unsigned char)extra[o + j];
                if (http_cab_ok(ln, eol - o, nm, vl, &te))
                    return -2;
                if (!http_iguales(nm, "host") ||
                    !http_iguales(nm, "connection") ||
                    !http_iguales(nm, "content-length") ||
                    !http_iguales(nm, "transfer-encoding"))
                    return -2;
            }
            nl++;
            if (nl > 8) return -2;
            for (j = o; j < eol + 2; j++) {
                if (t + 1 >= smax) return -3;
                sal[t++] = extra[j];
            }
            o = eol + 2;
        }
    }
    if (http_pon(sal, smax, &t, "\r\n")) return -3;
    if (t > U_HTTP_MAX_LIN * 4) return -3;
    if (t >= smax) return -3;
    sal[t] = 0;
    return t;
}

int u_http_pide(const u_http_url_t *u, const char *extra, char *sal,
                int smax) {
    return http_pide_x(0, u, extra, 0, sal, smax);
}

int u_http_pide_post(const u_http_url_t *u, const char *extra, int npost,
                     char *sal, int smax) {
    if (npost < 0 || npost > U_HTTP_MAX_CPO) return -1;
    return http_pide_x(1, u, extra, npost, sal, smax);
}

/* Valida "Nombre: valor". 0 ok; 1 valida pero sin sitio para el
   valor (el llamador la ignora); -1 malformada. */
static int http_cab_ok(const unsigned char *l, int n, char *nom,
                       char *val, int *es_te) {
    int i, cpos = -1, v0, v1;
    for (i = 0; i < n; i++) {
        unsigned char c = l[i];
        if (c == ':') {
            cpos = i;
            break;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '-'))
            return -1;
    }
    if (cpos <= 0 || cpos >= U_HTTP_NOM) return -1;
    for (i = 0; i < cpos; i++) nom[i] = (char)l[i];
    nom[cpos] = 0;
    v0 = cpos + 1;
    while (v0 < n && (l[v0] == ' ' || l[v0] == '\t')) v0++;
    v1 = n;
    while (v1 > v0 && (l[v1 - 1] == ' ' || l[v1 - 1] == '\t')) v1--;
    if (v1 - v0 >= U_HTTP_VAL) {
        /* strcmp-estilo: != 0 = distinto. Solo se ignora si NO es
           framing; content-length/transfer-encoding largas = error. */
        if (http_iguales(nom, "content-length") &&
            http_iguales(nom, "transfer-encoding")) {
            *es_te = 0;
            return 1;
        }
        return -1;
    }
    for (i = v0; i < v1; i++) {
        if (l[i] < 32 || l[i] > 126) return -1;
        val[i - v0] = (char)l[i];
    }
    val[v1 - v0] = 0;
    *es_te = http_iguales(nom, "transfer-encoding") == 0;
    return 0;
}

static int http_linea_estado(u_http_res_t *r, const unsigned char *l,
                             int n) {
    int i = 0, k, may = 0, men = 0, nd = 0, code = 0;
    if (n < 12) return -1;
    if (l[0] != 'H' || l[1] != 'T' || l[2] != 'T' || l[3] != 'P' ||
        l[4] != '/')
        return -1;
    i = 5;
    while (i < n && l[i] >= '0' && l[i] <= '9') {
        may = may * 10 + (l[i] - '0');
        if (may > 99) return -1;
        nd++;
        i++;
    }
    if (!nd || i >= n || l[i] != '.') return -1;
    i++;
    nd = 0;
    while (i < n && l[i] >= '0' && l[i] <= '9') {
        men = men * 10 + (l[i] - '0');
        if (men > 99) return -1;
        nd++;
        i++;
    }
    if (!nd) return -1;
    if (!(may == 1 && (men == 0 || men == 1))) return -1;
    r->version[0] = 'H';
    r->version[1] = 'T';
    r->version[2] = 'T';
    r->version[3] = 'P';
    r->version[4] = '/';
    r->version[5] = (char)('0' + may);
    r->version[6] = '.';
    r->version[7] = (char)('0' + men);
    r->version[8] = 0;
    if (i >= n || l[i] != ' ') return -1;
    while (i < n && l[i] == ' ') i++;
    if (i + 2 >= n) return -1;
    if (!http_es_dig((char)l[i]) || !http_es_dig((char)l[i + 1]) ||
        !http_es_dig((char)l[i + 2]))
        return -1;
    code = (l[i] - '0') * 100 + (l[i + 1] - '0') * 10 +
           (l[i + 2] - '0');
    if (code < 100 || code > 599) return -1;
    r->codigo = code;
    i += 3;
    if (i < n && l[i] != ' ') return -1;
    if (i < n) i++;
    for (k = i; k < n; k++)
        if (l[k] < 32 || l[k] > 126) return -1;
    if (n - i >= (int)sizeof(r->motivo)) return -1;
    for (k = i; k < n; k++) r->motivo[k - i] = (char)l[k];
    r->motivo[n - i] = 0;
    return 0;
}

static int http_cab_guarda(u_http_res_t *r, const char *nom,
                           const char *val) {
    int k;
    if (r->ncab >= U_HTTP_MAX_CAB) return -1;
    if (!http_iguales(nom, "content-length")) {
        unsigned int v;
        if (http_num(val, &v)) return -1;
        if (r->tiene_cl) {
            if (v != r->content_length) return -1;
        } else {
            r->content_length = v;
            r->tiene_cl = 1;
        }
    }
    if (!http_iguales(nom, "transfer-encoding")) r->chunked = 1;
    for (k = 0; nom[k] && k + 1 < U_HTTP_NOM; k++)
        r->hnom[r->ncab][k] = nom[k];
    r->hnom[r->ncab][k] = 0;
    for (k = 0; val[k] && k + 1 < U_HTTP_VAL; k++)
        r->hval[r->ncab][k] = val[k];
    r->hval[r->ncab][k] = 0;
    r->ncab++;
    return 0;
}

static int http_cabeceras(u_http_res_t *r, int fin, int capcuerpo) {
    int o = 0, eol = -1, i;
    char nom[U_HTTP_NOM], val[U_HTTP_VAL];
    for (i = 0; i + 1 < fin; i++) {
        if (http_hbuf[i] == '\r' && http_hbuf[i + 1] == '\n') {
            eol = i;
            break;
        }
    }
    if (eol < 0 || eol > U_HTTP_MAX_LIN) return -1;
    if (http_linea_estado(r, http_hbuf, eol)) return -1;
    o = eol + 2;
    while (o < fin) {
        int e2 = -1, te = 0;
        for (i = o; i + 1 < fin; i++) {
            if (http_hbuf[i] == '\r' && http_hbuf[i + 1] == '\n') {
                e2 = i;
                break;
            }
        }
        if (e2 < 0) return -1;
        if (e2 == o) {
            o += 2;
            break;
        }
        if (http_hbuf[o] == ' ' || http_hbuf[o] == '\t') return -1;
        if (e2 - o > U_HTTP_MAX_LIN) return -1;
        {
            int ck = http_cab_ok(http_hbuf + o, e2 - o, nom, val, &te);
            if (ck < 0) return -1;
            if (te) r->chunked = 1;
            if (!ck && http_cab_guarda(r, nom, val)) return -1;
        }
        o = e2 + 2;
    }
    if (r->chunked) {
        /* v2.4.3: chunked manda; Content-Length (si lo hay) se ignora
           y no limita (el tope lo pone capcuerpo al acumular). */
        r->tiene_cl = 0;
        r->content_length = 0;
    } else if (r->tiene_cl && r->content_length > (unsigned int)capcuerpo) {
        r->err = -8;
        return -1;
    }
    if ((r->codigo >= 100 && r->codigo < 200) || r->codigo == 204 ||
        r->codigo == 304) {
        r->estado = U_HTTP_ST_OK;
        return 1;
    }
    if (r->tiene_cl && r->content_length == 0) {
        r->estado = U_HTTP_ST_OK;
        return 1;
    }
    r->estado = U_HTTP_ST_CUERPO;
    return 0;
}

void u_http_res_inicia(u_http_res_t *r) {
    int i, j;
    if (!r) return;
    r->estado = U_HTTP_ST_LINEA;
    r->err = 0;
    r->codigo = 0;
    r->version[0] = 0;
    r->motivo[0] = 0;
    r->ncab = 0;
    for (i = 0; i < U_HTTP_MAX_CAB; i++) {
        r->hnom[i][0] = 0;
        r->hval[i][0] = 0;
    }
    r->content_length = 0;
    r->tiene_cl = 0;
    r->chunked = 0;
    r->cuerpo_n = 0;
    r->cuerpo_esp = 0;
    r->ch_rest = -1;
    r->ch_nlin = 0;
    r->ch_crlf = 0;
    r->ip = 0;
    r->puerto = 0;
    http_hlen = 0;
    for (j = 0; j < (int)sizeof(http_hbuf); j++) http_hbuf[j] = 0;
}

/* v2.4.3: interpreta linea de tamano chunked ("1A[;ext]" o "0").
   Devuelve tamano o -1 si malformada. */
static int http_ch_tam(const char *l, int n) {
    unsigned int v = 0;
    int i = 0, nd = 0;
    if (n <= 0) return -1;
    for (; i < n; i++) {
        char c = l[i];
        if (c >= '0' && c <= '9') v = v * 16u + (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            v = v * 16u + (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v = v * 16u + (unsigned int)(c - 'A' + 10);
        else
            break;
        nd++;
        if (v > 0xFFFFFFu) return -1;   /* tope cuerdo: 16M por trozo */
    }
    if (!nd) return -1;
    for (; i < n; i++) {
        char c = l[i];
        if (c == ';') {
            for (; i < n; i++)
                if (l[i] < 32 || l[i] > 126) return -1;
            return (int)v;
        }
        if (c != ' ' && c != '\t') return -1;
    }
    return (int)v;
}

/* v2.4.3: avanza el cuerpo chunked con d[o..n). 1 completo (estado OK),
   0 falta mas (o==n), -1 error (estado ERR). Incremental: trozos,
   lineas y CRLF pueden venir partidos entre llamadas. */
static int http_ch_avanza(u_http_res_t *r, const unsigned char *d, int *po,
                          int n, unsigned char *cuerpo, int capcuerpo,
                          int *nbody) {
    int o = *po, i;
    while (o < n) {
        if (r->ch_rest == -2) {
            /* trailers: se ignoran; linea vacia = fin */
            if (r->ch_nlin < 31 &&
                !(r->ch_nlin > 0 && d[o] == '\n' &&
                  r->ch_lin[r->ch_nlin - 1] == '\r')) {
                if (d[o] == '\r' ||
                    (d[o] >= 32 && d[o] <= 126) || d[o] == '\t') {
                    r->ch_lin[r->ch_nlin++] = (char)d[o++];
                } else {
                    r->estado = U_HTTP_ST_ERR;
                    r->err = -6;
                    return -1;
                }
                continue;
            }
            if (d[o] == '\n' && r->ch_nlin > 0 &&
                r->ch_lin[r->ch_nlin - 1] == '\r') {
                o++;
                if (r->ch_nlin == 1) {
                    /* linea vacia: mensaje completo */
                    r->ch_nlin = 0;
                    if (o < n) {
                        r->estado = U_HTTP_ST_ERR;
                        r->err = -8;
                        return -1;
                    }
                    r->estado = U_HTTP_ST_OK;
                    *po = o;
                    return 1;
                }
                r->ch_nlin = 0;
                continue;
            }
            r->estado = U_HTTP_ST_ERR;
            r->err = -6;
            return -1;
        }
        if (r->ch_rest == -1) {
            /* linea de tamano */
            if (r->ch_nlin < 31 &&
                !(r->ch_nlin > 0 && d[o] == '\n' &&
                  r->ch_lin[r->ch_nlin - 1] == '\r')) {
                r->ch_lin[r->ch_nlin++] = (char)d[o++];
                continue;
            }
            if (d[o] == '\n' && r->ch_nlin > 0 &&
                r->ch_lin[r->ch_nlin - 1] == '\r') {
                int t = http_ch_tam(r->ch_lin, r->ch_nlin - 1);
                o++;
                r->ch_nlin = 0;
                if (t < 0) {
                    r->estado = U_HTTP_ST_ERR;
                    r->err = -6;
                    return -1;
                }
                r->ch_rest = (t == 0) ? -2 : t;
                r->ch_crlf = 0;
                continue;
            }
            r->estado = U_HTTP_ST_ERR;
            r->err = -6;
            return -1;
        }
        if (r->ch_rest > 0) {
            unsigned int rest = (unsigned int)(n - o);
            unsigned int want = (unsigned int)r->ch_rest;
            unsigned int toma = rest < want ? rest : want;
            if ((unsigned int)(*nbody) + toma > (unsigned int)capcuerpo) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -8;
                return -1;
            }
            for (i = 0; (unsigned int)i < toma; i++)
                cuerpo[*nbody + i] = d[o + i];
            *nbody += (int)toma;
            r->cuerpo_n += toma;
            o += (int)toma;
            r->ch_rest -= (int)toma;
            continue;
        }
        /* ch_rest == 0: CRLF de fin de datos */
        if (!r->ch_crlf) {
            if (d[o] == '\r') {
                r->ch_crlf = 1;
                o++;
                continue;
            }
        } else if (d[o] == '\n') {
            r->ch_crlf = 0;
            r->ch_rest = -1;
            o++;
            continue;
        }
        r->estado = U_HTTP_ST_ERR;
        r->err = -6;
        return -1;
    }
    *po = o;
    return 0;
}

int u_http_res_datos(u_http_res_t *r, const unsigned char *d, int n,
                     unsigned char *cuerpo, int capcuerpo, int *nbody) {
    int o = 0, p, i;
    if (!r || !d || n < 0 || !cuerpo || !nbody) {
        if (r) {
            r->estado = U_HTTP_ST_ERR;
            r->err = -6;
        }
        return -1;
    }
    if (capcuerpo < 1) capcuerpo = 1;
    if (capcuerpo > U_HTTP_MAX_CPO) capcuerpo = U_HTTP_MAX_CPO;
    *nbody = (int)r->cuerpo_n;
    if (r->estado == U_HTTP_ST_OK) {
        if (n > 0) {
            r->estado = U_HTTP_ST_ERR;
            r->err = -8;
            return -1;
        }
        return 1;
    }
    if (r->estado == U_HTTP_ST_ERR) return -1;
    if (r->estado == U_HTTP_ST_LINEA || r->estado == U_HTTP_ST_CAB) {
        /* Búfer solo de cabeceras: se consume d[] hasta ver
           CRLFCRLF; el resto de d[] es cuerpo y no se copia a
           http_hbuf (evita falsos -6 y el desfase o=p cuando las
           cabeceras venían divididas). El fin, si aparece ahora,
           termina justo en el último byte consumido. */
        o = 0;
        p = -1;
        for (i = 0; i < n; i++) {
            if (http_hlen >= U_HTTP_MAX_SEC) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -6;
                return -1;
            }
            http_hbuf[http_hlen++] = d[i];
            if (http_hlen >= 4 &&
                http_hbuf[http_hlen - 4] == '\r' &&
                http_hbuf[http_hlen - 3] == '\n' &&
                http_hbuf[http_hlen - 2] == '\r' &&
                http_hbuf[http_hlen - 1] == '\n') {
                p = http_hlen;
                o = i + 1;
                break;
            }
        }
        if (p < 0) {
            if (http_hlen >= U_HTTP_MAX_SEC) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -6;
                return -1;
            }
            for (i = 0; i + 1 < http_hlen; i++) {
                if (http_hbuf[i] == '\r' &&
                    http_hbuf[i + 1] == '\n') {
                    r->estado = U_HTTP_ST_CAB;
                    break;
                }
            }
            return 0;
        }
        if (http_cabeceras(r, p, capcuerpo) < 0) {
            r->estado = U_HTTP_ST_ERR;
            if (!r->err) r->err = -6;
            return -1;
        }
        if (r->estado == U_HTTP_ST_OK) {
            if (o < n) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -8;
                return -1;
            }
            return 1;
        }
        if (r->estado == U_HTTP_ST_CUERPO && r->tiene_cl &&
            r->cuerpo_n >= r->content_length) {
            if (o < n) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -8;
                return -1;
            }
            r->estado = U_HTTP_ST_OK;
            return 1;
        }
    }
    while (o < n) {
        if (r->chunked) {
            int q = http_ch_avanza(r, d, &o, n, cuerpo, capcuerpo,
                                   nbody);
            if (q != 0) return q;
            continue;
        }
        if (r->tiene_cl) {
            unsigned int need = r->content_length - r->cuerpo_n;
            unsigned int rest = (unsigned int)(n - o);
            if (rest > need) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -8;
                return -1;
            }
            if (r->cuerpo_n + rest > (unsigned int)capcuerpo) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -8;
                return -1;
            }
            for (i = 0; (unsigned int)i < rest; i++)
                cuerpo[*nbody + i] = d[o + i];
            *nbody += (int)rest;
            r->cuerpo_n += rest;
            o = n;
            if (r->cuerpo_n == r->content_length) {
                r->estado = U_HTTP_ST_OK;
                return 1;
            }
            return 0;
        } else {
            int rest = n - o;
            if ((unsigned int)(*nbody + rest) > (unsigned int)capcuerpo) {
                r->estado = U_HTTP_ST_ERR;
                r->err = -8;
                return -1;
            }
            for (i = 0; i < rest; i++) cuerpo[*nbody + i] = d[o + i];
            *nbody += rest;
            r->cuerpo_n += (unsigned int)rest;
            o = n;
            return 0;
        }
    }
    if (r->estado == U_HTTP_ST_OK) return 1;
    return 0;
}

int u_http_res_fin(u_http_res_t *r) {
    int era_cuerpo;
    if (!r) return -1;
    if (r->estado == U_HTTP_ST_OK) return 1;
    era_cuerpo = (r->estado == U_HTTP_ST_CUERPO);
    r->estado = U_HTTP_ST_ERR;
    r->err = era_cuerpo ? -8 : -6;
    if (era_cuerpo && !r->tiene_cl && !r->chunked) {
        /* Cierre sin CL: cuerpo completo (no vale para chunked:
           sin chunk-0 es truncado). */
        r->estado = U_HTTP_ST_OK;
        r->err = 0;
        return 1;
    }
    return -1;
}

int u_http_cabecera(const u_http_res_t *r, const char *nombre,
                    char *sal, int smax) {
    int i, k;
    if (!r || !nombre || !sal || smax <= 0) return -1;
    for (i = 0; i < r->ncab; i++) {
        int j = 0, ig = 1;
        while (nombre[j] && r->hnom[i][j]) {
            char a = nombre[j], b = r->hnom[i][j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) {
                ig = 0;
                break;
            }
            j++;
        }
        if (ig && !nombre[j] && !r->hnom[i][j]) {
            int t = 0;
            while (r->hval[i][t]) t++;
            for (k = 0; k < t && k + 1 < smax; k++)
                sal[k] = r->hval[i][k];
            sal[k] = 0;
            return t;
        }
    }
    return -1;
}

/* v2.4.4: jar de cookies por proceso (solo memoria). */
static char gal_host[U_HTTP_GAL_N][U_HTTP_GAL_NOM];
static char gal_nom[U_HTTP_GAL_N][U_HTTP_GAL_NOM];
static char gal_val[U_HTTP_GAL_N][U_HTTP_GAL_VAL];

void u_http_galleta_limpia(void) {
    int i, j;
    for (i = 0; i < U_HTTP_GAL_N; i++) {
        for (j = 0; j < U_HTTP_GAL_NOM; j++)
            gal_host[i][j] = gal_nom[i][j] = 0;
        for (j = 0; j < U_HTTP_GAL_VAL; j++) gal_val[i][j] = 0;
    }
}

static int gal_igual(const char *a, const char *b) {
    int i = 0;
    for (;;) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        if (!ca) return 1;
        i++;
    }
}

/* "nombre=valor[; attrs]": recorta en ';', quita blancos de borde y
   copia nombre (hasta '=') y valor en salidas acotadas. 0 ok. */
static int gal_parte(const char *s, char *nom, char *val) {
    int i = 0, k = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    while (s[i] && s[i] != '=' && s[i] != ';' && k + 1 < U_HTTP_GAL_NOM)
        nom[k++] = s[i++];
    nom[k] = 0;
    if (s[i] != '=') return -1;
    i++;
    k = 0;
    while (s[i] && s[i] != ';' && k + 1 < U_HTTP_GAL_VAL)
        val[k++] = s[i++];
    val[k] = 0;
    while (k > 0 && (val[k - 1] == ' ' || val[k - 1] == '\t')) val[--k] = 0;
    if (!nom[0]) return -1;
    return 0;
}

int u_http_galleta_guarda(const char *host, const char *setcookie) {
    char nm[U_HTTP_GAL_NOM], vl[U_HTTP_GAL_VAL];
    int i, libre = -1;
    if (!host || !setcookie || !host[0]) return -1;
    if (gal_parte(setcookie, nm, vl)) return -1;
    for (i = 0; i < U_HTTP_GAL_N; i++) {
        if (!gal_host[i][0]) {
            if (libre < 0) libre = i;
            continue;
        }
        if (gal_igual(gal_host[i], host) && !u_strcmp(gal_nom[i], nm)) {
            int k = 0;
            while (vl[k] && k + 1 < U_HTTP_GAL_VAL) {
                gal_val[i][k] = vl[k];
                k++;
            }
            gal_val[i][k] = 0;
            return 0;
        }
    }
    if (libre < 0) return -1;
    {
        int k = 0;
        while (host[k] && k + 1 < U_HTTP_GAL_NOM) {
            char c = host[k];
            if (c >= 'A' && c <= 'Z') c += 32;
            gal_host[libre][k] = c;
            k++;
        }
        gal_host[libre][k] = 0;
    }
    {
        int k = 0;
        while (nm[k] && k + 1 < U_HTTP_GAL_NOM) {
            gal_nom[libre][k] = nm[k];
            k++;
        }
        gal_nom[libre][k] = 0;
    }
    {
        int k = 0;
        while (vl[k] && k + 1 < U_HTTP_GAL_VAL) {
            gal_val[libre][k] = vl[k];
            k++;
        }
        gal_val[libre][k] = 0;
    }
    return 0;
}

int u_http_galleta_cab(const char *host, char *sal, int smax) {
    int i, t = 0, n = 0, k;
    static const char pre[] = "Cookie: ";
    static const char sep[] = "; ";
    static const char fin[] = "\r\n";
    if (!host || !sal || smax <= 0) return -1;
    for (i = 0; i < U_HTTP_GAL_N; i++)
        if (gal_host[i][0] && gal_igual(gal_host[i], host)) {
            int ln = 0, lv = 0;
            while (gal_nom[i][ln]) ln++;
            while (gal_val[i][lv]) lv++;
            if (t + (n ? 2 : 8) + ln + 1 + lv + 2 >= smax) break;
            if (!n) {
                for (k = 0; pre[k]; k++) sal[t++] = pre[k];
            } else {
                for (k = 0; sep[k]; k++) sal[t++] = sep[k];
            }
            for (k = 0; k < ln; k++) sal[t++] = gal_nom[i][k];
            sal[t++] = '=';
            for (k = 0; k < lv; k++) sal[t++] = gal_val[i][k];
            n++;
        }
    if (!n) {
        sal[0] = 0;
        return 0;
    }
    for (k = 0; fin[k]; k++) sal[t++] = fin[k];
    sal[t] = 0;
    return t;
}

int u_http_cabecera_n(const u_http_res_t *r, const char *nombre, int idx,
                      char *sal, int smax) {
    int i, k, n = 0;
    if (!r || !nombre || idx < 0 || !sal || smax <= 0) return -1;
    for (i = 0; i < r->ncab; i++) {
        int j = 0, ig = 1;
        while (nombre[j] && r->hnom[i][j]) {
            char a = nombre[j], b = r->hnom[i][j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) {
                ig = 0;
                break;
            }
            j++;
        }
        if (!(ig && !nombre[j] && !r->hnom[i][j])) continue;
        if (n++ < idx) continue;
        {
            int t = 0;
            while (r->hval[i][t]) t++;
            for (k = 0; k < t && k + 1 < smax; k++)
                sal[k] = r->hval[i][k];
            sal[k] = 0;
            return t;
        }
    }
    return -1;
}
/* Nucleo comun GET/POST (ver u_http_trae_x). metodo "GET" o "POST"
   (0 = GET); extra son cabeceras "N: v\r\n" (p. ej. Cookie);
   post/npost cuerpo a enviar (solo POST). Mismos codigos. */
static int http_trae_x(const char *url, const char *metodo,
                       const char *extra, const unsigned char *post,
                       int npost, u_http_res_t *r, unsigned char *cuerpo,
                       int capcuerpo, unsigned int espera_ms) {
    u_http_url_t u;
    unsigned int ip, fin;
    int id, k, nbody = 0, posteo = 0, esget = 0;
    if (!url || !r || !cuerpo || capcuerpo < 1) return -1;
    if (!metodo || (metodo[0] == 'G' && metodo[1] == 'E' &&
                    metodo[2] == 'T' && !metodo[3]))
        esget = 1;
    else if (metodo[0] == 'P' && metodo[1] == 'O' &&
             metodo[2] == 'S' && metodo[3] == 'T' && !metodo[4])
        posteo = 1;
    else
        return -1;
    if (npost < 0 || npost > U_HTTP_MAX_CPO) return -1;
    if (posteo && npost > 0 && !post) return -1;
    if (capcuerpo > U_HTTP_MAX_CPO) capcuerpo = U_HTTP_MAX_CPO;
    if (u_http_url(url, &u)) return -2;
    if (!espera_ms) espera_ms = 60000;
    if (espera_ms > 120000) espera_ms = 120000;
    fin = http_ahora() + espera_ms / 10u;
    u_http_res_inicia(r);
    r->puerto = u.puerto;
    if (u.es_ip) {
        ip = u.ip;
    } else {
        u_dns_res_t dr;
        unsigned int rest, tmo;
        int kd;
        if ((int)(http_ahora() - fin) >= 0) return -5;
        rest = (unsigned int)((int)(fin - http_ahora()));
        tmo = rest * 10u;
        if (tmo > 30000) tmo = 30000;
        if (tmo < 100) tmo = 100;
        kd = u_dns_resuelve3(u.host, 0, 0, &dr, tmo);
        if (kd == -3) return -3;
        if (kd == -5) return -5;
        if (kd) return -4;
        ip = dr.ip;
    }
    r->ip = ip;
    if ((int)(http_ahora() - fin) >= 0) return -5;
    id = u_tcp_abrir(ip, u.puerto, 0);
    if (id <= 0) return -5;
    {
        int npet, env = 0;
        if (posteo)
            npet = u_http_pide_post(&u, extra, npost, http_pet,
                                    (int)sizeof(http_pet));
        else
            npet = u_http_pide(&u, extra, http_pet,
                               (int)sizeof(http_pet));
        if (npet < 0) {
            u_tcp_abortar(id);
            return -2;
        }
        while (env < npet) {
            int trozo = npet - env, w;
            if (trozo > 536) trozo = 536;
            if ((int)(http_ahora() - fin) >= 0) {
                u_tcp_abortar(id);
                return -5;
            }
            w = u_tcp_enviar(id, http_pet + env, trozo);
            if (w > 0) {
                if (w != trozo) {
                    u_tcp_abortar(id);
                    return -5;
                }
                env += w;
                continue;
            }
            if (w == -8) {
                u_dormir(100);
                continue;
            }
            u_tcp_abortar(id);
            return -5;
        }
        /* POST: cuerpo tras las cabeceras, en trozos de 536. */
        if (posteo && npost > 0) {
            int envp = 0;
            while (envp < npost) {
                int trozo = npost - envp, w;
                if (trozo > 536) trozo = 536;
                if ((int)(http_ahora() - fin) >= 0) {
                    u_tcp_abortar(id);
                    return -5;
                }
                w = u_tcp_enviar(id, post + envp, trozo);
                if (w > 0) {
                    if (w != trozo) {
                        u_tcp_abortar(id);
                        return -5;
                    }
                    envp += w;
                    continue;
                }
                if (w == -8) {
                    u_dormir(100);
                    continue;
                }
                u_tcp_abortar(id);
                return -5;
            }
        }
    }
    for (;;) {
        unsigned char tb[8192];   /* v2.4.3: 8K por llamada (ventana 8K) */
        unsigned int rest;
        int rr, q, espera;
        if ((int)(http_ahora() - fin) >= 0) {
            u_tcp_abortar(id);
            return -5;
        }
        rest = (unsigned int)((int)(fin - http_ahora())) * 10u;
        espera = rest > 5000 ? 5000 : (rest ? (int)rest : 1);
        rr = u_tcp_recibir(id, tb, (int)sizeof(tb),
                           (unsigned short)espera);
        if (rr > 0) {
            q = u_http_res_datos(r, tb, rr, cuerpo, capcuerpo,
                                 &nbody);
            if (q > 0) break;
            if (q < 0) {
                u_tcp_abortar(id);
                if (r->err == -7) return -7;
                if (r->err == -8) return -8;
                return -6;
            }
            continue;
        }
        if (!rr) continue;
        if (rr == -12) {
            q = u_http_res_fin(r);
            if (q > 0) break;
            u_tcp_abortar(id);
            if (r->err == -6) return -6;
            return -8;
        }
        u_tcp_abortar(id);
        return -5;
    }
    {
        unsigned char tb[64];
        int rr2;
        if ((int)(http_ahora() - fin) >= 0) {
            /* Respuesta completa y valida: cierre ordenado aunque
               se agote el tiempo global. */
            (void)u_tcp_cerrar(id);
            return 0;
        }
        rr2 = u_tcp_recibir(id, tb, (int)sizeof(tb), 500);
        if (rr2 > 0) {
            u_tcp_abortar(id);
            return -8;
        }
        if (rr2 == -12) {
            (void)u_tcp_cerrar(id);
            return 0;
        }
        /* Servidor con la conexion retenida (p. ej. 204 que tarda):
           FIN ordenado en vez de RST y exito igual. */
        (void)u_tcp_cerrar(id);
        return 0;
    }
}

int u_http_trae(const char *url, u_http_res_t *r, unsigned char *cuerpo,
                int capcuerpo, unsigned int espera_ms) {
    return http_trae_x(url, "GET", 0, 0, 0, r, cuerpo, capcuerpo,
                       espera_ms);
}

int u_http_trae_x(const char *url, const char *metodo, const char *extra,
                  const unsigned char *post, int npost, u_http_res_t *r,
                  unsigned char *cuerpo, int capcuerpo,
                  unsigned int espera_ms) {
    return http_trae_x(url, metodo, extra, post, npost, r, cuerpo,
                       capcuerpo, espera_ms);
}

/* ---- F7-C: HTML minimo (parser + URL + layout; sin JS/CSS) ----
   Sin libc: solo u_* y bucles acotados. Sin recursion. Todo
   truncado se avisa (d->truncado / retorno 1), nunca silencioso. */

static int html_mini(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int html_es_blanco(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
        c == '\f' || c == '\v';
}

/* Copia s[0..n) a d (cap) decodificando UTF-8; lo invalido se
   sustituye por U+FFFD (EF BF BD). Devuelve bytes escritos. */
static int html_utf8_copia(const unsigned char *s, int n, char *d,
                           int cap) {
    int i = 0, t = 0;
    while (i < n && t < cap) {
        unsigned char c0 = s[i];
        if (c0 < 0x80) {
            d[t++] = (char)c0;
            i++;
        } else if (c0 >= 0xC2 && c0 <= 0xDF && i + 1 < n &&
            (s[i + 1] & 0xC0) == 0x80) {
            if (t + 1 >= cap) break;
            d[t++] = (char)s[i];
            d[t++] = (char)s[i + 1];
            i += 2;
        } else if (c0 >= 0xE0 && c0 <= 0xEF && i + 2 < n &&
            (s[i + 1] & 0xC0) == 0x80 &&
            (s[i + 2] & 0xC0) == 0x80) {
            unsigned int cp =
                ((unsigned int)(c0 & 0x0F) << 12) |
                ((unsigned int)(s[i + 1] & 0x3F) << 6) |
                (unsigned int)(s[i + 2] & 0x3F);
            if ((c0 == 0xE0 && s[i + 1] < 0xA0) || c0 == 0xED ||
                cp < 0x800) {
                if (t + 2 >= cap) break;
                d[t++] = (char)0xEF;
                d[t++] = (char)0xBF;
                d[t++] = (char)0xBD;
                i++;
            } else {
                if (t + 2 >= cap) break;
                d[t++] = (char)s[i];
                d[t++] = (char)s[i + 1];
                d[t++] = (char)s[i + 2];
                i += 3;
            }
        } else if (c0 >= 0xF0 && c0 <= 0xF4 && i + 3 < n &&
            (s[i + 1] & 0xC0) == 0x80 &&
            (s[i + 2] & 0xC0) == 0x80 &&
            (s[i + 3] & 0xC0) == 0x80) {
            unsigned int cp =
                ((unsigned int)(c0 & 0x07) << 18) |
                ((unsigned int)(s[i + 1] & 0x3F) << 12) |
                ((unsigned int)(s[i + 2] & 0x3F) << 6) |
                (unsigned int)(s[i + 3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) {
                if (t + 2 >= cap) break;
                d[t++] = (char)0xEF;
                d[t++] = (char)0xBF;
                d[t++] = (char)0xBD;
                i++;
            } else {
                if (t + 3 >= cap) break;
                d[t++] = (char)s[i];
                d[t++] = (char)s[i + 1];
                d[t++] = (char)s[i + 2];
                d[t++] = (char)s[i + 3];
                i += 4;
            }
        } else {
            if (t + 2 >= cap) break;
            d[t++] = (char)0xEF;
            d[t++] = (char)0xBF;
            d[t++] = (char)0xBD;
            i++;
        }
    }
    return t;
}

/* Decodifica una entidad en s[0..max) (s[0] == '&'). Escribe en
   sal (cap>=4) y devuelve consumidos de s (minimo 1). Desconocida
   o sin ';' se deja literal ('&'). */
static int html_entidad(const unsigned char *s, int max,
                        unsigned char *sal) {
    int i;
    unsigned int v;
    if (max < 3 || s[0] != '&') {
        sal[0] = '&';
        return 1;
    }
    if (s[1] == '#') {
        int hex = 0;
        v = 0;
        i = 2;
        if (i < max && (s[i] == 'x' || s[i] == 'X')) {
            hex = 1;
            i++;
        }
        if (i >= max) {
            sal[0] = '&';
            return 1;
        }
        if (hex) {
            int nd = 0;
            while (i < max && s[i] != ';' && nd < 8) {
                int c = s[i];
                int d = -1;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else break;
                v = v * 16 + (unsigned int)d;
                nd++;
                i++;
            }
            if (!nd || i >= max || s[i] != ';') {
                sal[0] = '&';
                return 1;
            }
        } else {
            int nd = 0;
            while (i < max && s[i] != ';' && nd < 10) {
                int c = s[i];
                if (c < '0' || c > '9') break;
                v = v * 10 + (unsigned int)(c - '0');
                nd++;
                i++;
            }
            if (!nd || i >= max || s[i] != ';') {
                sal[0] = '&';
                return 1;
            }
        }
        if (v == 0 || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
            v = 0xFFFD;
        if (v < 0x80) {
            sal[0] = (unsigned char)v;
            return i + 1;
        } else if (v < 0x800) {
            sal[0] = (unsigned char)(0xC0 | (v >> 6));
            sal[1] = (unsigned char)(0x80 | (v & 0x3F));
            return i + 1;
        } else if (v < 0x10000) {
            sal[0] = (unsigned char)(0xE0 | (v >> 12));
            sal[1] = (unsigned char)(0x80 | ((v >> 6) & 0x3F));
            sal[2] = (unsigned char)(0x80 | (v & 0x3F));
            return i + 1;
        }
        sal[0] = (unsigned char)(0xF0 | (v >> 18));
        sal[1] = (unsigned char)(0x80 | ((v >> 12) & 0x3F));
        sal[2] = (unsigned char)(0x80 | ((v >> 6) & 0x3F));
        sal[3] = (unsigned char)(0x80 | (v & 0x3F));
        return i + 1;
    }
    {
        struct {
            const char *nom;
            unsigned char b0, b1;
            int nb;
        } tab[] = {
            { "amp;", '&', 0, 1 }, { "lt;", '<', 0, 1 },
            { "gt;", '>', 0, 1 }, { "quot;", '"', 0, 1 },
            { "apos;", '\'', 0, 1 },
            { "nbsp;", 0xC2, 0xA0, 2 }, { 0, 0, 0, 0 }
        };
        int k = 0;
        while (tab[k].nom) {
            int q = 0;
            while (tab[k].nom[q] && 1 + q < max &&
                s[1 + q] == (unsigned char)tab[k].nom[q]) q++;
            if (tab[k].nom[q] == 0) {
                sal[0] = tab[k].b0;
                if (tab[k].nb > 1) sal[1] = tab[k].b1;
                return 1 + q;
            }
            k++;
        }
        sal[0] = '&';
        return 1;
    }
}

/* Identificadores de etiqueta (solo subset F7-C). */
#define HT_DES 0
#define HT_HTML 1
#define HT_HEAD 2
#define HT_BODY 3
#define HT_TITLE 4
#define HT_H 5
#define HT_P 6
#define HT_BR 7
#define HT_HR 8
#define HT_DIV 9
#define HT_SPAN 10
#define HT_B 11
#define HT_I 12
#define HT_UL 13
#define HT_OL 14
#define HT_LI 15
#define HT_A 16
#define HT_IMG 17
#define HT_SCRIPT 18
#define HT_STYLE 19
#define HT_OBJECT 20
#define HT_FORM 21
#define HT_INPUT 22
#define HT_SELECT 23
#define HT_TEXTAREA 24
#define HT_BUTTON 25

static int html_tag_id(const char *s, int n, int *nh) {
    static const char *noms[] = {
        "html", "head", "body", "title", "h", "p", "br", "hr",
        "div", "span", "b", "i", "ul", "ol", "li", "a", "img",
        "script", "style", "object", "embed",
        "form", "input", "select", "textarea", "button", 0
    };
    static const int ids[] = {
        HT_HTML, HT_HEAD, HT_BODY, HT_TITLE, HT_H, HT_P, HT_BR,
        HT_HR, HT_DIV, HT_SPAN, HT_B, HT_I, HT_UL, HT_OL, HT_LI,
        HT_A, HT_IMG, HT_SCRIPT, HT_STYLE, HT_OBJECT, HT_OBJECT,
        HT_FORM, HT_INPUT, HT_SELECT, HT_TEXTAREA, HT_BUTTON
    };
    int k = 0;
    *nh = 0;
    if (n >= 2 && s[0] == 'h' && s[1] >= '1' && s[1] <= '6' &&
        (n == 2)) {
        *nh = s[1] - '0';
        return HT_H;
    }
    if (n >= 2 && s[0] == 's') {
        if (n == 6 && s[1] == 't' && s[2] == 'r' && s[3] == 'o' &&
            s[4] == 'n' && s[5] == 'g')
            return HT_B;
        if (n == 2 && s[1] == 'm')
            return HT_I;
    }
    while (noms[k]) {
        int q = 0;
        while (noms[k][q] && q < n && noms[k][q] == s[q]) q++;
        if (!noms[k][q] && q == n) return ids[k];
        k++;
    }
    return HT_DES;
}

/* Estado del parser (solo pila; el documento es el destino). */
/* v2.4: CSS basico (color/fondo + negrita/cursiva) y formularios
   GET. Sin cascada completa: ultima regla valida gana; inline >
   clase > etiqueta. Todo acotado y con aviso de truncado. */
#define U_CSS_MAX_REGLA 64
#define U_CSS_MAX_SEL 16
#define U_CSS_BUF 4096
typedef struct {
    char sel[U_CSS_MAX_SEL];   /* etiqueta en minusculas o clase */
    int es_clase;              /* 1 si .clase, 0 si etiqueta */
    int fg;                    /* RGB, -1 sin fijar */
    int bg;                    /* RGB, -1 sin fijar */
    int bold;                  /* 1/0, -1 sin fijar */
    int ital;                  /* 1/0, -1 sin fijar */
} css_regla_t;

typedef struct {
    u_html_doc_t *d;
    const char *base;
    int pila_b[U_HTML_MAX_PILA]; /* negrita */
    int pila_i[U_HTML_MAX_PILA]; /* enfasis */
    int pila_e[U_HTML_MAX_PILA]; /* enlace o -1 */
    int pila_t[U_HTML_MAX_PILA]; /* id de etiqueta */
    int npila;
    int st_tag[16];              /* nest de estilo (todos los elementos) */
    int st_fg[16];
    int st_bg[16];
    int st_b[16];
    int st_i[16];
    int nest;
    int css;                     /* 1 dentro de <style> (captura) */
    unsigned char css_buf[U_CSS_BUF];
    int css_n;
    css_regla_t reg[U_CSS_MAX_REGLA];
    int nreg;
    char f_acc[U_HTML_URL];      /* action absoluta del form en curso */
    char f_campo[64];            /* primer input de texto */
    int f_met;                   /* 0 get (defecto), 1 otro */
    int f_ok;                    /* action valida */
    int en_forma;                /* 1 dentro de <form> */
    int cur;                    /* tramo en curso o -1 (o -2 gota) */
    int curb;                   /* bloque en curso o -1 (o -2 gota) */
    int titulo;                 /* 1 dentro de <title> */
    int salto;                  /* id en skip (script/style/object) */
    int lista;                  /* profundidad de lista 0..8 */
    int ord[8];                 /* 1 si el nivel es ol */
    int nnum[8];                /* siguiente numero por nivel ol */
} html_st_t;

static int html_attr(const unsigned char *pg, int ini, int fin,
                     const char *nom, unsigned char *sal, int cap);

static int html_est(html_st_t *e) {
    if (e->npila > 0 && e->pila_e[e->npila - 1] >= 0)
        return U_HTML_E_ENLACE;
    if (e->npila > 0 && e->pila_b[e->npila - 1]) return U_HTML_E_NEGRITA;
    if (e->nest > 0 && e->st_b[e->nest - 1]) return U_HTML_E_NEGRITA;
    if (e->npila > 0 && e->pila_i[e->npila - 1]) return U_HTML_E_ENFASIS;
    if (e->nest > 0 && e->st_i[e->nest - 1]) return U_HTML_E_ENFASIS;
    return U_HTML_E_NORMAL;
}

static int html_enl(html_st_t *e) {
    if (e->npila <= 0) return -1;
    return e->pila_e[e->npila - 1];
}

/* ---- v2.4: CSS basico ---- */
static int css_igual(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        int ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        i++;
    }
    return a[i] == b[i];
}

/* Nombre de color CSS a RGB (16 basicos + transparent=ignorar).
   Devuelve RGB, -1 si no es color (-2 transparente). */
static int css_color(const char *s) {
    static const char *nom[] = {
        "black", "silver", "gray", "white", "maroon", "red",
        "purple", "fuchsia", "green", "lime", "olive", "yellow",
        "navy", "blue", "teal", "aqua", 0
    };
    static const int val[] = {
        0x000000, 0xC0C0C0, 0x808080, 0xFFFFFF, 0x800000, 0xFF0000,
        0x800080, 0xFF00FF, 0x008000, 0x00FF00, 0x808000, 0xFFFF00,
        0x000080, 0x0000FF, 0x008080, 0x00FFFF
    };
    int i = 0, v = 0, k;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (css_igual(s + i, "transparent")) return -2;
    if (s[i] == '#') {
        int n = 0;
        i++;
        while (((s[i] >= '0' && s[i] <= '9') ||
                (s[i] >= 'a' && s[i] <= 'f') ||
                (s[i] >= 'A' && s[i] <= 'F')) && n < 6) {
            int d = s[i];
            if (d >= '0' && d <= '9') d -= '0';
            else if (d >= 'a' && d <= 'f') d = d - 'a' + 10;
            else d = d - 'A' + 10;
            v = v * 16 + d;
            i++;
            n++;
        }
        if (n == 3)
            return ((v & 0xF00) << 8) | ((v & 0x0F0) << 4) | (v & 0x00F) |
                   ((v & 0xF00) << 12) | ((v & 0x0F0) << 8) | ((v & 0x00F) << 4);
        if (n == 6) return v & 0xFFFFFF;
        return -1;
    }
    k = 0;
    while (nom[k]) {
        int q = 0;
        while (nom[k][q] && s[i + q] &&
               nom[k][q] == (s[i + q] >= 'A' && s[i + q] <= 'Z' ?
                             s[i + q] + 32 : s[i + q]))
            q++;
        if (!nom[k][q] && (s[i + q] == 0 || s[i + q] == ' ' ||
                           s[i + q] == '\t' || s[i + q] == ';' ||
                           s[i + q] == '!'))
            return val[k];
        k++;
    }
    return -1;
}

/* Declara "prop : valor" (ya recortada). Ajusta fg/bg/bold/ital. */
static void css_una_decl(const char *pr, const char *va, int *fg,
                         int *bg, int *bo, int *it) {
    if (css_igual(pr, "color")) {
        int c = css_color(va);
        if (c >= 0) *fg = c;
    } else if (css_igual(pr, "background-color") ||
               css_igual(pr, "background")) {
        int c = css_color(va);
        if (c >= 0) *bg = c;
    } else if (css_igual(pr, "font-weight")) {
        if (css_igual(va, "bold")) *bo = 1;
        else if (css_igual(va, "normal")) *bo = 0;
    } else if (css_igual(pr, "font-style")) {
        if (css_igual(va, "italic") || css_igual(va, "oblique"))
            *it = 1;
        else if (css_igual(va, "normal")) *it = 0;
    }
}

/* Lista "p:v; p:v" (cadenas mutables acotadas). */
static void css_decls(char *s, int *fg, int *bg, int *bo, int *it) {
    char *p = s;
    while (*p) {
        char *d2, pr[32], va[64];
        int i = 0;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ||
               *p == ';')
            p++;
        if (!*p) break;
        d2 = p;
        while (*d2 && *d2 != ':' && *d2 != ';' && *d2 != '}')
            d2++;
        if (*d2 != ':') {
            while (*d2 && *d2 != ';' && *d2 != '}') d2++;
            p = *d2 ? d2 + 1 : d2;
            continue;
        }
        i = 0;
        while (p < d2 && i < 31) pr[i++] = *p++;
        pr[i] = 0;
        p = d2 + 1;
        while (*p == ' ' || *p == '\t') p++;
        i = 0;
        while (*p && *p != ';' && *p != '}' && i < 63) va[i++] = *p++;
        va[i] = 0;
        while (i > 0 && (va[i - 1] == ' ' || va[i - 1] == '\t'))
            va[--i] = 0;
        css_una_decl(pr, va, fg, bg, bo, it);
        if (*p) p++;
    }
}

/* Nombre de etiqueta para reglas (solo subset con estilo). */
static void html_tag_nom(int tag, int nh, char *sal) {
    sal[0] = 0;
    if (tag == HT_H) {
        if (nh >= 1 && nh <= 6) {
            sal[0] = 'h';
            sal[1] = (char)('0' + nh);
            sal[2] = 0;
        }
        return;
    }
    {
        const char *n = 0;
        if (tag == HT_P) n = "p";
        else if (tag == HT_DIV) n = "div";
        else if (tag == HT_SPAN) n = "span";
        else if (tag == HT_B) n = "b";
        else if (tag == HT_I) n = "i";
        else if (tag == HT_UL) n = "ul";
        else if (tag == HT_OL) n = "ol";
        else if (tag == HT_LI) n = "li";
        else if (tag == HT_A) n = "a";
        else if (tag == HT_BODY) n = "body";
        else if (tag == HT_HTML) n = "html";
        else if (tag == HT_HEAD) n = "head";
        else if (tag == HT_TITLE) n = "title";
        else if (tag == HT_FORM) n = "form";
        else if (tag == HT_INPUT) n = "input";
        else if (tag == HT_BUTTON) n = "button";
        if (n) {
            int i = 0;
            while (n[i] && i < 15) {
                sal[i] = n[i];
                i++;
            }
            sal[i] = 0;
        }
    }
}

/* Aplica una regla ya recortada ("sel" simple, "decls" crudas). */
static void css_regla_add(html_st_t *e, const char *sel,
                          const char *decls) {
    css_regla_t *r;
    int es_cl = 0, i = 0, f, b, bo, it;
    char loc[U_CSS_MAX_SEL];
    static char dc[512];
    if (e->nreg >= U_CSS_MAX_REGLA) {
        e->d->truncado = 1;
        return;
    }
    while (sel[i] == ' ' || sel[i] == '\t') i++;
    if (sel[i] == '.') {
        es_cl = 1;
        i++;
    }
    f = 0;
    while (sel[i] && sel[i] != ' ' && sel[i] != '\t' &&
           sel[i] != ':' && sel[i] != '.' && sel[i] != '#' &&
           sel[i] != '[' && f < U_CSS_MAX_SEL - 1)
        loc[f++] = sel[i++];
    loc[f] = 0;
    if (!f) return;
    if (!es_cl) {
        /* Ultimo componente (ignora "div p": casa con p). */
        int u = 0, ini = 0;
        while (loc[u]) {
            if (loc[u] == ' ') ini = u + 1;
            u++;
        }
        if (ini) {
            int q = 0;
            while (loc[ini + q] && q < U_CSS_MAX_SEL - 1) {
                loc[q] = loc[ini + q];
                q++;
            }
            loc[q] = 0;
        }
    }
    /* Copia mutable acotada de las declaraciones. */
    {
        int q = 0;
        while (decls[q] && q < 511) {
            dc[q] = decls[q];
            q++;
        }
        dc[q] = 0;
    }
    f = 0;
    b = 0;
    bo = 0;
    it = 0;
    {
        int ff = -1, bb = -1, oo = -1, ii = -1;
        css_decls(dc, &ff, &bb, &oo, &ii);
        f = ff;
        b = bb;
        bo = oo;
        it = ii;
    }
    r = &e->reg[e->nreg++];
    i = 0;
    while (loc[i] && i < U_CSS_MAX_SEL - 1) {
        r->sel[i] = loc[i] >= 'A' && loc[i] <= 'Z' ? loc[i] + 32 :
                                                          loc[i];
        i++;
    }
    r->sel[i] = 0;
    r->es_clase = es_cl;
    r->fg = f;
    r->bg = b;
    r->bold = bo;
    r->ital = it;
}

/* Interpreta la hoja capturada en e->css_buf. */
static void css_parse(html_st_t *e) {
    unsigned char *s = e->css_buf;
    int n = e->css_n, p = 0;
    while (p < n) {
        char sel[128], dec[512];
        int i = 0, q;
        while (p < n && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' ||
                         s[p] == '\n'))
            p++;
        if (p + 1 < n && s[p] == '/' && s[p + 1] == '*') {
            p += 2;
            while (p + 1 < n && !(s[p] == '*' && s[p + 1] == '/'))
                p++;
            p = p + 1 < n ? p + 2 : n;
            continue;
        }
        if (p >= n) break;
        if (s[p] == '@' || s[p] == '}') {
            while (p < n && s[p] != ';' && s[p] != '}' && s[p] != '{')
                p++;
            if (p < n && (s[p] == ';' || s[p] == '}')) p++;
            continue;
        }
        while (p < n && s[p] != '{' && i < 127) sel[i++] = (char)s[p++];
        sel[i] = 0;
        if (p >= n || s[p] != '{') break;
        p++;
        q = 0;
        while (p < n && s[p] != '}' && q < 511) dec[q++] = (char)s[p++];
        dec[q] = 0;
        if (p < n && s[p] == '}') p++;
        /* Lista separada por comas: cada una, una regla. */
        {
            int a = 0;
            while (sel[a]) {
                char un[128];
                int u = 0;
                while (sel[a] == ' ' || sel[a] == '\t') a++;
                while (sel[a] && sel[a] != ',' && u < 127)
                    un[u++] = sel[a++];
                un[u] = 0;
                if (u) css_regla_add(e, un, dec);
                if (sel[a] == ',') a++;
            }
        }
    }
}

/* Calcula fg/bg/bold/ital del elemento (tag + reglas + class + style).
   Hereda del padre (nest top). Todo -1/0 si nada aplica. */
static void css_elem(html_st_t *e, int tag, int nh,
                     const unsigned char *pg, int ini, int fin,
                     int *fg, int *bg, int *bo, int *it) {
    int pf = -1, pb = -1, pbo = 0, pit = 0, i;
    char tnom[16];
    unsigned char cv[128];
    unsigned char sv[512];
    if (e->nest > 0) {
        pf = e->st_fg[e->nest - 1];
        pb = e->st_bg[e->nest - 1];
        pbo = e->st_b[e->nest - 1];
        pit = e->st_i[e->nest - 1];
    }
    html_tag_nom(tag, nh, tnom);
    if (tnom[0]) {
        for (i = 0; i < e->nreg; i++) {
            if (!e->reg[i].es_clase && !u_strcmp(e->reg[i].sel, tnom)) {
                if (e->reg[i].fg >= 0) pf = e->reg[i].fg;
                if (e->reg[i].bg >= 0) pb = e->reg[i].bg;
                if (e->reg[i].bold >= 0) pbo = e->reg[i].bold;
                if (e->reg[i].ital >= 0) pit = e->reg[i].ital;
            }
        }
    }
    if (html_attr(pg, ini, fin, "class", cv, (int)sizeof(cv)) >= 0 &&
        cv[0]) {
        int p = 0;
        while (cv[p]) {
            char un[U_CSS_MAX_SEL];
            int q = 0;
            while (cv[p] == ' ' || cv[p] == '\t') p++;
            if (!cv[p]) break;
            while (cv[p] && cv[p] != ' ' && cv[p] != '\t' &&
                   q < U_CSS_MAX_SEL - 1)
                un[q++] = (char)(cv[p] >= 'A' && cv[p] <= 'Z' ?
                                 cv[p] + 32 :
                                 cv[p++]);
            un[q] = 0;
            for (i = 0; i < e->nreg; i++) {
                if (e->reg[i].es_clase && !u_strcmp(e->reg[i].sel, un)) {
                    if (e->reg[i].fg >= 0) pf = e->reg[i].fg;
                    if (e->reg[i].bg >= 0) pb = e->reg[i].bg;
                    if (e->reg[i].bold >= 0) pbo = e->reg[i].bold;
                    if (e->reg[i].ital >= 0) pit = e->reg[i].ital;
                }
            }
        }
    }
    if (html_attr(pg, ini, fin, "style", sv, (int)sizeof(sv)) >= 0 &&
        sv[0]) {
        int f2 = -1, b2 = -1, bo2 = -1, it2 = -1;
        static char ds[512];
        int q = 0;
        while (sv[q] && q < 511) {
            ds[q] = (char)sv[q];
            q++;
        }
        ds[q] = 0;
        css_decls(ds, &f2, &b2, &bo2, &it2);
        if (f2 >= 0) pf = f2;
        if (b2 >= 0) pb = b2;
        if (bo2 >= 0) pbo = bo2;
        if (it2 >= 0) pit = it2;
    }
    *fg = pf;
    *bg = pb;
    *bo = pbo;
    *it = pit;
}

/* Abre tramo si hace falta (0 almacenable, -1 gota). */
static int html_seg(html_st_t *e) {
    int est, en, fg, bg;
    if (e->cur == -2) return -1;
    est = html_est(e);
    en = html_enl(e);
    fg = e->nest > 0 ? e->st_fg[e->nest - 1] : -1;
    bg = e->nest > 0 ? e->st_bg[e->nest - 1] : -1;
    if (e->cur >= 0) {
        u_html_seg_t *s = &e->d->seg[e->cur];
        if (s->est == est && s->enlace == en && s->fg == fg &&
            s->bg == bg)
            return 0;
        if (s->len == 0) {
            e->d->nseg--;
            e->cur = -1;
        } else {
            e->cur = -1;
        }
    }
    if (e->d->nseg >= U_HTML_MAX_SEG) {
        e->d->truncado = 1;
        e->cur = -2;
        return -1;
    }
    e->cur = e->d->nseg;
    e->d->seg[e->cur].ini = e->d->texto_n;
    e->d->seg[e->cur].len = 0;
    e->d->seg[e->cur].est = est;
    e->d->seg[e->cur].enlace = en;
    e->d->seg[e->cur].fg = e->nest > 0 ? e->st_fg[e->nest - 1] : -1;
    e->d->seg[e->cur].bg = e->nest > 0 ? e->st_bg[e->nest - 1] : -1;
    e->d->nseg++;
    return 0;
}

/* Nest de estilo: hereda del padre; un nivel por elemento (16). */
static void html_seg_cierra(html_st_t *e);
static void nest_mete(html_st_t *e, int tag, int fg, int bg) {
    if (e->nest >= 16) {
        e->d->truncado = 1;
        return;
    }
    e->st_tag[e->nest] = tag;
    e->st_fg[e->nest] = fg;
    e->st_bg[e->nest] = bg;
    e->st_b[e->nest] = e->nest > 0 ? e->st_b[e->nest - 1] : 0;
    e->st_i[e->nest] = e->nest > 0 ? e->st_i[e->nest - 1] : 0;
    e->nest++;
    html_seg_cierra(e);
}

static void nest_saca(html_st_t *e, int tag) {
    int i, h = -1;
    for (i = e->nest - 1; i >= 0; i--) {
        if (e->st_tag[i] == tag) {
            h = i;
            break;
        }
    }
    if (h < 0) return;
    html_seg_cierra(e);
    e->nest = h;
}

static void html_seg_cierra(html_st_t *e) {
    if (e->cur >= 0) {
        if (e->d->seg[e->cur].len == 0)
            e->d->nseg--;
        e->cur = -1;
    }
}

/* Abre bloque (cierra el anterior con sus punteros inline).
   -1 ok, -2 gota. */
static int html_bloque(html_st_t *e, int tipo, int nivel, int img) {
    int i;
    if (e->curb == -2) return -2;
    if (e->curb >= 0) {
        u_html_bloq_t *b = &e->d->bloq[e->curb];
        for (i = e->npila - 1; i >= 0; i--) {
            if (e->pila_e[i] >= 0 &&
                e->pila_e[i] < e->d->nenl &&
                e->d->enl[e->pila_e[i]].len < 0)
                e->d->enl[e->pila_e[i]].len =
                    e->d->texto_n - e->d->enl[e->pila_e[i]].ini;
            if (e->pila_t[i] == HT_A) e->pila_e[i] = -1;
        }
        html_seg_cierra(e);
        b->seg_n = e->d->nseg - b->seg_ini;
        if (b->seg_n == 0 && (tipo == U_HTML_B_TXT ||
                tipo == U_HTML_B_P || tipo == U_HTML_B_LI ||
                (tipo >= U_HTML_B_H1 && tipo <= U_HTML_B_H6)))
            e->d->nbloq--;
        e->curb = -1;
    }
    if (e->d->nbloq >= U_HTML_MAX_BLOQ) {
        e->d->truncado = 1;
        e->curb = -2;
        return -2;
    }
    e->curb = e->d->nbloq;
    e->d->bloq[e->curb].tipo = tipo;
    e->d->bloq[e->curb].nivel = nivel;
    e->d->bloq[e->curb].seg_ini = e->d->nseg;
    e->d->bloq[e->curb].seg_n = 0;
    e->d->bloq[e->curb].img = img;
    e->d->nbloq++;
    return -1;
}

static void html_bloque_cierra(html_st_t *e) {
    if (e->curb >= 0) {
        u_html_bloq_t *b = &e->d->bloq[e->curb];
        int i;
        for (i = e->npila - 1; i >= 0; i--) {
            if (e->pila_e[i] >= 0 &&
                e->pila_e[i] < e->d->nenl &&
                e->d->enl[e->pila_e[i]].len < 0)
                e->d->enl[e->pila_e[i]].len =
                    e->d->texto_n - e->d->enl[e->pila_e[i]].ini;
            if (e->pila_t[i] == HT_A) e->pila_e[i] = -1;
        }
        html_seg_cierra(e);
        b->seg_n = e->d->nseg - b->seg_ini;
        if (b->seg_n == 0 && (b->tipo == U_HTML_B_TXT ||
                b->tipo == U_HTML_B_P || b->tipo == U_HTML_B_LI ||
                (b->tipo >= U_HTML_B_H1 && b->tipo <= U_HTML_B_H6)))
            e->d->nbloq--;
        e->curb = -1;
    }
}

/* Un byte decodificado al documento (texto o titulo). */
static void html_txt_byte(html_st_t *e, unsigned char c) {
    if (e->css) {
        if (e->css_n < U_CSS_BUF) e->css_buf[e->css_n++] = c;
        else e->d->truncado = 1;
        return;
    }
    if (e->titulo) {
        int n = u_strlen(e->d->titulo);
        if (n < 126) {
            e->d->titulo[n] = (char)c;
            e->d->titulo[n + 1] = 0;
        }
        return;
    }
    if (e->curb == -2) return;
    if (e->curb < 0)
        html_bloque(e, U_HTML_B_TXT, 0, -1);
    if (e->curb == -2) return;
    if (html_seg(e)) return;
    if (e->d->texto_n >= U_HTML_MAX_TXT) {
        e->d->truncado = 1;
        return;
    }
    e->d->texto[e->d->texto_n++] = (char)c;
    e->d->seg[e->cur].len++;
    if (e->d->seg[e->cur].len >= 2048)
        html_seg_cierra(e);
}

/* Apila estilo inline (0 si pila llena: se ignora con aviso). */
static void html_empuja(html_st_t *e, int tag) {
    int b, i, en;
    if (e->npila <= 0) {
        b = 0;
        i = 0;
        en = -1;
    } else {
        b = e->pila_b[e->npila - 1];
        i = e->pila_i[e->npila - 1];
        en = e->pila_e[e->npila - 1];
    }
    if (tag == HT_B) b = 1;
    else if (tag == HT_I) i = 1;
    if (e->npila >= U_HTML_MAX_PILA) {
        e->d->truncado = 1;
        return;
    }
    e->pila_b[e->npila] = b;
    e->pila_i[e->npila] = i;
    e->pila_e[e->npila] = en;
    e->pila_t[e->npila] = tag;
    e->npila++;
    html_seg_cierra(e);
}

/* Etiqueta de apertura ya identificada (con atributos). */
static void html_abre(html_st_t *e, int tag, int nh,
                      const unsigned char *pg, int ini, int fin) {
    int k;
    unsigned char hv[U_HTML_URL];
    int hn;
    unsigned char sv[U_HTML_URL];
    int sn;
    unsigned char av[U_HTML_ALT];
    if (tag == HT_H) {
        int fg, bg, bo, it;
        if (nh < 1 || nh > 6) return;
        html_bloque(e, U_HTML_B_H1 + (nh - 1), nh, -1);
        css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
        nest_mete(e, tag, fg, bg);
        if (e->nest > 0) {
            e->st_b[e->nest - 1] = bo;
            e->st_i[e->nest - 1] = it;
        }
        return;
    }
    if (tag == HT_P || tag == HT_DIV || tag == HT_BODY ||
        tag == HT_HTML || tag == HT_HEAD) {
        int fg, bg, bo, it;
        html_bloque(e, U_HTML_B_P, 0, -1);
        css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
        nest_mete(e, tag, fg, bg);
        if (e->nest > 0) {
            e->st_b[e->nest - 1] = bo;
            e->st_i[e->nest - 1] = it;
        }
        return;
    }
    if (tag == HT_BR) {
        html_bloque(e, U_HTML_B_BR, 0, -1);
        html_bloque_cierra(e);
        return;
    }
    if (tag == HT_HR) {
        html_bloque(e, U_HTML_B_HR, 0, -1);
        html_bloque_cierra(e);
        return;
    }
    if (tag == HT_UL || tag == HT_OL) {
        int fg, bg, bo, it;
        html_bloque_cierra(e);
        css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
        nest_mete(e, tag, fg, bg);
        if (e->nest > 0) {
            e->st_b[e->nest - 1] = bo;
            e->st_i[e->nest - 1] = it;
        }
        if (e->lista < 8) {
            e->ord[e->lista] = (tag == HT_OL);
            e->nnum[e->lista] = 1;
            e->lista++;
        } else {
            e->d->truncado = 1;
        }
        return;
    }
    if (tag == HT_LI) {
        int nv = e->lista > 8 ? 8 : e->lista;
        int lvl;
        if (e->lista > 0 && e->ord[e->lista - 1])
            nv = -nv;
        lvl = nv < 0 ? -nv : nv;
        html_bloque(e, U_HTML_B_LI, nv, -1);
        {
            int fg, bg, bo, it;
            css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
            nest_mete(e, tag, fg, bg);
            if (e->nest > 0) {
                e->st_b[e->nest - 1] = bo;
                e->st_i[e->nest - 1] = it;
            }
        }
        if (nv == 0) {
            html_txt_byte(e, (unsigned char)'\xE2');
            html_txt_byte(e, (unsigned char)'\x80');
            html_txt_byte(e, (unsigned char)'\xA2');
            html_txt_byte(e, ' ');
        } else if (nv < 0) {
            char nb[12];
            int q2 = 0, m2;
            u_itoa(e->nnum[lvl - 1], nb);
            e->nnum[lvl - 1]++;
            while (nb[q2]) {
                html_txt_byte(e, (unsigned char)nb[q2]);
                q2++;
            }
            m2 = 0;
            while (m2 < 1) {
                html_txt_byte(e, '.');
                m2++;
            }
            html_txt_byte(e, ' ');
        } else {
            html_txt_byte(e, (unsigned char)'\xE2');
            html_txt_byte(e, (unsigned char)'\x80');
            html_txt_byte(e, (unsigned char)'\xA2');
            html_txt_byte(e, ' ');
        }
        return;
    }
    if (tag == HT_B || tag == HT_I || tag == HT_SPAN ||
        tag == HT_BUTTON) {
        int fg, bg, bo, it;
        html_empuja(e, tag);
        css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
        if (bo && e->npila > 0) e->pila_b[e->npila - 1] = 1;
        if (it && e->npila > 0) e->pila_i[e->npila - 1] = 1;
        nest_mete(e, tag, fg, bg);
        if (e->nest > 0) {
            e->st_b[e->nest - 1] = bo;
            e->st_i[e->nest - 1] = it;
        }
        return;
    }
    if (tag == HT_A) {
        int idx = -1;
        hn = html_attr(pg, ini, fin, "href", hv, U_HTML_URL);
        if (hn >= 0) {
            char abs[U_HTML_URL];
            if (u_html_resuelve_url(e->base, (const char *)hv,
                    abs) == 0) {
                if (e->d->nenl < U_HTML_MAX_ENL) {
                    idx = e->d->nenl;
                    u_strcpy(e->d->enl[idx].url, abs);
                    e->d->enl[idx].ini = e->d->texto_n;
                    e->d->enl[idx].len = -1;
                    e->d->nenl++;
                } else {
                    e->d->truncado = 1;
                }
            }
        }
        html_empuja(e, HT_A);
        if (idx >= 0)
            e->pila_e[e->npila - 1] = idx;
        {
            int fg, bg, bo, it;
            css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
            if (bo && e->npila > 0) e->pila_b[e->npila - 1] = 1;
            if (it && e->npila > 0) e->pila_i[e->npila - 1] = 1;
            nest_mete(e, tag, fg, bg);
            if (e->nest > 0) {
                e->st_b[e->nest - 1] = bo;
                e->st_i[e->nest - 1] = it;
            }
        }
        html_seg_cierra(e);
        return;
    }
    if (tag == HT_IMG) {
        int gi = -1;
        sn = html_attr(pg, ini, fin, "src", sv, U_HTML_URL);
        html_attr(pg, ini, fin, "alt", av, U_HTML_ALT);
        if (e->d->nimg < U_HTML_MAX_IMG) {
            char asrc[U_HTML_URL];
            asrc[0] = 0;
            if (sn >= 0)
                u_html_resuelve_url(e->base, (const char *)sv,
                    asrc);
            gi = e->d->nimg;
            u_strcpy(e->d->img[gi].src, asrc);
            {
                int q = 0;
                while (av[q] && q < U_HTML_ALT - 1) {
                    e->d->img[gi].alt[q] = (char)av[q];
                    q++;
                }
                e->d->img[gi].alt[q] = 0;
            }
            e->d->nimg++;
        } else {
            e->d->truncado = 1;
        }
        if (gi >= 0) {
            html_bloque(e, U_HTML_B_IMG, 0, gi);
            html_bloque_cierra(e);
        }
        return;
    }
    if (tag == HT_TITLE) {
        e->titulo = 1;
        return;
    }
    if (tag == HT_STYLE) {
        e->css = 1;   /* el texto va al buffer CSS (no a salto) */
        e->css_n = 0;
        return;
    }
    if (tag == HT_SCRIPT || tag == HT_OBJECT || tag == HT_SELECT ||
        tag == HT_TEXTAREA) {
        e->salto = tag;
        return;
    }
    if (tag == HT_FORM) {
        unsigned char av[U_HTML_URL];
        unsigned char mv[16];
        int fg, bg, bo, it;
        e->en_forma = 1;
        e->f_ok = 0;
        e->f_campo[0] = 0;
        e->f_met = 0;
        if (html_attr(pg, ini, fin, "action", av, U_HTML_URL) >= 0 &&
            av[0]) {
            char abs[U_HTML_URL];
            if (u_html_resuelve_url(e->base, (const char *)av,
                    abs) == 0) {
                int q = 0;
                while (abs[q] && q < U_HTML_URL - 1) {
                    e->f_acc[q] = abs[q];
                    q++;
                }
                e->f_acc[q] = 0;
                e->f_ok = 1;
            }
        }
        if (html_attr(pg, ini, fin, "method", mv, 16) >= 0 && mv[0]) {
            if (!(mv[0] == 'g' || mv[0] == 'G')) e->f_met = 1;
        }
        css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
        nest_mete(e, tag, fg, bg);
        if (e->nest > 0) {
            e->st_b[e->nest - 1] = bo;
            e->st_i[e->nest - 1] = it;
        }
        return;
    }
    if (tag == HT_INPUT) {
        unsigned char nv[64];
        unsigned char tv[16];
        unsigned char vv[128];
        if (!e->en_forma) return;
        if (html_attr(pg, ini, fin, "type", tv, 16) < 0) tv[0] = 0;
        if (html_attr(pg, ini, fin, "name", nv, 64) < 0) nv[0] = 0;
        html_attr(pg, ini, fin, "value", vv, 128);
        /* Solo texto (por defecto) y sin campo aun: es el buscador. */
        if (!e->f_campo[0] && nv[0] &&
            (tv[0] == 0 || css_igual((const char *)tv, "text") ||
             css_igual((const char *)tv, "search"))) {
            int q = 0;
            while (nv[q] && q < 63) {
                e->f_campo[q] = (char)nv[q];
                q++;
            }
            e->f_campo[q] = 0;
        }
        return;
    }
    /* Desconocida: contenido igual, pero el estilo aplica. */
    {
        int fg, bg, bo, it;
        css_elem(e, tag, nh, pg, ini, fin, &fg, &bg, &bo, &it);
        nest_mete(e, tag, fg, bg);
        if (e->nest > 0) {
            e->st_b[e->nest - 1] = bo;
            e->st_i[e->nest - 1] = it;
        }
    }
    for (k = 0; k < e->npila; k++) {
        (void)k;
    }
}

/* Etiqueta de cierre ya identificada. */
static void html_cierra(html_st_t *e, int tag, int nh) {
    int i, h;
    (void)nh;
    if (tag == HT_TITLE) {
        e->titulo = 0;
        return;
    }
    if (tag == HT_H || tag == HT_P || tag == HT_DIV ||
        tag == HT_LI || tag == HT_UL || tag == HT_OL ||
        tag == HT_BODY || tag == HT_HTML || tag == HT_HEAD) {
        if (tag == HT_UL || tag == HT_OL) {
            html_bloque_cierra(e);
            if (e->lista > 0) e->lista--;
            nest_saca(e, tag);
            return;
        }
        html_bloque_cierra(e);
        nest_saca(e, tag);
        return;
    }
    if (tag == HT_BR || tag == HT_HR || tag == HT_IMG ||
        tag == HT_INPUT)
        return;
    if (tag == HT_STYLE) {
        e->css = 0;
        css_parse(e);
        return;
    }
    if (tag == HT_FORM) {
        if (e->en_forma && e->f_ok && e->f_campo[0]) {
            int q = 0;
            while (e->f_acc[q] && q < U_HTML_URL - 1) {
                e->d->forma[q] = e->f_acc[q];
                q++;
            }
            e->d->forma[q] = 0;
            q = 0;
            while (e->f_campo[q] && q < 63) {
                e->d->campo[q] = e->f_campo[q];
                q++;
            }
            e->d->campo[q] = 0;
            if (e->f_met)
                e->d->forma_post = 1;
            else
                e->d->hay_forma = 1;
        }
        e->en_forma = 0;
        nest_saca(e, tag);
        return;
    }
    h = -1;
    for (i = e->npila - 1; i >= 0; i--) {
        if (e->pila_t[i] == tag) {
            h = i;
            break;
        }
    }
    nest_saca(e, tag);
    if (h < 0) return;
    for (i = e->npila - 1; i >= h; i--) {
        if (e->pila_e[i] >= 0 && e->pila_e[i] < e->d->nenl &&
            e->d->enl[e->pila_e[i]].len < 0)
            e->d->enl[e->pila_e[i]].len =
                e->d->texto_n - e->d->enl[e->pila_e[i]].ini;
    }
    html_seg_cierra(e);
    e->npila = h;
}

/* Busca atributo nom en el tag pg[ini..fin) (ya en minusculas).
   Lo decodifica (entidades+UTF-8) en sal (cap). Devuelve long
   (0 puede ser vacio) o -1 si no existe. */
static int html_attr(const unsigned char *pg, int ini, int fin,
                     const char *nom, unsigned char *sal, int cap) {
    int p = ini, t = 0, q;
    sal[0] = 0;
    while (p < fin) {
        char an[40];
        int an_n = 0;
        while (p < fin &&
            (pg[p] == ' ' || pg[p] == '\t' || pg[p] == '\r' ||
             pg[p] == '\n'))
            p++;
        if (p < fin && pg[p] == '/') p++;
        if (p >= fin || pg[p] == '>') break;
        while (p < fin && an_n < 39 &&
            ((pg[p] >= 'a' && pg[p] <= 'z') ||
             (pg[p] >= '0' && pg[p] <= '9') || pg[p] == '-' ||
             pg[p] == '_' || pg[p] == ':')) {
            an[an_n++] = (char)pg[p++];
        }
        an[an_n] = 0;
        if (!an_n) {
            while (p < fin && pg[p] != ' ' && pg[p] != '\t' &&
                pg[p] != '>' && pg[p] != '=')
                p++;
            continue;
        }
        while (p < fin && (pg[p] == ' ' || pg[p] == '\t')) p++;
        if (p < fin && pg[p] == '=') {
            unsigned char tmp[U_HTML_URL + 16];
            int tn = 0;
            int com = 0;
            int cerrado = 0;
            p++;
            while (p < fin && (pg[p] == ' ' || pg[p] == '\t')) p++;
            if (p < fin && (pg[p] == '"' || pg[p] == '\'')) {
                com = pg[p];
                p++;
            }
            while (p < fin && tn < U_HTML_URL + 15) {
                if (com) {
                    if (pg[p] == com) {
                        p++;
                        cerrado = 1;
                        break;
                    }
                } else if (pg[p] == ' ' || pg[p] == '\t' ||
                    pg[p] == '\r' || pg[p] == '\n' ||
                    pg[p] == '>') {
                    break;
                }
                tmp[tn++] = pg[p++];
            }
            if (com && !cerrado) {
                while (p < fin && pg[p] != com && pg[p] != '>')
                    p++;
                if (p < fin && pg[p] == com) p++;
            }
            if (!u_strcmp(an, nom)) {
                int k = 0;
                t = 0;
                while (k < tn && t < cap - 1) {
                    if (tmp[k] == '&') {
                        unsigned char dec[4];
                        int cs, dl;
                        cs = html_entidad(tmp + k, tn - k, dec);
                        if (cs < 1) cs = 1;
                        if (dec[0] < 0x80) dl = 1;
                        else if ((dec[0] & 0xE0) == 0xC0) dl = 2;
                        else if ((dec[0] & 0xF0) == 0xE0) dl = 3;
                        else dl = 4;
                        {
                            int m = 0;
                            while (m < dl && t < cap - 1) {
                                sal[t++] = dec[m++];
                            }
                        }
                        k += cs;
                    } else {
                        sal[t++] = tmp[k++];
                    }
                }
                {
                    char cvt[U_HTML_ALT + U_HTML_URL];
                    int cn;
                    if (t >= cap) t = cap - 1;
                    sal[t] = 0;
                    cn = html_utf8_copia(sal, t, cvt,
                        (int)sizeof(cvt));
                    q = 0;
                    while (q < cn && q < cap - 1) {
                        sal[q] = (unsigned char)cvt[q];
                        q++;
                    }
                    sal[q] = 0;
                    return q;
                }
            }
        } else if (!u_strcmp(an, nom)) {
            sal[0] = 0;
            return 0;
        }
    }
    return -1;
}

int u_html_resuelve_url(const char *base, const char *ref,
                        char *sal) {
    u_http_url_t u;
    const char *p;
    int t;
    if (!base || !ref || !sal) return -1;
    while (*ref == ' ' || *ref == '\t') ref++;
    p = ref + u_strlen(ref);
    while (p > ref &&
        (*(p - 1) == ' ' || *(p - 1) == '\t' || *(p - 1) == '\r' ||
         *(p - 1) == '\n'))
        p--;
    if (p == ref) {
        if (!base[0] || u_http_url(base, &u)) return -3;
        u_strcpy(sal, base);
        return 0;
    }
    {
        int i = 0, esc = -1;
        while (ref + i < p) {
            if (ref[i] == ':') {
                esc = i;
                break;
            }
            if (ref[i] == '/' || ref[i] == '?' || ref[i] == '#')
                break;
            if ((unsigned char)ref[i] < 32 ||
                (unsigned char)ref[i] == 127)
                return -1;
            i++;
        }
        if (esc >= 0) {
            char esq[16];
            int q = 0;
            while (q < esc && q < 15) {
                esq[q] = (char)html_mini(ref[q]);
                q++;
            }
            esq[q] = 0;
            if (!u_strcmp(esq, "http")) {
                char tmp[U_HTML_URL + 32];
                int j = 0;
                while (ref + j < p && j < U_HTML_URL + 31) {
                    tmp[j] = ref[j];
                    j++;
                }
                tmp[j] = 0;
                {
                    int f = 0;
                    while (tmp[f] && tmp[f] != '#') f++;
                    tmp[f] = 0;
                }
                if (u_http_url(tmp, &u)) return -1;
                u_strcpy(sal, tmp);
                return 0;
            }
            return -2;
        }
    }
    if (!base[0] || u_http_url(base, &u)) return -3;
    if (ref[0] == '#') {
        u_strcpy(sal, base);
        return 0;
    }
    if (ref[0] == '?') {
        t = 0;
        while (u.host[t]) {
            sal[t] = u.host[t];
            t++;
        }
        {
            int j = 0, q = t;
            char pre[600];
            int pt = 0;
            pre[pt++] = 'h';
            pre[pt++] = 't';
            pre[pt++] = 't';
            pre[pt++] = 'p';
            pre[pt++] = ':';
            pre[pt++] = '/';
            pre[pt++] = '/';
            while (u.host[j]) pre[pt++] = u.host[j++];
            if (u.puerto != 80) {
                char nb[12];
                int r2 = 0;
                pre[pt++] = ':';
                u_itoa((int)u.puerto, nb);
                while (nb[r2]) pre[pt++] = nb[r2++];
            }
            j = 0;
            while (u.ruta[j] && pt < 590) pre[pt++] = u.ruta[j++];
            j = 0;
            while (ref + j < p && pt < 599) pre[pt++] = ref[j++];
            pre[pt] = 0;
            if (u_http_url(pre, &u)) return -3;
            u_strcpy(sal, pre);
            (void)q;
            return 0;
        }
    }
    {
        char pre[600];
        int pt = 0, j;
        pre[pt++] = 'h';
        pre[pt++] = 't';
        pre[pt++] = 't';
        pre[pt++] = 'p';
        pre[pt++] = ':';
        pre[pt++] = '/';
        pre[pt++] = '/';
        j = 0;
        while (u.host[j] && pt < 590) pre[pt++] = u.host[j++];
        if (u.puerto != 80) {
            char nb[12];
            int r2 = 0;
            pre[pt++] = ':';
            u_itoa((int)u.puerto, nb);
            while (nb[r2] && pt < 590) pre[pt++] = nb[r2++];
        }
        if (ref[0] == '/') {
            j = 0;
            while (ref + j < p && ref[j] != '#' && pt < 599)
                pre[pt++] = ref[j++];
        } else {
            j = 0;
            while (u.ruta[j] && pt < 590) pre[pt++] = u.ruta[j++];
            while (pt > 8 && pre[pt - 1] != '/') pt--;
            j = 0;
            while (ref + j < p && ref[j] != '#' && pt < 599)
                pre[pt++] = ref[j++];
        }
        pre[pt] = 0;
        {
            char norm[600];
            int sgm[64];
            int ns = 0, w = 0, r3 = 0;
            int raiz = 0;
            while (pre[raiz] && pre[raiz] != ':' && raiz < 599)
                raiz++;
            if (pre[raiz] == ':' && pre[raiz + 1] == '/' &&
                pre[raiz + 2] == '/') {
                raiz += 3;
                while (pre[raiz] && pre[raiz] != '/' && raiz < 599)
                    raiz++;
            }
            w = 0;
            while (w < raiz && w < 599) {
                norm[w] = pre[w];
                w++;
            }
            norm[w] = 0;
            r3 = raiz;
            while (pre[r3] && ns < 64) {
                int f3 = r3;
                while (pre[f3] && pre[f3] != '/') f3++;
                {
                    int ln = f3 - r3;
                    if ((ln == 1 && pre[r3] == '.') ||
                        ln == 0) {
                        /* nada */
                    } else if (ln == 2 && pre[r3] == '.' &&
                        pre[r3 + 1] == '.') {
                        if (ns > 0) {
                            ns--;
                            w = sgm[ns];
                            norm[w] = 0;
                        }
                    } else {
                        if (w + ln + 1 < 599) {
                            sgm[ns++] = w;
                            norm[w++] = '/';
                            {
                                int z = 0;
                                while (z < ln) {
                                    norm[w++] = pre[r3 + z];
                                    z++;
                                }
                            }
                            norm[w] = 0;
                        }
                    }
                }
                r3 = pre[f3] ? f3 + 1 : f3;
            }
            if (w + 1 < 599) {
                norm[w] = 0;
            }
            if (u_http_url(norm[0] ? norm : pre, &u)) return -3;
            u_strcpy(sal, norm[0] ? norm : pre);
            return 0;
        }
    }
}

int u_html_crea(u_html_doc_t *d) {
    if (!d) return -1;
    d->texto = (char *)u_malloc(U_HTML_MAX_TXT);
    d->seg = (u_html_seg_t *)u_malloc(
        U_HTML_MAX_SEG * (int)sizeof(u_html_seg_t));
    d->bloq = (u_html_bloq_t *)u_malloc(
        U_HTML_MAX_BLOQ * (int)sizeof(u_html_bloq_t));
    d->enl = (u_html_enl_t *)u_malloc(
        U_HTML_MAX_ENL * (int)sizeof(u_html_enl_t));
    d->img = (u_html_img_t *)u_malloc(
        U_HTML_MAX_IMG * (int)sizeof(u_html_img_t));
    if (!d->texto || !d->seg || !d->bloq || !d->enl || !d->img) {
        if (d->texto) u_free(d->texto);
        if (d->seg) u_free(d->seg);
        if (d->bloq) u_free(d->bloq);
        if (d->enl) u_free(d->enl);
        if (d->img) u_free(d->img);
        d->texto = 0;
        d->seg = 0;
        d->bloq = 0;
        d->enl = 0;
        d->img = 0;
        return -1;
    }
    d->texto_n = 0;
    d->nseg = 0;
    d->nbloq = 0;
    d->nenl = 0;
    d->nimg = 0;
    d->titulo[0] = 0;
    d->truncado = 0;
    d->forma[0] = 0;
    d->campo[0] = 0;
    d->hay_forma = 0;
    d->forma_post = 0;
    {
        int qi;
        for (qi = 0; qi < U_HTML_MAX_IMG; qi++) d->img_fil[qi] = 0;
    }
    return 0;
}

void u_html_libera(u_html_doc_t *d) {
    if (!d) return;
    if (d->texto) u_free(d->texto);
    if (d->seg) u_free(d->seg);
    if (d->bloq) u_free(d->bloq);
    if (d->enl) u_free(d->enl);
    if (d->img) u_free(d->img);
    d->texto = 0;
    d->seg = 0;
    d->bloq = 0;
    d->enl = 0;
    d->img = 0;
    d->texto_n = 0;
    d->nseg = 0;
    d->nbloq = 0;
    d->nenl = 0;
    d->nimg = 0;
    d->titulo[0] = 0;
    d->truncado = 0;
    d->forma[0] = 0;
    d->campo[0] = 0;
    d->hay_forma = 0;
    d->forma_post = 0;
    {
        int qi;
        for (qi = 0; qi < U_HTML_MAX_IMG; qi++) d->img_fil[qi] = 0;
    }
}

/* Inicializa el estado del parser (comun legacy/Hubbub): mismo
   punto de partida para que ambos rindan igual. */
static void html_ini(html_st_t *e, u_html_doc_t *d, const char *base) {
    e->d = d;
    e->base = base;
    e->npila = 0;
    e->nest = 0;
    e->css = 0;
    e->css_n = 0;
    e->nreg = 0;
    e->en_forma = 0;
    e->f_acc[0] = 0;
    e->f_campo[0] = 0;
    e->f_met = 0;
    e->f_ok = 0;
    e->cur = -1;
    e->curb = -1;
    e->titulo = 0;
    e->salto = 0;
    e->lista = 0;
    {
        int z = 0;
        while (z < 8) {
            e->ord[z] = 0;
            e->nnum[z] = 1;
            z++;
        }
    }
    d->texto_n = 0;
    d->nseg = 0;
    d->nbloq = 0;
    d->nenl = 0;
    d->nimg = 0;
    d->titulo[0] = 0;
    d->truncado = 0;
    d->forma[0] = 0;
    d->campo[0] = 0;
    d->hay_forma = 0;
    d->forma_post = 0;
    {
        int qi;
        for (qi = 0; qi < U_HTML_MAX_IMG; qi++) d->img_fil[qi] = 0;
    }
}

int u_html_parse(u_html_doc_t *d, const unsigned char *pg, int n,
                 const char *base) {
    html_st_t e;
    int p = 0;
    if (!d || !d->texto || !pg || n < 0 || !base) return -1;
    html_ini(&e, d, base);
    if (n <= 0) return 0;
    while (p < n) {
        /* Contenido ignorado (script/style/object). */
        if (e.salto) {
            int q = p, id = e.salto;
            while (q < n) {
                if (pg[q] == '<' && q + 2 < n && pg[q + 1] == '/') {
                    int r3 = q + 2, s3 = 0;
                    char nb[16];
                    while (r3 < n && s3 < 15 &&
                        ((pg[r3] >= 'a' && pg[r3] <= 'z') ||
                         (pg[r3] >= 'A' && pg[r3] <= 'Z'))) {
                        nb[s3++] = (char)html_mini(pg[r3]);
                        r3++;
                    }
                    nb[s3] = 0;
                    {
                        int got = html_tag_id(nb, s3,
                            &s3) == id;
                        if (got) {
                            while (r3 < n && pg[r3] != '>') r3++;
                            p = r3 < n ? r3 + 1 : n;
                            e.salto = 0;
                            break;
                        }
                    }
                }
                q++;
            }
            if (e.salto) p = n;
            continue;
        }
        if (pg[p] != '<') {
            /* Texto hasta el proximo '<' (con entidades). */
            int q = p;
            while (q < n && pg[q] != '<') q++;
            while (p < q) {
                if (pg[p] == '&') {
                    unsigned char dec[4];
                    int cs = html_entidad(pg + p, q - p, dec);
                    int dl;
                    if (cs < 1) cs = 1;
                    if (dec[0] < 0x80) dl = 1;
                    else if ((dec[0] & 0xE0) == 0xC0) dl = 2;
                    else if ((dec[0] & 0xF0) == 0xE0) dl = 3;
                    else dl = 4;
                    {
                        int m = 0;
                        while (m < dl) {
                            html_txt_byte(&e, dec[m++]);
                        }
                    }
                    p += cs;
                } else if ((pg[p] & 0x80) == 0) {
                    if (pg[p] == '\r' || pg[p] == '\n' ||
                        pg[p] == '\t')
                        html_txt_byte(&e, ' ');
                    else
                        html_txt_byte(&e, pg[p]);
                    p++;
                } else {
                    int q2 = p + 1;
                    while (q2 < q && q2 < p + 4 &&
                        (pg[q2] & 0xC0) == 0x80)
                        q2++;
                    {
                        char cvt[8];
                        int cn = html_utf8_copia(pg + p, q2 - p,
                            cvt, 5);
                        int m = 0;
                        if (!cn) {
                            html_txt_byte(&e, 0xEF);
                            html_txt_byte(&e, 0xBF);
                            html_txt_byte(&e, 0xBD);
                            p++;
                        } else {
                            while (m < cn) {
                                html_txt_byte(&e,
                                    (unsigned char)cvt[m++]);
                            }
                            p += q2 - p;
                        }
                    }
                }
            }
            continue;
        }
        /* Tag o texto literal. */
        if (p + 1 < n && pg[p + 1] == '!') {
            /* Comentario o doctype: hasta '>'. */
            int q = p + 2;
            if (q + 1 < n && pg[q] == '-' && pg[q + 1] == '-') {
                q += 2;
                while (q + 2 < n && !(pg[q] == '-' &&
                        pg[q + 1] == '-' && pg[q + 2] == '>'))
                    q++;
                p = (q + 2 < n) ? q + 3 : n;
            } else {
                while (q < n && pg[q] != '>') {
                    if (q - p > 512) break;
                    q++;
                }
                p = q < n ? q + 1 : n;
            }
            continue;
        }
        if (p + 1 < n && pg[p + 1] == '?') {
            int q = p + 2;
            while (q < n && pg[q] != '>') {
                if (q - p > 512) break;
                q++;
            }
            p = q < n ? q + 1 : n;
            continue;
        }
        {
            int q = p + 1, cierre = 0;
            if (q < n && pg[q] == '/') {
                cierre = 1;
                q++;
            }
            while (q < n &&
                (pg[q] == ' ' || pg[q] == '\t' || pg[q] == '\r' ||
                 pg[q] == '\n'))
                q++;
            {
                int r3 = q;
                char nb[24];
                int nn = 0;
                while (r3 < n && nn < 23 &&
                    ((pg[r3] >= 'a' && pg[r3] <= 'z') ||
                     (pg[r3] >= 'A' && pg[r3] <= 'Z') ||
                     (pg[r3] >= '0' && pg[r3] <= '9'))) {
                    nb[nn++] = (char)html_mini(pg[r3]);
                    r3++;
                }
                if (!nn) {
                    html_txt_byte(&e, '<');
                    p++;
                    continue;
                }
                {
                    int f3 = r3, id, nh = 0, selfclose = 0;
                    while (f3 < n && pg[f3] != '>') {
                        if (f3 - p > 512) break;
                        f3++;
                    }
                    if (f3 >= n || pg[f3] != '>') {
                        html_txt_byte(&e, '<');
                        p++;
                        continue;
                    }
                    {
                        int f4 = f3 - 1;
                        while (f4 > r3 && (pg[f4] == ' ' ||
                                pg[f4] == '\t'))
                            f4--;
                        if (pg[f4] == '/') selfclose = 1;
                    }
                    id = html_tag_id(nb, nn, &nh);
                    if (cierre) {
                        html_cierra(&e, id, nh);
                    } else {
                        html_abre(&e, id, nh, pg, r3, f3);
                        if (selfclose)
                            html_cierra(&e, id, nh);
                    }
                    p = f3 + 1;
                }
            }
        }
    }
    html_bloque_cierra(&e);
    if (d->truncado) return 1;
    return 0;
}

#if defined(F32_NSHUBBUB)
#include <stdint.h>
#include <stddef.h>
#include <hubbub/parser.h>
#include <hubbub/types.h>

/* Puente Hubbub -> constructor de bloques (v2.4.3+hubbub).
   Hubbub tokeniza HTML5 de verdad (entidades, comentarios, doctype,
   rawtext/CDATA); la construccion de bloques/enlaces/formularios/
   CSS es la de siempre (html_abre/html_cierra/html_txt_byte), asi
   que el layout no cambia. Sin DOM intermedio ni GC extra. */
typedef struct {
    html_st_t e;
    hubbub_parser *ps;
    int salta;   /* id HT_* en salto (script/object/select/textarea), -1 no */
} hb_t;

static void hb_error(uint32_t ln, uint32_t col, const char *m, void *pw) {
    (void)ln;
    (void)col;
    (void)m;
    (void)pw;
}

/* Texto ya decodificado por Hubbub: mismo tratamiento que el legado
   pero sin re-decodificar entidades. */
static void hb_texto(html_st_t *e, const uint8_t *s, unsigned long n) {
    unsigned long p = 0;
    while (p < n) {
        unsigned char c = s[p];
        if (c < 0x80) {
            if (c == 0) {
                p++;
                continue;
            }
            if (c == '\r' || c == '\n' || c == '\t')
                html_txt_byte(e, ' ');
            else
                html_txt_byte(e, c);
            p++;
        } else {
            unsigned long q = p + 1;
            while (q < n && q < p + 4 && (s[q] & 0xC0) == 0x80) q++;
            {
                char cvt[8];
                int cn = html_utf8_copia(s + p, (int)(q - p), cvt, 5);
                int m = 0;
                if (!cn) {
                    html_txt_byte(e, 0xEF);
                    html_txt_byte(e, 0xBF);
                    html_txt_byte(e, 0xBF);
                    p++;
                } else {
                    while (m < cn) {
                        html_txt_byte(e, (unsigned char)cvt[m]);
                        m++;
                    }
                    p = q;
                }
            }
        }
    }
}

/* Nombre de etiqueta en minusculas (como el scanner legado). */
static int hb_nom(char *d, const uint8_t *s, unsigned long n) {
    unsigned long i = 0;
    int t = 0;
    while (i < n && t < 23) {
        unsigned c = s[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) break;
        d[t++] = (char)c;
        i++;
    }
    d[t] = 0;
    return t;
}

/* Copia con escape para re-sintetizar el tag (" & < >). */
static int hb_esc(char *d, int cap, int t, const uint8_t *s,
                  unsigned long n) {
    unsigned long i;
    for (i = 0; i < n && t < cap; i++) {
        unsigned char c = s[i];
        const char *rep = 0;
        if (c == '&') rep = "&amp;";
        else if (c == '<') rep = "&lt;";
        else if (c == '>') rep = "&gt;";
        else if (c == '"') rep = "&quot;";
        if (rep) {
            int k = 0;
            while (rep[k] && t < cap) d[t++] = rep[k++];
        } else if (c >= 32 || c == '\t' || c >= 0x80) {
            d[t++] = (char)c;
        } else if (t < cap) {
            d[t++] = ' ';
        }
    }
    return t;
}

/* Re-sintetiza <nombre a="v"...> para reutilizar html_abre tal cual
   (incluye style/clase/href/action). Devuelve longitud. */
static int hb_sint(char *sal, int cap, const char *nom,
                   const hubbub_attribute *at, uint32_t na) {
    unsigned i;
    int t = 0, k;
    if (t < cap) sal[t++] = '<';
    for (k = 0; nom[k] && t < cap; k++) sal[t++] = nom[k];
    for (i = 0; i < na && t < cap - 2; i++) {
        int k2;
        sal[t++] = ' ';
        for (k2 = 0; k2 < 64; k2++) {
            unsigned char c;
            if ((unsigned)k2 >= at[i].name.len) break;
            c = at[i].name.ptr[k2];
            if (c >= 'A' && c <= 'Z') c += (unsigned char)('a' - 'A');
            if (t < cap) sal[t++] = (char)c;
        }
        if (t + 2 < cap) {
            sal[t++] = '=';
            sal[t++] = '"';
        }
        t = hb_esc(sal, cap - 1, t, at[i].value.ptr, at[i].value.len);
        if (t < cap) sal[t++] = '"';
    }
    if (t < cap) sal[t++] = '>';
    if (t < cap)
        sal[t] = 0;
    else if (cap > 0)
        sal[cap - 1] = 0;
    return t;
}

static hubbub_error hb_tok(const hubbub_token *tk, void *pw) {
    hb_t *h = (hb_t *)pw;
    html_st_t *e = &h->e;
    char nb[24];
    int nn, nh = 0, id;
    hubbub_parser_optparams prm;
    if (!tk) return HUBBUB_OK;
    if (tk->type == HUBBUB_TOKEN_DOCTYPE ||
        tk->type == HUBBUB_TOKEN_COMMENT ||
        tk->type == HUBBUB_TOKEN_EOF)
        return HUBBUB_OK;
    if (tk->type == HUBBUB_TOKEN_CHARACTER) {
        if (h->salta < 0)
            hb_texto(e, tk->data.character.ptr, tk->data.character.len);
        return HUBBUB_OK;
    }
    nn = hb_nom(nb, tk->data.tag.name.ptr, tk->data.tag.name.len);
    id = html_tag_id(nb, nn, &nh);
    if (tk->type == HUBBUB_TOKEN_END_TAG) {
        if (h->salta >= 0) {
            if (id == h->salta) {
                h->salta = -1;
                prm.content_model.model = HUBBUB_CONTENT_MODEL_PCDATA;
                hubbub_parser_setopt(h->ps, HUBBUB_PARSER_CONTENT_MODEL,
                                     &prm);
            }
            return HUBBUB_OK;
        }
        html_cierra(e, id, nh);
        return HUBBUB_OK;
    }
    if (h->salta >= 0) return HUBBUB_OK;
    if (id == HT_SCRIPT || id == HT_OBJECT || id == HT_SELECT ||
        id == HT_TEXTAREA) {
        h->salta = id;
        prm.content_model.model = HUBBUB_CONTENT_MODEL_CDATA;
        hubbub_parser_setopt(h->ps, HUBBUB_PARSER_CONTENT_MODEL, &prm);
        return HUBBUB_OK;
    }
    if (id == HT_STYLE || id == HT_TITLE) {
        prm.content_model.model = id == HT_STYLE ?
            HUBBUB_CONTENT_MODEL_CDATA : HUBBUB_CONTENT_MODEL_RCDATA;
        hubbub_parser_setopt(h->ps, HUBBUB_PARSER_CONTENT_MODEL, &prm);
    }
    {
        char syn[2048];
        int sl = hb_sint(syn, (int)sizeof(syn), nb,
                         tk->data.tag.attributes, tk->data.tag.n_attributes);
        html_abre(e, id, nh, (const unsigned char *)syn, 0, sl);
        if (tk->data.tag.self_closing) html_cierra(e, id, nh);
    }
    return HUBBUB_OK;
}

/* Igual contrato que u_html_parse, pero tokenizado con Hubbub
   (HTML5 real). -1 args, -2 sin memoria/Hubbub (reintentar con
   u_html_parse), 0/1 como el original (1 = truncado). */
int u_html_parse_hb(u_html_doc_t *d, const unsigned char *pg, int n,
                    const char *base) {
    hb_t h;
    hubbub_parser *ps = 0;
    hubbub_parser_optparams prm;
    hubbub_error r;
    if (!d || !d->texto || !pg || n < 0 || !base) return -1;
    html_ini(&h.e, d, base);
    if (n <= 0) return 0;
    h.ps = 0;
    h.salta = -1;
    r = hubbub_parser_create(0, 0, &ps);
    if (r != HUBBUB_OK || !ps) return -2;
    h.ps = ps;
    prm.token_handler.handler = hb_tok;
    prm.token_handler.pw = &h;
    r = hubbub_parser_setopt(ps, HUBBUB_PARSER_TOKEN_HANDLER, &prm);
    if (r == HUBBUB_OK) {
        prm.error_handler.handler = hb_error;
        prm.error_handler.pw = &h;
        r = hubbub_parser_setopt(ps, HUBBUB_PARSER_ERROR_HANDLER, &prm);
    }
    if (r == HUBBUB_OK)
        r = hubbub_parser_parse_chunk(ps, pg, (unsigned)n);
    if (r == HUBBUB_OK)
        r = hubbub_parser_completed(ps);
    hubbub_parser_destroy(ps);
    h.ps = 0;
    if (r != HUBBUB_OK) {
        d->truncado = 1;
        return -2;
    }
    html_bloque_cierra(&h.e);
    return d->truncado ? 1 : 0;
}
#endif

static int html_ancho(const u_html_doc_t *d, int ini, int len) {
    int i = 0, c = 0;
    while (i < len) {
        unsigned char b = (unsigned char)d->texto[ini + i];
        if ((b & 0xC0) != 0x80) c++;
        i++;
    }
    return c;
}

int u_html_maqueta(const u_html_doc_t *d, int cols,
                   u_html_fila_t *filas, int maxfilas) {
    int nf = 0, b;
    if (!d || cols < 10 || !filas || maxfilas <= 0) return -1;
    for (b = 0; b < d->nbloq && nf < maxfilas; b++) {
        const u_html_bloq_t *bl = &d->bloq[b];
        int s, sang = 0;
        if (b > 0 && nf < maxfilas) {
            filas[nf].ini = 0;
            filas[nf].len = 0;
            filas[nf].x = 0;
            filas[nf].estilo = U_HTML_E_VACIA;
                filas[nf].fg = -1;
                filas[nf].bg = -1;
            filas[nf].enlace = -1;
            filas[nf].img = -1;
            nf++;
        }
        if (nf >= maxfilas) break;
        if (bl->tipo == U_HTML_B_HR) {
            filas[nf].ini = 0;
            filas[nf].len = 0;
            filas[nf].x = 0;
            filas[nf].estilo = U_HTML_E_HR;
                filas[nf].fg = -1;
                filas[nf].bg = -1;
            filas[nf].enlace = -1;
            filas[nf].img = -1;
            nf++;
            continue;
        }
        if (bl->tipo == U_HTML_B_BR) {
            filas[nf].ini = 0;
            filas[nf].len = 0;
            filas[nf].x = 0;
            filas[nf].estilo = U_HTML_E_VACIA;
                filas[nf].fg = -1;
                filas[nf].bg = -1;
            filas[nf].enlace = -1;
            filas[nf].img = -1;
            nf++;
            continue;
        }
        if (bl->tipo == U_HTML_B_IMG) {
            int fr = 1, k;
            /* Con miniatura decodificada (img_fil>0) la imagen ocupa
               varias filas de texto (16 px cada una). */
            if (bl->img >= 0 && bl->img < U_HTML_MAX_IMG &&
                d->img_fil[bl->img] > 1)
                fr = d->img_fil[bl->img];
            if (fr > 16) fr = 16;
            for (k = 0; k < fr && nf < maxfilas; k++) {
                filas[nf].ini = 0;
                filas[nf].len = 0;
                filas[nf].x = 2;
                filas[nf].estilo = U_HTML_E_IMG;
                filas[nf].fg = -1;
                filas[nf].bg = -1;
                filas[nf].enlace = -1;
                filas[nf].img = bl->img;
                nf++;
            }
            continue;
        }
        if (bl->tipo == U_HTML_B_LI) {
            int nv = bl->nivel < 0 ? -bl->nivel : bl->nivel;
            if (nv < 0) nv = 0;
            if (nv > 8) nv = 8;
            sang = 2 + nv * 2;
        }
        for (s = bl->seg_ini;
            s < bl->seg_ini + bl->seg_n && nf < maxfilas;
            s++) {
            const u_html_seg_t *sg = &d->seg[s];
            int off = sg->ini, rest = sg->len;
            int est = sg->est;
            int x = sang;
            if (bl->tipo >= U_HTML_B_H1 &&
                bl->tipo <= U_HTML_B_H6 && est != U_HTML_E_ENLACE)
                est = U_HTML_E_TITULO;
            if (rest <= 0) continue;
            while (rest > 0 && nf < maxfilas) {
                int w0 = off, wl = 0;
                int av = cols - x;
                if (av < 4) {
                    if (x == sang) {
                        filas[nf].ini = off;
                        filas[nf].len = 0;
                        filas[nf].x = x;
                        filas[nf].estilo = U_HTML_E_VACIA;
                filas[nf].fg = -1;
                filas[nf].bg = -1;
                        filas[nf].enlace = -1;
                        filas[nf].img = -1;
                        nf++;
                        if (nf >= maxfilas) break;
                    }
                    x = sang;
                    av = cols - x;
                }
                while (wl < rest) {
                    int wn = 0;
                    while (w0 + wl + wn < off + rest &&
                        d->texto[w0 + wl + wn] != ' ')
                        wn++;
                    if (!wn) break;
                    if (html_ancho(d, w0, wl + wn) <= av) {
                        wl += wn;
                        if (w0 + wl < off + rest &&
                            d->texto[w0 + wl] == ' ')
                            wl++;
                    } else if (!wl) {
                        int take = 0, cw2 = 0;
                        while (take < wn && cw2 < av) {
                            unsigned char bb =
                                (unsigned char)d->texto[w0 + take];
                            if ((bb & 0xC0) != 0x80) {
                                if (cw2 + 1 > av) break;
                                cw2++;
                            }
                            take++;
                        }
                        if (!take) take = 1;
                        wl = take;
                        if (w0 + wl < off + rest &&
                            d->texto[w0 + wl] == ' ')
                            wl++;
                    } else {
                        break;
                    }
                }
                if (!wl) {
                    if (d->texto[w0] == ' ') {
                        off++;
                        rest--;
                        continue;
                    }
                    break;
                }
                filas[nf].ini = w0;
                filas[nf].len = wl;
                filas[nf].x = x;
                filas[nf].estilo = est;
                filas[nf].enlace = sg->enlace;
                filas[nf].img = -1;
                filas[nf].fg = sg->fg;
                filas[nf].bg = sg->bg;
                nf++;
                x += html_ancho(d, w0, wl);
                off = w0 + wl;
                rest = (sg->ini + sg->len) - off;
            }
        }
        if ((bl->tipo == U_HTML_B_H1 || bl->tipo == U_HTML_B_H2) &&
            nf < maxfilas) {
            filas[nf].ini = 0;
            filas[nf].len = 0;
            filas[nf].x = 0;
            filas[nf].estilo = U_HTML_E_HR;
                filas[nf].fg = -1;
                filas[nf].bg = -1;
            filas[nf].enlace = -1;
            filas[nf].img = -1;
            nf++;
        }
    }
    return nf;
}

/* ---- F7-D: imagenes BMP/GIF (miniatura RGB para fbrowser) ----
   Sin libc: solo u_* y bucles acotados. BMP BI_RGB (1/4/8/24/32)
   y GIF87a/89a (primer frame, entrelace si/no, transparencia =
   negro). Todo truncado/comprimido ajeno -> error, nunca cuelgue.
   La salida es miniatura encajada en U_IMG_BOX_* (sin ampliar). */

static unsigned img_le16(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned img_le32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void u_img_lib(u_img_t *o) {
    if (!o) return;
    if (o->px) u_free(o->px);
    o->px = 0;
    o->w = o->h = 0;
}

/* Reduce full[w*h] a la caja (sin ampliar) en o (px en heap). */
static int img_mini(const unsigned char *full, int w, int h,
                    u_img_t *o) {
    int tw = w, th = h;
    unsigned char *px;
    int x, y;
    if (!full || w < 1 || h < 1 || !o) return -1;
    if (tw > U_IMG_BOX_W) {
        th = th * U_IMG_BOX_W / tw;
        tw = U_IMG_BOX_W;
    }
    if (th > U_IMG_BOX_H) {
        tw = tw * U_IMG_BOX_H / th;
        th = U_IMG_BOX_H;
    }
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    px = (unsigned char *)u_malloc(tw * th * 3);
    if (!px) return -5;
    for (y = 0; y < th; y++) {
        int sy = y * h / th, k;
        for (x = 0; x < tw; x++) {
            int sx = x * w / tw;
            const unsigned char *f = full + (sy * w + sx) * 3;
            unsigned char *q = px + (y * tw + x) * 3;
            for (k = 0; k < 3; k++) q[k] = f[k];
        }
    }
    o->w = tw;
    o->h = th;
    o->px = px;
    return 0;
}

static int img_bmp(const unsigned char *d, int n, u_img_t *o) {
    unsigned doff, dib;
    int w, ah, top = 0, bpp;
    unsigned paloff = 0, entry = 0, ncols = 0;
    unsigned stride;
    unsigned char *full = 0;
    int x, y, q;
    if (n < 26) return -2;
    doff = img_le32(d + 10);
    dib = img_le32(d + 14);
    if (dib == 12) {
        if (n < 26) return -2;
        w = (int)img_le16(d + 18);
        ah = (int)img_le16(d + 20);
        if (img_le16(d + 22) != 1) return -2;
        bpp = (int)img_le16(d + 24);
        paloff = 26;
        entry = 3;
    } else if (dib == 40) {        long wl;
        int ih;
        if (n < 54) return -2;
        wl = (long)((int)img_le32(d + 18));
        ih = (int)img_le32(d + 22);
        if (wl < 1 || wl > U_IMG_MAX_DIM) return wl < 1 ? -2 : -4;
        if (ih == 0 || ih > U_IMG_MAX_DIM || ih < -U_IMG_MAX_DIM)
            return ih == 0 ? -2 : -4;
        if (img_le16(d + 26) != 1) return -2;
        bpp = (int)img_le16(d + 28);
        if (img_le32(d + 30) != 0) return -3;
        w = (int)wl;
        ah = ih < 0 ? -ih : ih;
        top = ih < 0 ? 1 : 0;
        paloff = 14 + 40;
        entry = 4;
        {
            unsigned usada = img_le32(d + 46);
            ncols = (unsigned)1 << bpp;
            if (bpp > 8) ncols = 0;
            if (bpp <= 8 && usada > ncols) ncols = usada;
        }
    } else {
        return -2;
    }
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)
        return -3;
    if (w < 1 || ah < 1) return -2;
    if (w > U_IMG_MAX_DIM || ah > U_IMG_MAX_DIM) return -4;
    if ((long)w * (long)ah > U_IMG_MAX_PX) return -4;
    if (bpp <= 8) {
        unsigned hay, minoff = dib == 12 ? 26u : 54u;
        if (doff < minoff || doff > (unsigned)n) return -2;
        hay = (doff - paloff) / entry;
        if (!ncols) ncols = (unsigned)1 << bpp;
        if (ncols > hay) ncols = hay;
        if (!ncols) return -2;
    }
    if (bpp == 24)
        stride = ((unsigned)w * 3 + 3) & ~3u;
    else if (bpp == 32)
        stride = (unsigned)w * 4;
    else if (bpp == 8)
        stride = ((unsigned)w + 3) & ~3u;
    else if (bpp == 4)
        stride = (((unsigned)w + 1) / 2 + 3) & ~3u;
    else
        stride = (((unsigned)w + 7) / 8 + 3) & ~3u;
    if (doff > (unsigned)n) return -2;
    if (doff < (dib == 12 ? 26u : 54u)) return -2;
    if ((unsigned)ah > ((unsigned)n - doff) / stride) return -2;
    full = (unsigned char *)u_malloc(w * ah * 3);
    if (!full) return -5;
    for (y = 0; y < ah; y++) {
        int sy = top ? y : ah - 1 - y;
        const unsigned char *f = d + doff + (unsigned)sy * stride;
        unsigned char *q0 = full + (unsigned)y * (unsigned)w * 3;
        for (x = 0; x < w; x++) {
            unsigned char *q1 = q0 + (unsigned)x * 3;
            if (bpp == 24) {
                q1[0] = f[x * 3 + 2];
                q1[1] = f[x * 3 + 1];
                q1[2] = f[x * 3];
            } else if (bpp == 32) {
                q1[0] = f[x * 4 + 2];
                q1[1] = f[x * 4 + 1];
                q1[2] = f[x * 4];
            } else {
                unsigned idx;
                if (bpp == 8)
                    idx = f[x];
                else if (bpp == 4)
                    idx = (x & 1) ? (unsigned)(f[x / 2] & 15)
                                  : (unsigned)((f[x / 2] >> 4) & 15);
                else
                    idx = (unsigned)((f[x / 8] >> (7 - (x & 7))) & 1);
                if (idx >= ncols) {
                    q1[0] = q1[1] = q1[2] = 0;
                } else {
                    /* Paleta BGR (CORE 3 B, INFO 4 B con relleno). */
                    const unsigned char *pe =
                        d + paloff + idx * entry;
                    q1[0] = pe[2];
                    q1[1] = pe[1];
                    q1[2] = pe[0];
                }
            }
        }
    }
    q = img_mini(full, w, ah, o);
    u_free(full);
    return q;
}

/* Estado LZW en estaticos (4K entradas no caben en la pila). */
static int lzw_pre[4096];
static unsigned char lzw_suf[4096];
static unsigned char lzw_pila[4096];

static int img_gif(const unsigned char *d, int n, u_img_t *o) {
    int w, h, p = 13, gct = 0, gsz = 0;
    const unsigned char *gtab = 0;
    int trans = -1;
    unsigned char *full = 0, *idx = 0;
    int q = -2;
    if (n < 13) return -2;
    w = (int)img_le16(d + 6);
    h = (int)img_le16(d + 8);
    if (w < 1 || h < 1) return -2;
    if (w > U_IMG_MAX_DIM || h > U_IMG_MAX_DIM) return -4;
    if ((long)w * (long)h > U_IMG_MAX_PX) return -4;
    if (d[10] & 0x80) {
        gct = 1;
        gsz = 2 << (d[10] & 7);
        gtab = d + 13;
        p = 13 + gsz * 3;
    }
    if (p > n) return -2;
    for (;;) {
        int sep, etiqueta, tam;
        if (p >= n) break;
        sep = d[p++];
        if (sep == 0x3B) break;
        if (sep == 0x21) {
            if (p >= n) break;
            etiqueta = d[p++];
            if (etiqueta == 0xF9 && p + 5 < n && d[p] >= 4) {
                if (d[p + 1] & 1) trans = d[p + 4];
                p++;
            }
            for (;;) {
                if (p >= n) break;
                tam = d[p++];
                if (!tam) break;
                if (p + tam > n) {
                    p = n;
                    break;
                }
                p += tam;
            }
            continue;
        }
        if (sep != 0x2C) {
            /* Bloque desconocido: se ignora por sub-bloques. */
            for (;;) {
                if (p >= n) break;
                tam = d[p++];
                if (!tam) break;
                if (p + tam > n) {
                    p = n;
                    break;
                }
                p += tam;
            }
            continue;
        }
        /* Descriptor de imagen (primer frame; el resto se ignora). */
        {
            int izq, arr, iw, ih, plc, inter, minc, i;
            const unsigned char *tab = gtab;
            int ntab = gsz;
            if (p + 9 > n) {
                q = -2;
                goto mal;
            }
            izq = (int)img_le16(d + p);
            arr = (int)img_le16(d + p + 2);
            iw = (int)img_le16(d + p + 4);
            ih = (int)img_le16(d + p + 6);
            plc = d[p + 8];
            p += 9;
            inter = (plc & 0x40) ? 1 : 0;
            if (iw < 1 || ih < 1 || izq < 0 || arr < 0 ||
                izq + iw > w || arr + ih > h) {
                q = -2;
                goto mal;
            }
            if (plc & 0x80) {
                ntab = 2 << (plc & 7);
                if (p + ntab * 3 > n) {
                    q = -2;
                    goto mal;
                }
                tab = d + p;
                p += ntab * 3;
            } else if (!gct) {
                q = -3;
                goto mal;
            }
            if (p >= n) {
                q = -2;
                goto mal;
            }
            minc = d[p++];
            if (minc < 2 || minc > 8) {
                q = -2;
                goto mal;
            }
            full = (unsigned char *)u_malloc((unsigned)w *
                                             (unsigned)h * 3);
            if (!full) {
                q = -5;
                goto mal;
            }
            for (i = 0; i < w * h * 3; i++) full[i] = 0;
            idx = (unsigned char *)u_malloc((unsigned)iw *
                                            (unsigned)ih);
            if (!idx) {
                q = -5;
                goto mal;
            }
            /* LZW: lee codigos LSB-first de los sub-bloques. */
            {
                int claro = 1 << minc, eoi = claro + 1;
                int proximo = eoi + 1, bits = minc + 1;
                int npx = 0, cdigo = 0, nbits = 0, fin = 0;
                int previo = -1, primero = 0, k;
                for (k = 0; k < claro; k++) {
                    lzw_pre[k] = -1;
                    lzw_suf[k] = (unsigned char)k;
                }
                while (!fin && p < n) {
                    int tam2 = d[p++];
                    int f2;
                    if (!tam2) break;
                    if (p + tam2 > n) {
                        q = -2;
                        goto mal;
                    }
                    for (f2 = 0; f2 < tam2 && !fin; f2++) {
                        cdigo |= (int)d[p + f2] << nbits;
                        nbits += 8;
                        while (!fin && nbits >= bits) {
                            int cd = cdigo & ((1 << bits) - 1);
                            int cur, npi = 0;
                            cdigo >>= bits;
                            nbits -= bits;
                            if (cd == claro) {
                                proximo = eoi + 1;
                                bits = minc + 1;
                                previo = -1;
                                continue;
                            }
                            if (cd == eoi) {
                                fin = 1;
                                break;
                            }
                            cur = cd;
                            if (cd >= proximo) {
                                if (previo < 0) {
                                    q = -2;
                                    goto mal;
                                }
                                cur = previo;
                                lzw_pila[npi++] = (unsigned char)primero;
                            }
                            while (cur >= claro) {
                                if (cur < 0 || cur >= 4096 ||
                                    npi >= 4095) {
                                    q = -2;
                                    goto mal;
                                }
                                lzw_pila[npi++] =
                                    lzw_suf[cur];
                                cur = lzw_pre[cur];
                            }
                            primero = cur;
                            lzw_pila[npi++] = (unsigned char)cur;
                            while (npi > 0 && npx < iw * ih) {
                                idx[npx++] =
                                    lzw_pila[--npi];
                            }
                            if (previo >= 0 && proximo < 4096) {
                                lzw_pre[proximo] = previo;
                                lzw_suf[proximo] =
                                    (unsigned char)primero;
                                proximo++;
                                if (proximo == (1 << bits) &&
                                    bits < 12)
                                    bits++;
                            }
                            previo = cd;
                            if (npx >= iw * ih) fin = 1;
                        }
                    }
                    p += tam2;
                }
                if (npx < iw * ih) {
                    q = -2;
                    goto mal;
                }
            }
            /* Indices -> RGB (transparencia = negro). */
            {
                int yy, xx;
                static const int pasa_ini[4] = { 0, 4, 2, 1 };
                static const int pasa_paso[4] = { 8, 8, 4, 2 };
                for (yy = 0; yy < ih; yy++) {
                    int yd = yy;
                    if (inter) {
                        int r2 = yy, ps;
                        for (ps = 0; ps < 4; ps++) {
                            if (yy >= pasa_ini[ps] &&
                                ((yy - pasa_ini[ps]) %
                                 pasa_paso[ps]) == 0) {
                                int orden = 0, t2;
                                for (t2 = 0; t2 < ps; t2++) {
                                    int c2 = ih - pasa_ini[t2];
                                    if (c2 > 0)
                                        orden += (c2 +
                                            pasa_paso[t2] - 1) /
                                                 pasa_paso[t2];
                                }
                                orden += (yy - pasa_ini[ps]) /
                                         pasa_paso[ps];
                                r2 = orden;
                                break;
                            }
                        }
                        yd = r2;
                    }
                    for (xx = 0; xx < iw; xx++) {
                        int iv = idx[yd * iw + xx];
                        unsigned char *qp =
                            full + ((arr + yy) * w + izq + xx) * 3;
                        if (iv == trans || iv >= ntab) {
                            qp[0] = qp[1] = qp[2] = 0;
                        } else {
                            qp[0] = tab[iv * 3];
                            qp[1] = tab[iv * 3 + 1];
                            qp[2] = tab[iv * 3 + 2];
                        }
                    }
                }
            }
            q = img_mini(full, w, h, o);
            u_free(full);
            u_free(idx);
            return q;
        }
    }
mal:
    if (full) u_free(full);
    if (idx) u_free(idx);
    return q;
}

int u_img_dec(const unsigned char *d, int n, u_img_t *o) {
    if (!d || n < 0 || !o) return -1;
    o->w = o->h = 0;
    o->px = 0;
    if (n >= 2 && d[0] == 'B' && d[1] == 'M')
        return img_bmp(d, n, o);
    if (n >= 6 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F' &&
        d[3] == '8' && (d[4] == '7' || d[4] == '9') && d[5] == 'a')
        return img_gif(d, n, o);
    return -2;
}
