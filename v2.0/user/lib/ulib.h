#ifndef F32_ULIB_H
#define F32_ULIB_H

#include "sys.h"

/* F32 C++: las APIs de ulib son C (sin mangling). */
#ifdef __cplusplus
extern "C" {
#endif

/* Biblioteca C propia de usuario (v1.6). Sin libc: todo sobre syscalls.
   Compila con -ffreestanding -nostdlib; enlaza en programas F32X v3. */

/* ---- cadenas ---- */
int u_strlen(const char *s);
int u_strcmp(const char *a, const char *b);
int u_strncmp(const char *a, const char *b, int n);
void u_strcpy(char *d, const char *s);
void u_strncpy(char *d, const char *s, int n);
void u_strcat(char *d, const char *s);
char *u_strchr(const char *s, int c);
int u_atoi(const char *s);
void u_itoa(int v, char *b);
void u_utoa(unsigned int v, char *b, int base);
void *u_memcpy(void *d, const void *s, int n);
void *u_memmove(void *d, const void *s, int n);
void *u_memset(void *d, int c, int n);
int u_memcmp(const void *a, const void *b, int n);

/* ---- memoria (monticulo sbrk, first-fit con union) ---- */
void *u_malloc(int n);
void u_free(void *p);
void *u_realloc(void *p, int n);
void *u_calloc(int n, int tam);
void u_malloc_info(int *nbloq, int *nbytes, int *nlibre);

/* ---- ficheros (sin bufer: posicion sincronizada por syscall) ---- */
#define U_INI BUSCAR_INI
#define U_ACT BUSCAR_ACT
#define U_FIN BUSCAR_FIN

typedef struct {
    int fd;
    int tam;
    int pos;
} UFILE;

UFILE *u_fopen(const char *ruta, const char *modo);
int u_fread(void *buf, int n, UFILE *f);
int u_fwrite(const void *buf, int n, UFILE *f);
int u_fseek(UFILE *f, int off, int base);
int u_ftell(UFILE *f);
int u_fsize(UFILE *f);
int u_fijar_tam(UFILE *f, int tam);
int u_fclose(UFILE *f);
int u_remove(const char *ruta);

/* ---- salida / entrada ---- */
void u_print(const char *s);
void u_print_num(int v);
void u_print_hex(unsigned int v);
void u_printf(const char *fmt, ...);
int u_tecla(void);
const char *u_args(void);
void u_lee_linea(char *d, int max);

#define TEC_ARRIBA_U 0x101
#define TEC_ABAJO_U  0x102
#define TEC_IZQ_U    0x103
#define TEC_DER_U    0x104
#define TEC_MENU_U   0x105
/* v2.2: suprimir/inicio/fin/repag/avpag + TAB 0x0F del driver. */
#define TEC_SUPR_U   0x106
#define TEC_INICIO_U 0x107
#define TEC_FIN_U    0x108
#define TEC_REPAG_U  0x109
#define TEC_AVPA_U   0x10A
/* v2.2-editor: modificadores en la tecla (ver teclado.h): May 0x200,
   Ctrl 0x400 sobre 0x101-0x10A; Ctrl+letra 0xE100-0xE119
   (0xE200-0xE219 con May); Ctrl+Tab 0xE120/0xE220; May+Tab 0xE121;
   Ctrl+Retroceso 0x7F. */
#define TEC_MAY_U  0x200
#define TEC_CTRL_U 0x400
#define TEC_CTRL_AU  0xE100
#define TEC_CTRL_MAYAU 0xE200
#define TEC_CTRL_TAB_U 0xE120
#define TEC_CTRL_MAY_TAB_U 0xE220
#define TEC_MAY_TAB_U 0xE121

/* ---- graficos ---- */
#define C_NEGRO 0x00
#define C_AZUL 0x01
#define C_VERDE 0x02
#define C_CYAN 0x03
#define C_ROJO 0x04
#define C_MAGENTA 0x05
#define C_MARRON 0x06
#define C_GRIS 0x07
#define C_GRIS_OSCURO 0x08
#define C_AZUL_CLARO 0x09
#define C_VERDE_CLARO 0x0A
#define C_CYAN_CLARO 0x0B
#define C_ROJO_CLARO 0x0C
#define C_MAGENTA_CLARO 0x0D
#define C_AMARILLO 0x0E
#define C_BLANCO 0x0F

#define U_RGB(r, g, b) \
    ((((unsigned int)(r)) << 16) | (((unsigned int)(g)) << 8) | \
     ((unsigned int)(b)))

typedef struct {
    unsigned int w, h;
    unsigned int pix[1];
} USPRITE;

void u_destino(int d);
void u_fija_clip(int x, int y, int w, int h);
void u_presentar(int x, int y, int w, int h);
void u_pixel(int x, int y, int color);
void u_linea(int x1, int y1, int x2, int y2, int color);
void u_rectangulo(int x, int y, int w, int h, int color);
void u_texto(int x, int y, const char *s, int color);
void u_limpiar(int color);
void u_sprite(int x, int y, const USPRITE *s);
void u_sprite_t(int x, int y, const USPRITE *s, unsigned int clave);
void u_blit(int x0, int y0, int w, int h, int x1, int y1);
unsigned int u_punto(int x, int y);

/* ---- tiempo ---- */
int u_ticks(void);
int u_dormir(int ms);
int u_hay_tecla(void);

/* ---- sistema v1.7 ---- */
int u_ejecutar(const char *ruta, const char *args);
int u_terminal(void);

/* ---- procesos v1.9 ---- */
typedef struct {
    int pid;
    int estado;
    int rc;
    int grafico;
    int marcos;
    char nombre[16];
} uproc_t;

int u_crear(const char *nombre, const char *args);   /* >0 pid, <0 error */
int u_mi_pid(void);
int u_listar(uproc_t *buf, int max);                  /* devuelve cuantos */
int u_esperar(int pid);
int u_ceder(void);
int u_info_memoria(unsigned int *v5);
int u_info_memoria2(unsigned int *v8);   /* v2.0: 8 palabras (ver sys.h) */
int u_proc_stat(int pid, unsigned int *v5); /* v2.0: estado,ticks,cedes,pico,nmarcos */
int u_ipc_enviar(int dst, const void *ptr, int len);
int u_ipc_recibir(void *buf, int max, int *emisor);
int u_ipc_hay(void);   /* mensajes pendientes (no bloquea) */
int u_raton(unsigned int *v4);   /* [x,y,botones,rueda], bloquea */
int u_hay_raton(void);           /* paquetes pendientes (no bloquea) */
const char *u_estado_nombre(int e);

/* ---- directorios y reloj v2.0-etapa5 (syscalls 41-44) ---- */
typedef struct {
    int tipo;             /* 1 carpeta, 2 archivo, 3 programa del CD */
    char nombre[33];
    unsigned int tam;
    int idx;              /* indice fs o -1 si es CD */
} udir_t;                 /* 48 B en cable (tipo, nombre, 3 relleno, tam, idx) */

int u_dir_hijo(int dir, int n, udir_t *dst);  /* tipo o -1; dir<0 = actual */
int u_dir_crear(const char *ruta);            /* 0 ok, -1 mal/existe */
int u_renombrar(const char *ruta, const char *nuevo); /* 0,-1,-2 choca */
int u_reloj(unsigned int *v6);                /* [Y,M,D,h,m,s], 0 ok */

/* ---- video en vivo (syscalls 47-48) ---- */
int u_video_modo(int w, int h);   /* 0 ok, -1 sin soporte, -2 sin memoria */
int u_video_info(unsigned int *v4);  /* [W,H,paso,prof], 0 ok */

/* ---- modificadores de teclado (syscall 49, v2.2-editor) ---- */
int u_teclado_mod(void);  /* bit0 Shift, bit1 Ctrl, bit2 AltGr, bit3 Caps */

/* ---- energia (syscalls 50-51, v2.2-f32 boton FacilOS) ---- */
void u_apagar(void);     /* no retorna si hay ACPI */
void u_reiniciar(void);  /* no retorna si el 8042 responde */

/* ---- red propia (syscalls 52-54, v2.3-red F1; sin POSIX) ---- */
/* Ficha de 32 B (disposicion identica a drv_red.h del nucleo).
   rsv[0]: ARP en cache; rsv[1]: ICMP ok; rsv[2]: ICMP errores;
   rsv[3]: UDP recibidos (F4). Sin adaptador (retorno != 0) los
   rsv traen diagnostico del probe: rsv[0..1] etapa virtio,
   rsv[2] bitmask de NICs PCI vistas, rsv[3] slot; mac =
   vendor:device del primer clase-red (dhcp lo muestra). */
typedef struct {
    unsigned char mac[6];
    unsigned char presente; /* 1 si hay adaptador activo */
    unsigned char irq;      /* linea PCI (informativa) */
    unsigned short io_base; /* base PIO */
    unsigned short mtu;     /* 1500 */
    unsigned int rx_ok;
    unsigned int tx_ok;
    unsigned int rx_err;
    unsigned int tx_err;
    unsigned char rsv[4];
} red_if_t;
int u_red_interfaz(red_if_t *f);  /* 0 ok, -1 sin adaptador */
/* Envia trama Ethernet completa (14+carga, sin CRC). Tam 14..1514.
   Devuelve tam o -1 sin adaptador, -2 tam invalido, -3 agotada. */
int u_red_trama_enviar(const void *marco, int tam);
/* Recibe trama (sin CRC). >0 tam, 0 espera agotada, -1 sin adaptador,
   -2 parametros invalidos. Espera acotada a 10 s. */
int u_red_trama_recibir(void *marco, int max, int espera);
/* F2: configuracion L3 (16 B: ip, mascara, puerta, dns; primer
   octeto en el byte bajo). */
typedef struct {
    unsigned int ip;
    unsigned int mascara;
    unsigned int puerta;
    unsigned int dns;
} red_cfg_t;
int u_red_configurar_leer(red_cfg_t *c);      /* op 0 */
int u_red_fijar(int campo, unsigned int valor); /* 1=ip 2=mask 3=gw 4=dns */
/* F2: consulta (12 B: ip, mac[6], espera): 0 ok, 1 espera agotada. */
typedef struct {
    unsigned int ip;
    unsigned char mac[6];
    unsigned short espera;
} red_consulta_t;
int u_red_consultar(int tipo, void *ptr);
/* F2: sonda IP de diagnostico (8 B: ip, ttl, proto, espera). */
typedef struct {
    unsigned int ip;
    unsigned char ttl;
    unsigned char proto;
    unsigned short espera;
} red_sonda_t;
/* F2: ruta (12 B: dst, mask, gw; gw=0 directa). Devuelve bytes. */
typedef struct {
    unsigned int dst;
    unsigned int mask;
    unsigned int gw;
} red_ruta_t;
int u_red_rutas(void *buf, int max);
/* F3: comprobar eco ICMP. Entrada 12 B {ip,n,espera,ttl};
   salida 6 B por eco {seq,rtt,ok,ttl}; devuelve bytes o negativo
   (-1 sin adaptador, -2 args, -3 sin IP; -4 ARP agotado y -5
   fallo Tx se propagan del envio). */
typedef struct {
    unsigned int ip;
    unsigned short n;
    unsigned short espera;
    unsigned char ttl;
    unsigned char res;
    unsigned short res2;
} red_peticion_t;
typedef struct {
    unsigned short seq;
    unsigned short rtt;
    unsigned char ok;
    unsigned char ttl;
} red_respuesta_t;
int u_red_comprobar(const red_peticion_t *pet, red_respuesta_t *resp,
                    int max);
/* F4: UDP propio (sin sockets). Entrada envio 8 B {ip,dpuerto,
   spuerto}; salida recepcion {src:4,sport:2,len:2}+datos. */
typedef struct {
    unsigned int ip;
    unsigned short dpuerto;
    unsigned short spuerto;
} red_udp_envio_t;
typedef struct {
    unsigned short puerto;
    unsigned short espera;
} red_udp_peticion_t;
int u_red_udp_ligar(int puerto);
int u_red_udp_enviar(const red_udp_envio_t *e, const void *datos,
                     int len);
int u_red_udp_recibir(const red_udp_peticion_t *pet, void *buf,
                      int max);
/* F5: TCP propio (ops de la 61; structs de tamano fijo). Estados
   1 escucha .. 6 fin remoto; errores -1..-9, -12 fin remoto.
   F6-C: -8 tambien es ventana del par cerrada o insuficiente
   al enviar (reintentar mas tarde, sin bloquear); -6 tambien
   es tupla protegida durante la gracia tras cierre (reintentar
   pasados unos segundos; ver TCP.md, no es TIME_WAIT). */
typedef struct {
    unsigned int ip;
    unsigned short puerto;
    unsigned short local;
} red_tcp_abre_t;
typedef struct {
    unsigned short puerto;
    unsigned short res;
} red_tcp_escucha_t;
typedef struct {
    unsigned int id;
    unsigned short espera;
    unsigned short res;
} red_tcp_acepta_t;
typedef struct {
    unsigned int id;
} red_tcp_id_t;
int u_tcp_abrir(unsigned int ip, unsigned short puerto,
                unsigned short local);
int u_tcp_escuchar(unsigned short puerto);
int u_tcp_aceptar(int id, unsigned short espera);
int u_tcp_enviar(int id, const void *datos, int len);
int u_tcp_recibir(int id, void *buf, int max, unsigned short espera);
int u_tcp_cerrar(int id);
int u_tcp_abortar(int id);
int u_tcp_estado(int id);
/* F4: DNS A/IN. Arma consulta; devuelve long (>=12) o negativo
   (-1 args, -2 dominio invalido, -3 sin sitio). Con error, la
   salida puede quedar parcial: solo vale si devuelve long. */
int u_dns_armar(const char *dom, unsigned short xid,
                unsigned char *sal, int max);
int u_dns_leer(const unsigned char *m, int n, unsigned short xid,
               const unsigned char *preg, int npreg,
               unsigned int *ips, unsigned int *ttls, int maxips,
               int *rcode);
/* F6-D: lectura ampliada. Ademas de lo de u_dns_leer: valida que
   los A pertenezcan a la pregunta o a la cadena CNAME del mismo
   mensaje; si hay CNAME y ningun A, expande el primer destino a
   cname (texto con puntos, "" si directo) y su TTL minimo a
   *cttl. cname/cnmax y cttl pueden ser NULL/0 (equivale a leer).
   u_dns_leer es un envoltorio con cname=0. */
int u_dns_leer2(const unsigned char *m, int n, unsigned short xid,
                const unsigned char *preg, int npreg,
                unsigned int *ips, unsigned int *ttls, int maxips,
                int *rcode, char *cname, int cnmax,
                unsigned int *cttl);
/* F6-D: resolucion completa Ring3 (cache + UDP + CNAME + TCP).
   dom: nombre; servidor 0 = configurado, puerto 0 = 53.
   espera_ms 0 = 30000 (tope 60000). Rellena r y devuelve:
   0 ok, -1 args, -2 nombre invalido, -3 NXDOMAIN, -4 error DNS
   (r->rcode), -5 timeout/red, -6 TCP fallo, -7 ciclo/profundidad,
   -8 NODATA. r->ip/ttl validos solo con 0; r->cname cadena de
   alias ("" si directo); r->de_cache/via_tcp informativos. */
typedef struct {
    unsigned int ip;
    unsigned int ttl;
    int rc;
    int rcode;
    int nrec;
    int saltos;
    int via_tcp;
    int de_cache;
    char cname[256];
} u_dns_res_t;
int u_dns_resuelve3(const char *dom, unsigned int servidor,
                    unsigned short puerto, u_dns_res_t *r,
                    unsigned int espera_ms);
int u_dns_resuelve(const char *dom, unsigned int servidor,
                   unsigned int *ip, unsigned int *ttl);
/* F6-D: cache (por proceso). n = entradas validas; limpiar la
   vacia. cache_viva es el predicado puro (para pruebas). */
void u_dns_cache_limpiar(void);
int u_dns_cache_n(void);
int u_dns_cache_viva(unsigned int ins, unsigned int ttl,
                     unsigned int ahora);
/* F7-B: HTTP/1.0 Ring3 (sin sockets; sobre TCP 61 + DNS).
   Solo GET con Connection: close. Parser incremental y
   reutilizable (patron del codec DNS F6-D). */
#define U_HTTP_MAX_URL 512     /* URL completa */
#define U_HTTP_MAX_SEC 8192    /* seccion de cabeceras */
#define U_HTTP_MAX_LIN 4096  /* linea de estado/cabecera (v2.5: 1024->4096; CSP real de DDG ~2K) */
#define U_HTTP_MAX_CAB 32      /* cabeceras por respuesta */
#define U_HTTP_MAX_CPO 262144   /* cuerpo acumulado (v2.4.3: 256K) */
#define U_HTTP_NOM 64          /* nombre de cabecera */
#define U_HTTP_VAL 256         /* valor de cabecera */
/* v2.4.4 jar de cookies (solo memoria, sin disco): por proceso
   (cada programa Ring3 es un proceso: sin fugas entre apps).
   Coincidencia por host exacto (minusculas); se ignora
   Path/Domain/Expires/Secure (sesion). Valores largos se truncan. */
#define U_HTTP_GAL_N 48        /* entradas */
#define U_HTTP_GAL_NOM 64      /* nombre */
#define U_HTTP_GAL_VAL 192     /* valor */
#define U_HTTP_ST_LINEA 0      /* esperando linea de estado */
#define U_HTTP_ST_CAB 1        /* leyendo cabeceras */
#define U_HTTP_ST_CUERPO 2     /* leyendo cuerpo */
#define U_HTTP_ST_OK 3         /* mensaje completo */
#define U_HTTP_ST_ERR -1       /* error (ver r->err) */
typedef struct {
    int estado;                /* U_HTTP_ST_* */
    int err;                   /* 0 ok; neg: ver u_http_trae */
    int codigo;                /* 100..599, 0 si aun no hay linea */
    char version[16];          /* p. ej. "HTTP/1.0" */
    char motivo[64];           /* frase, puede ser "" */
    unsigned int ip; /* IPv4 del servidor (red) */
    unsigned short puerto; /* puerto usado */
    char hnom[U_HTTP_MAX_CAB][U_HTTP_NOM];
    char hval[U_HTTP_MAX_CAB][U_HTTP_VAL];
    int ncab;
    unsigned int content_length;
    int tiene_cl;              /* 1 si hubo Content-Length valido */
    int chunked;               /* 1 si vino Transfer-Encoding */
    unsigned int cuerpo_n;     /* bytes de cuerpo aceptados */
    unsigned int cuerpo_esp;   /* bytes esperados (CL) o 0 si cierre */
    /* v2.4.3 chunked (solo si chunked==1 y estado==CUERPO):
       ch_rest==-1 espera linea de tamano; >=0 faltan datos del
       trozo; -2 tras chunk-0 (trailers hasta linea vacia). */
    int ch_rest;
    char ch_lin[32];           /* linea de tamano/trailer en curso */
    int ch_nlin;               /* bytes en ch_lin */
    int ch_crlf;               /* 0 nada, 1 vista '\r' (fin de datos) */
} u_http_res_t;
typedef struct {
    char host[256];            /* minusculas, sin punto final */
    unsigned short puerto;     /* 1..65535, 80/443 por defecto */
    int esquema;               /* 0 http, 1 https (TLS) */
    char ruta[256];            /* empieza por '/', con ?consulta */
    unsigned int ip;           /* si es IPv4 literal */
    int es_ip;                 /* 1 si host era IPv4 literal */
} u_http_url_t;
/* Parsea URL http://. 0 ok; -1 args, -2 esquema, -3 host,
   -4 puerto, -5 largo. Solo http (nunca https). */
int u_http_url(const char *url, u_http_url_t *u);
/* Construye peticion GET en sal (cabe Host/Connection/User-Agent).
   Devuelve long o negativo (-1 args, -2 url, -3 sin sitio). */
int u_http_pide(const u_http_url_t *u, const char *ruta_extra,
                char *sal, int smax);
/* Parser incremental. u_http_res_inicia() una vez; despues
   u_http_res_datos() por cada trozo (el tope de cuerpo se toma
   de capcuerpo; el cuerpo se acumula en cuerpo[0..capcuerpo),
   *nbody avanza) y u_http_res_fin() al recibir EOF (-12).
   Devuelven 1 completo, 0 falta mas, -1 error (r->err detalla:
   -6 malformada (incl. CL no numerico o duplicado
   incoherente), -7 Transfer-Encoding, -8 sobre tope/
   excedente/truncada). Nunca acepta silenciosamente
   respuestas malformadas. */
void u_http_res_inicia(u_http_res_t *r);
int u_http_res_datos(u_http_res_t *r, const unsigned char *d, int n,
                     unsigned char *cuerpo, int capcuerpo, int *nbody);
int u_http_res_fin(u_http_res_t *r);
/* Busca cabecera (insensible a mayusculas), primera aparicion.
   Devuelve long del valor o -1 si no existe. */
int u_http_cabecera(const u_http_res_t *r, const char *nombre,
                    char *sal, int smax);
/* Igual pero aparicion idx (0 = primera): para Set-Cookie
   multiples. Devuelve long o -1 si no hay tantas. */
int u_http_cabecera_n(const u_http_res_t *r, const char *nombre, int idx,
                      char *sal, int smax);
/* Jar de cookies del proceso (ver U_HTTP_GAL_*). host en
   minusculas (como u_http_url_t.host). 0 ok; guarda ignora
   entradas vacias o host vacio. cab devuelve 0 si no hay
   nada para ese host o "Cookie: n=v; ...\r\n" (long sin NUL). */
void u_http_galleta_limpia(void);
int u_http_galleta_guarda(const char *host, const char *setcookie);
int u_http_galleta_cab(const char *host, char *sal, int smax);
/* Trae una URL completa (DNS + TCP + HTTP + cierre).
   cuerpo[0..capcuerpo) recibe el cuerpo (capcuerpo<=65536).
   espera_ms 0=60000, tope 120000. Devuelve:
   0 ok (respuesta completa y valida, cualquier codigo),
   -1 args, -2 URL invalida, -3 NXDOMAIN, -4 DNS (rcode/otro),
   -5 timeout/red (DNS/TCP/cierre), -6 respuesta malformada,
   -7 Transfer-Encoding no soportado, -8 cuerpo truncado,
   excedente o sobre tope. r->codigo/motivo/version/cabeceras
   quedan rellenos si hubo linea de estado valida. */
int u_http_trae(const char *url, u_http_res_t *r,
                unsigned char *cuerpo, int capcuerpo,
                unsigned int espera_ms);
/* v2.4.4: trae con metodo y cabeceras extra. metodo "GET" o "POST"
   (otro -> -1); extra son cabeceras "N: v\r\n" validadas (p. ej.
   Cookie, o 0); post/npost cuerpo a enviar (solo POST, se ignora
   en GET). Mismos codigos que u_http_trae; u_http_trae equivale a
   metodo GET sin extra ni cuerpo. */
int u_http_trae_x(const char *url, const char *metodo, const char *extra,
                  const unsigned char *post, int npost, u_http_res_t *r,
                  unsigned char *cuerpo, int capcuerpo,
                  unsigned int espera_ms);
/* Construye peticion POST en sal (como u_http_pide pero con
   Content-Length: npost y Content-Type: application/x-www-form-
   urlencoded; el cuerpo se envia aparte). Mismos codigos. */
int u_http_pide_post(const u_http_url_t *u, const char *extra, int npost,
                     char *sal, int smax);

/* F7-C: HTML minimo en Ring 3 (patron codec DNS/HTTP). v2.4: CSS
   basico (color/background + negrita/cursiva; sin cascada completa)
   y formularios GET (para buscadores tipo SearXNG). Sin JS.
   Todo acotado; la pagina vive en cuerpo<=65536 (tope F7-B) y el
   documento reparte un area propia (heap) sin duplicar la pagina.
   Parser permisivo con el contenido, estricto con los limites:
   truncados se avisan (retorno 1 / d->truncado), nunca silenciosos. */
#define U_HTML_MAX_TXT 49152    /* texto decodificado del documento */
#define U_HTML_MAX_SEG 1024     /* tramos inline (texto+estilo+enlace) */
#define U_HTML_MAX_BLOQ 512     /* bloques (titulo/parrafo/lista/img) */
#define U_HTML_MAX_ENL 128      /* enlaces distintos */
#define U_HTML_MAX_IMG 64       /* imagenes estructurales */
#define U_HTML_MAX_PILA 16      /* anidamiento inline */
#define U_HTML_URL 512          /* URL resuelta (como U_HTTP_MAX_URL) */
#define U_HTML_ALT 128          /* texto alternativo de imagen */

/* Tipos de bloque (u_html_bloq_t.tipo). */
#define U_HTML_B_TXT 0          /* texto suelto (fuera de bloque) */
#define U_HTML_B_H1 1           /* ... hasta H6 = 6 */
#define U_HTML_B_H2 2
#define U_HTML_B_H3 3
#define U_HTML_B_H4 4
#define U_HTML_B_H5 5
#define U_HTML_B_H6 6
#define U_HTML_B_P 7
#define U_HTML_B_LI 8
#define U_HTML_B_HR 9
#define U_HTML_B_IMG 10
#define U_HTML_B_BR 11          /* salto explicito (fila vacia) */

/* Estilos de tramo/fila (u_html_seg_t.est, u_html_fila_t.estilo). */
#define U_HTML_E_NORMAL 0
#define U_HTML_E_NEGRITA 1      /* strong/b */
#define U_HTML_E_ENFASIS 2      /* em/i */
#define U_HTML_E_ENLACE 3       /* a (siempre distinguible) */
#define U_HTML_E_TITULO 4       /* h1..h6 */
#define U_HTML_E_HR 5           /* separador (fila, sin texto) */
#define U_HTML_E_IMG 6          /* placeholder (fila, sin texto) */
#define U_HTML_E_VACIA 7        /* espaciado entre bloques */

typedef struct {
    int ini;                    /* offset en d->texto */
    int len;                    /* bytes (UTF-8 valido o FF FD) */
    int est;                    /* U_HTML_E_* */
    int enlace;                 /* indice a d->enl o -1 */
    int fg;                     /* RGB 0xRRGGBB, -1 = defecto */
    int bg;                     /* RGB 0xRRGGBB, -1 = defecto */
} u_html_seg_t;

typedef struct {
    int tipo;                   /* U_HTML_B_* */
    int nivel;                  /* h:1..6; li: sangria 0.. */
    int seg_ini;                /* primer tramo del bloque */
    int seg_n;                  /* tramos del bloque */
    int img;                    /* indice a d->img o -1 */
} u_html_bloq_t;

typedef struct {
    char url[U_HTML_URL];       /* absoluta http, sin fragmento */
    int ini;                    /* offset del ancla en d->texto */
    int len;                    /* bytes del ancla */
} u_html_enl_t;

typedef struct {
    char src[U_HTML_URL];       /* absoluta http (puede ser "") */
    char alt[U_HTML_ALT];       /* alternativa (puede ser "") */
} u_html_img_t;

typedef struct {
    char *texto;                /* area de texto (heap) */
    int texto_n;                /* bytes usados */
    u_html_seg_t *seg;          /* tramos (heap) */
    int nseg;
    u_html_bloq_t *bloq;        /* bloques (heap) */
    int nbloq;
    u_html_enl_t *enl;          /* enlaces (heap) */
    int nenl;
    u_html_img_t *img;          /* imagenes (heap) */
    int nimg;
    char titulo[128];           /* <title> (puede ser "") */
    int truncado;               /* 1 si algun tope recorto contenido */
    char forma[U_HTML_URL];     /* v2.4: action absoluta (GET) o "" */
    char campo[64];             /* v2.4: nombre del input de texto o "" */
    int hay_forma;              /* v2.4: 1 si hay formulario GET usable */
    int forma_post;             /* v2.4.4: 1 si hay formulario POST usable
                                   (misma action/campo; method != get) */
    int img_fil[U_HTML_MAX_IMG]; /* filas por imagen (0 = 1 placeholder);
                                   lo rellena el visor tras decodificar */
} u_html_doc_t;

/* Crea/libera el documento (reserva heap). 0 ok, -1 sin memoria. */
int u_html_crea(u_html_doc_t *d);
void u_html_libera(u_html_doc_t *d);
/* Interpreta pg[0..n) contra base (URL absoluta http, "" = sin base
   para relativas). 0 ok completo, 1 ok con truncado (d->truncado),
   -1 args. Nunca se cuelga con documentos malformados. */
int u_html_parse(u_html_doc_t *d, const unsigned char *pg, int n,
                 const char *base);
/* Resuelve ref contra base (absoluta http). sale[U_HTML_URL].
   0 ok; -1 args; -2 esquema no navegable (https/javascript/mailto/
   vacio con blancos); -3 largo o base invalida. */
int u_html_resuelve_url(const char *base, const char *ref, char *sal);
#ifdef F32_NSHUBBUB
/* v2.4.3+hubbub: igual contrato que u_html_parse, pero tokenizado
   con Hubbub (HTML5 real: entidades, comentarios, rawtext). 0 ok,
   1 truncado, -1 args, -2 sin memoria/Hubbub (reintentar legacy). */
int u_html_parse_hb(u_html_doc_t *d, const unsigned char *pg, int n,
                    const char *base);
#endif

/* Fila de layout (u_html_maqueta): tramo de texto ya ajustado al
   ancho. ini/len referencian d->texto (len 0 en VACIA/HR/IMG).
   x = sangria en celdas; enlace = indice o -1; img = indice o -1. */
typedef struct {
    int ini;
    int len;
    int x;
    int estilo;                 /* U_HTML_E_* */
    int enlace;
    int img;
    int fg;                     /* RGB 0xRRGGBB, -1 = defecto */
    int bg;                     /* RGB 0xRRGGBB, -1 = defecto */
} u_html_fila_t;

#define U_HTML_MAX_FILA 2048    /* filas por documento */
/* Distribuye el documento en filas de cols celdas (cols>=10).
   Devuelve nfilas (0 = documento vacio) o -1 por args. */
int u_html_maqueta(const u_html_doc_t *d, int cols,
                   u_html_fila_t *filas, int maxfilas);

/* F7-D: imagenes raster (BMP/GIF) para fbrowser. Solo memoria,
   sin libc. Decodifica a miniatura RGB acotada (sin ampliar). */
#define U_IMG_MAX_PX 65536     /* tope de decodificacion (256x256) */
#define U_IMG_MAX_DIM 512      /* tope por lado */
#define U_IMG_BOX_W 120        /* caja de miniatura */
#define U_IMG_BOX_H 64
#define U_IMG_N 8              /* imagenes por pagina (fbrowser) */
typedef struct {
    int w, h;                  /* miniatura 1..BOX (0 si vacia) */
    unsigned char *px;         /* RGB w*h*3 (heap) o 0 */
} u_img_t;
/* BMP (BI_RGB 1/4/8/24/32) o GIF87a/89a (primer frame, con/sin
   entrelace; transparencia = negro). Rellena o (px en heap).
   0 ok; -1 args; -2 formato/magica o truncado; -3 no soportado
   (BMP comprimido, GIF sin tabla); -4 demasiado grande;
   -5 sin memoria. */
int u_img_dec(const unsigned char *d, int n, u_img_t *o);
void u_img_lib(u_img_t *o);

#ifdef __cplusplus
}
#endif

#endif
