#ifndef F32_ULIB_H
#define F32_ULIB_H

#include "sys.h"

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
const char *u_estado_nombre(int e);

#endif
