#ifndef F32_UGFX_H
#define F32_UGFX_H

#include "ulib.h"
#include "tema.h"   /* v2.1-P0: colores RGB del tema para apps UGFX */

/* F32 C++: API C (sin mangling). */
#ifdef __cplusplus
extern "C" {
#endif

/* UGFX v1 (etapa 5): biblioteca grafica de usuario para FacilOS32.
 * Capa Ring3 sobre el protocolo del servidor (IPC/buzones). No conoce el
 * framebuffer fisico: dibuja con syscalls de lease durante DRAW.
 *
 * Modelo: el servidor posee cada ventana (x,y,w,h); el cliente dibuja
 * solo su CONTENIDO (debajo del cromo de 20 px) al recibir DRAW y
 * confirma con ACK. Eventos: FOCO, TECLA, RATON, SALIR.
 */

#define UG_CHROME 20        /* alto de la barra de titulo (servidor) */
#define UG_FW 8             /* ancho de celda de texto (fuente BIOS 8x16) */
#define UG_FH 16            /* alto de celda de texto */

/* Tipos de evento (ug_ev_t.tipo). */
#define UG_EV_NADA 0
#define UG_EV_DRAW 1        /* redibujar todo el contenido */
#define UG_EV_FOCO 2        /* foco: ev.foco (1 dentro, 0 fuera) */
#define UG_EV_TECLA 3       /* tecla: ev.tecla (ASCII, 8, 13, 9, 27, 0x101+) */
#define UG_EV_RATON 4       /* raton: rx,ry (pantalla), rbot, rueda */
#define UG_EV_SALIR 5       /* el servidor pide cerrar (QUIT) */

typedef struct {
    int tipo;
    int x, y, w, h;         /* DRAW: geometria actual de la ventana */
    int foco;               /* FOCO */
    int tecla;              /* TECLA */
    int rx, ry, rbot;       /* RATON (coords de pantalla) */
    int rueda;              /* RATON: giro desde el aviso anterior */
} ug_ev_t;

typedef struct {
    int srv;                /* pid del servidor */
    int id;                 /* id de ventana (-1 sin registrar) */
    int w, h;               /* tamano actual */
    int x, y;               /* posicion actual */
    int foco;               /* ultimo FOCO conocido */
    int vivo;               /* 0 tras SALIR */
    int ox, oy;             /* origen del contenido (lease activo) */
    /* Coalescencia de DRAW (F8-compositor): mensaje inspeccionado que
       llego tras el DRAW en curso; se entrega en la proxima llamada
       en vez de perderse (antes se descartaba al mirar el buzon). */
    char pend_msg[64];
    int pend_len;
    int pend_de;
    int hay_pend;
} ug_win_t;

/* Conexion/registro: envia REG y espera ID (bloquea). 0 ok, <0 error. */
int ug_abre(ug_win_t *v, int srv, int w, int h, const char *titulo);
/* Cierra: envia BYE. */
void ug_cierra(ug_win_t *v);
/* Siguiente evento: bloquear!=0 espera; 0 vuelve con NADA si vacio.
   Devuelve el tipo (o SALIR si llego QUIT). */
int ug_evento(ug_win_t *v, ug_ev_t *ev, int bloquear);
/* Pide geometria al servidor ("INF"): 0 ok. Tolera DRAW interleados. */
int ug_pide_info(ug_win_t *v, int *w, int *h);
/* Pide reframe al servidor (tras un hijo que escribio a consola). */
void ug_sync(ug_win_t *v);
/* Refresco periodico: el servidor manda DRAW cada s segundos (1-60;
   0 cancela). Para relojes/indicadores en vivo (v2.1-p1). */
void ug_tick(ug_win_t *v, int s);
/* Modo edicion (v2.2): TAB y ESC llegan a la ventana (autocompletado).
   0 lo cancela. */
void ug_edit(ug_win_t *v, int on);
/* Modo modal: 1 suspende entrada y pintado del servidor (hijo con
   pantalla+teclado propios), 0 reanuda con repintado total. */
void ug_modal(ug_win_t *v, int on);

/* ---- dibujo (coords relativas al contenido; solo entre DRAW y ug_fin) */
void ug_comienza(ug_win_t *v);   /* toma el lease */
void ug_fin(ug_win_t *v);        /* presenta y confirma ACK */
void ug_pixel(ug_win_t *v, int x, int y, int color);
void ug_linea(ug_win_t *v, int x0, int y0, int x1, int y1, int color);
void ug_rect(ug_win_t *v, int x, int y, int w, int h, int color);
void ug_relleno(ug_win_t *v, int x, int y, int w, int h, int color);
void ug_texto(ug_win_t *v, int x, int y, const char *s, int color);
void ug_limpia(ug_win_t *v, int color);   /* limpia el contenido */

/* ---- controles ---- */
#define UG_BOTON_OFF 3
#define UG_BOTON_PULSADO 2
#define UG_BOTON_HOVER 1
#define UG_BOTON_NORMAL 0

int ug_dentro(int x, int y, int x0, int y0, int w, int h);
void ug_etiqueta(ug_win_t *v, int x, int y, const char *s, int color);
void ug_boton(ug_win_t *v, int x, int y, int w, int h, const char *t,
              int estado);
/* Flanco de subida del boton izquierdo: 1 solo al pulsar. */
int ug_clic(int antes, int ahora);
void ug_panel(ug_win_t *v, int x, int y, int w, int h, int borde,
              int relleno);
void ug_check(ug_win_t *v, int x, int y, const char *t, int marcado);
/* Campo de texto: dibuja con cursor si foco!=0. */
void ug_campo(ug_win_t *v, int x, int y, int w, const char *buf, int pos,
              int foco);
/* Edicion: insertar/borrar/izq/der. Devuelve 1 si cambio. */
int ug_campo_tecla(char *buf, int max, int *pos, int tecla);
/* Lista: dibuja n items (h = n*16 recortado), sel resaltado, scroll base. */
void ug_lista(ug_win_t *v, int x, int y, int w, int h, const char **items,
              int n, int sel, int scroll);
/* Navegacion de lista: arriba/abajo. Devuelve 1 si cambio. */
int ug_lista_tecla(int *sel, int *scroll, int n, int vis, int tecla);
/* Clic en lista: item bajo (mx,my) o -1. My en coords de contenido. */
int ug_lista_clic(int my, int y0, int scroll, int n);

#ifdef __cplusplus
}
#endif

#endif
