#include "ulib.h"
#include "tema.h"

void __main(void) {
}

/* servidor (etapa 5/6): escritorio FacilOS32 por leases.
 * Compone fondo + ventanas + barra de tareas + menu + cursor; gestiona
 * foco, orden Z, arrastre, botones de ventana, menu de apps y reloj.
 * Protocolo (<=64 B, texto). C->S: REG w h titulo / BYE / ACK / SYNC / INF.
 * S->C: ID n / DRAW x y w h / FOCUS 0-1 / KEY code / MOU x y bot giro /
 * QUIT / INF w h. La shell queda excluida mientras corre (como el menu).
 */

#define NVENT 16
/* v2.1-p1: dimensiones reales (las pasa el nucleo en args "W H";
   defecto 800x600). Sin esto, otros modos VESA descentran todo. */
static int pan_w = 800, pan_h = 600, area_h = 570;
#define PANT_W (pan_w)
#define PANT_H (pan_h)
#define AREA_H (area_h)
#define CROMO_H 20
#define NMENU 8

typedef struct {
    int usado;
    int pid;
    int w, h, x, y, z;
    int visible;
    int minimizada;
    int foco;
    int max;                  /* v2.1-p1: maximizada (geometria previa en p*) */
    int px, py, pw, ph;
    int edit;                 /* v2.2: modo edicion (TAB y ESC llegan) */
    int ack_pend;       /* DRAW sin confirmar (cliente lento/bloqueado) */
    char titulo[24];
} ventana_t;

static ventana_t vents[NVENT];
static int nz = 0;
static int foco_id = -1;
static int mx = 400, my = 300, mb = 0, mb_antes = 0;
static int mou_id = -1, mou_bot = 0;
static int giro_lote = 0;   /* giro de rueda acumulado del lote */
static int arr_id = -1, arr_dx = 0, arr_dy = 0, arr_modo = 0;
static int arr_movido = 0;   /* el arrastre cambio geometria al menos una vez */
static int menu_abierto = 0, menu_sel = -1, ultimo_min = -1;

/* Diagnostico de compositor (F8-compositor): traza DRAW/ACK/suelta por
   serie. Cuesta unas lineas por frame con actividad; en reposo calla.
   0 = silencio total (produccion). */
#define SRV_DBG 0

/* Si durante la espera de ACK llega un REG/SYNC, la ventana entra en la
   siguiente pasada de frame. */
static int nuevo_marco = 0;

/* Modo modal (etapa 6, Fase J/K): un cliente ejecuta un programa de
   consola/pantalla completa (terminal/explorador + `ejecutar`). Mientras
   dura, el servidor NO toca teclado/raton (son del hijo: misma cola) NI
   pinta (el hijo posee el framebuffer); solo bombea IPC. Sin esto, cada
   evento repintaba el escritorio encima del hijo. Termina con MODAL 0
   del dueño o solo si el dueño muere. Como la shell en texto. */
static int modal_pid = 0;

/* TICK (v2.1-p1): refresco periodico por ventana. El cliente pide
   "TICK s" (1-60 s; 0 cancela) y el servidor le manda DRAW al vencer.
   Sin esto, relojes/indicadores en vivo se congelaban hasta el proximo
   clic (el servidor solo repintaba por eventos). Contenido solo. */
static int tick_sec[NVENT];
static unsigned tick_due[NVENT];

/* Fondo tematico (v2.1-p1): indice de degradado (ver tabla en fondo()).
   "FONDO n" lo cambia + repinta; persiste en fondo.cfg. */
static int fondo_idx = 0;
#define N_FONDOS 5

static void marca_sucia(int id);
static void dano_pintado(int k);
static void atiende_tick(int de, char *b, int r);
static void atiende_fondo(int de, char *b, int r);
static void pantalla_recarga(void);   /* VIDEO: relee tamano y reencaja */

/* Repintado por dano (etapa 6, Fase P): ver definicion junto a frame(). */
static int bg_dirty = 1;

/* Region danada del dorso (F8-compositor): union de lo repintado en
   este frame; al final solo se vuelca eso en vez de toda la pantalla.
   Sin esto cada frame movia 2 MB aunque solo cambiara el cursor. */
static int dmg_hay = 0, dmg_x0 = 0, dmg_y0 = 0, dmg_x1 = 0, dmg_y1 = 0;

static void marca_dano(int x, int y, int w, int h) {
    int x1 = x + w, y1 = y + h;
    if (w <= 0 || h <= 0) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > PANT_W) x1 = PANT_W;
    if (y1 > PANT_H) y1 = PANT_H;
    if (x1 <= x || y1 <= y) return;
    if (!dmg_hay) {
        dmg_x0 = x;
        dmg_y0 = y;
        dmg_x1 = x1;
        dmg_y1 = y1;
        dmg_hay = 1;
    } else {
        if (x < dmg_x0) dmg_x0 = x;
        if (y < dmg_y0) dmg_y0 = y;
        if (x1 > dmg_x1) dmg_x1 = x1;
        if (y1 > dmg_y1) dmg_y1 = y1;
    }
}

/* Espera adaptativa (F8-compositor): presupuesto normal 100 ms; si la
   ventana dio un planton (ack_pend), el siguiente se espera hasta
   300 ms antes de rendirse. Las apps rapidas nunca notan el tope
   (el ACK llega y se vuelve al instante); las lentas dejan de
   parpadear a cambio de una pausa acotada. */
static int espera_paciencia[NVENT];
static int win_dirty[NVENT];
static int win_cromo[NVENT];

/* v2.1-P0: degradado vertical por bandas (16 bandas de 36 px) +
   formas discretas. Solo se llama con bg_dirty (nunca por cursor).
   v2.1-p1: paleta segun fondo_idx (0 marino, 1 grafito, 2 bosque,
   3 burdeos, 4 medianoche). */
static const unsigned char FONDO_RT[N_FONDOS] = { 27, 45, 24, 74, 10 };
static const unsigned char FONDO_GT[N_FONDOS] = { 42, 52, 68, 28, 16 };
static const unsigned char FONDO_BT[N_FONDOS] = { 74, 64, 44, 48, 40 };
#define nombre_fondo t_fondo_nombre

static void fondo(void) {
    unsigned char r0 = FONDO_RT[fondo_idx], g0 = FONDO_GT[fondo_idx];
    unsigned char b0 = FONDO_BT[fondo_idx];
    unsigned char r1 = r0 > 15 ? (unsigned char)(r0 - 15) : 0;
    unsigned char g1 = g0 > 22 ? (unsigned char)(g0 - 22) : 0;
    unsigned char b1 = b0 > 34 ? (unsigned char)(b0 - 34) : 0;
    int i, nb = 16;
    int bh = AREA_H / nb;
    u_destino(1);   /* v2.2-post: todo el escritorio compone en el dorso
                       (antes frente: cada repintado parpadeaba) */
    for (i = 0; i < nb; i++) {
        int y = i * bh;
        int h = (i == nb - 1) ? (AREA_H - y) : bh;
        int rr = r0 + (r1 - r0) * i / (nb - 1);
        int gg = g0 + (g1 - g0) * i / (nb - 1);
        int bb = b0 + (b1 - b0) * i / (nb - 1);
        u_rectangulo(0, y, PANT_W, h, RGB24(rr, gg, bb));
    }
    /* Banda + linea de acento + medallones (tonos del tema). */
    {
        int banda = RGB24(r0 + 9 > 255 ? 255 : r0 + 9,
                          g0 + 12 > 255 ? 255 : g0 + 12,
                          b0 + 20 > 255 ? 255 : b0 + 20);
        u_rectangulo(0, 120, PANT_W, 46, banda);
        u_rectangulo(0, 166, PANT_W, 2, T_FONDO_LINEA);
        u_rectangulo(560, 300, 140, 90, banda);
        u_rectangulo(560, 300, 140, 2, T_FONDO_LINEA);
        u_rectangulo(120, 380, 220, 60, banda);
        u_rectangulo(120, 438, 220, 2, T_FONDO_LINEA);
    }
}

/* Rectangulo de fondo recortado a (cx,cy,cw,ch): mismos pixeles que
   fondo() en esa zona (bandas y adornos son deterministicos).
   Para el arrastre: repara solo la zona tocada en vez de borrar toda
   la pantalla y repintar todas las ventanas en cada tick de raton
   (ese bucle global + ACKs de 100 ms era el parpadeo al solapar). */
static void frect(int x, int y, int w, int h, int col,
                  int cx, int cy, int cw, int ch) {
    int ax = x, ay = y, bx = x + w, by = y + h;
    int qx = cx, qy = cy, qx2 = cx + cw, qy2 = cy + ch;
    if (ax < qx) ax = qx;
    if (ay < qy) ay = qy;
    if (bx > qx2) bx = qx2;
    if (by > qy2) by = qy2;
    if (bx > ax && by > ay)
        u_rectangulo(ax, ay, bx - ax, by - ay, col);
}

static void fondo_clip(int cx, int cy, int cw, int ch) {
    unsigned char r0 = FONDO_RT[fondo_idx], g0 = FONDO_GT[fondo_idx];
    unsigned char b0 = FONDO_BT[fondo_idx];
    unsigned char r1 = r0 > 15 ? (unsigned char)(r0 - 15) : 0;
    unsigned char g1 = g0 > 22 ? (unsigned char)(g0 - 22) : 0;
    unsigned char b1 = b0 > 34 ? (unsigned char)(b0 - 34) : 0;
    int i, nb = 16;
    int bh = AREA_H / nb;
    int banda;
    if (cw <= 0 || ch <= 0) return;
    if (cx < 0) { cw += cx; cx = 0; }
    if (cy < 0) { ch += cy; cy = 0; }
    if (cx + cw > PANT_W) cw = PANT_W - cx;
    if (cy + ch > AREA_H) ch = AREA_H - cy;
    if (cw <= 0 || ch <= 0) return;
    u_destino(1);
    for (i = 0; i < nb; i++) {
        int y = i * bh;
        int h = (i == nb - 1) ? (AREA_H - y) : bh;
        int rr = r0 + (r1 - r0) * i / (nb - 1);
        int gg = g0 + (g1 - g0) * i / (nb - 1);
        int bb = b0 + (b1 - b0) * i / (nb - 1);
        frect(0, y, PANT_W, h, RGB24(rr, gg, bb), cx, cy, cw, ch);
    }
    banda = RGB24(r0 + 9 > 255 ? 255 : r0 + 9,
                  g0 + 12 > 255 ? 255 : g0 + 12,
                  b0 + 20 > 255 ? 255 : b0 + 20);
    frect(0, 120, PANT_W, 46, banda, cx, cy, cw, ch);
    frect(0, 166, PANT_W, 2, T_FONDO_LINEA, cx, cy, cw, ch);
    frect(560, 300, 140, 90, banda, cx, cy, cw, ch);
    frect(560, 300, 140, 2, T_FONDO_LINEA, cx, cy, cw, ch);
    frect(120, 380, 220, 60, banda, cx, cy, cw, ch);
    frect(120, 438, 220, 2, T_FONDO_LINEA, cx, cy, cw, ch);
    marca_dano(cx, cy, cw, ch);
}

/* Icono 16x16 propio en (x,y); '.' y letras ajenas = transparente. */
static void dibuja_icono(int idx, int x, int y) {
    int r, c;
    if (idx < 0 || idx > 43) return;
    for (r = 0; r < 16; r++) {
        const char *fila = T_ICONOS[idx][r];
        for (c = 0; c < 16; c++) {
            char ch = fila[c];
            if (ch == '.' || ch == 0) continue;
            {
                int col = t_icon_rgb(ch);
                if (col) u_pixel(x + c, y + r, col);
            }
        }
    }
}

/* Icono segun titulo de ventana (titulos reales de las 6 apps). */
static int icono_app(const char *t) {
    if (!t) return 11;
    if (t[0] == 't' && t[1] == 'e') return 1;   /* terminal */
    if (t[0] == 'e' && t[1] == 'x' && t[2] == 'p') return 2;   /* explorador */
    if (t[0] == 'F' && t[1] == 'a') return 43;  /* FacilBrowser */
    if (t[0] == 'f' && t[1] == 'b') return 43;  /* fbrowser (nombre corto) */
    if (t[0] == 'e' && t[1] == 'd') return 3;   /* editor */
    if (t[0] == 's') return 4;                  /* sysinfo */
    if (t[0] == 'c') return 5;                  /* configura */
    return 11;
}

static void asegura_propiedad(void) {
    u_texto(0, 0, "", C_NEGRO);
    u_destino(1);   /* v2.2-post: reafirma dorso (las apps modales lo
                       dejan en 0 al salir) */
}

static void trae_frente(int id) {
    if (id < 0 || id >= NVENT || !vents[id].usado) return;
    vents[id].z = nz++;
}

/* Barra de titulo tematizada + botones con iconos; zonas de
   interaccion intactas (X: w-20, _: w-40, max: w-60, arrastre: resto). */
static void cromo(ventana_t *v) {
    int maxc;
    char t[24];
    int i = 0, n = 0;
    int act = v->foco ? 1 : 0;
    u_rectangulo(v->x, v->y, v->w, v->h, T_BORDE);
    if (v->h > CROMO_H + 2)
        u_rectangulo(v->x + 1, v->y + CROMO_H, v->w - 2,
                     v->h - CROMO_H - 1, T_VENT_FONDO);
    u_rectangulo(v->x + 1, v->y + CROMO_H, v->w - 2, 1, T_BORDE);
    u_rectangulo(v->x + 1, v->y + 1, v->w - 2, CROMO_H - 1,
                 act ? T_TIT_ACT : T_TIT_INACT);
    u_rectangulo(v->x + v->w - 20, v->y + 1, 19, CROMO_H - 1,
                 T_BTN_CERRAR);
    dibuja_icono(9, v->x + v->w - 19, v->y + 2);
    u_rectangulo(v->x + v->w - 40, v->y + 1, 19, CROMO_H - 1,
                 T_BTN_MIN);
    dibuja_icono(10, v->x + v->w - 39, v->y + 2);
    /* [☐] maximizar/restaurar (v2.1-p1): cuadrado tipico; si esta
       maximizada se dibuja solapado (restaurar). */
    u_rectangulo(v->x + v->w - 60, v->y + 1, 19, CROMO_H - 1,
                 T_BTN_MIN);
    if (v->max) {
        u_rectangulo(v->x + v->w - 56, v->y + 6, 9, 9, T_TIT_TEXTO);
        u_rectangulo(v->x + v->w - 55, v->y + 7, 7, 7, T_BTN_MIN);
        u_rectangulo(v->x + v->w - 52, v->y + 3, 9, 9, T_TIT_TEXTO);
        u_rectangulo(v->x + v->w - 51, v->y + 4, 7, 7, T_BTN_MIN);
    } else {
        u_rectangulo(v->x + v->w - 55, v->y + 5, 10, 10, T_TIT_TEXTO);
        u_rectangulo(v->x + v->w - 54, v->y + 6, 8, 8, T_BTN_MIN);
    }
    dibuja_icono(icono_app(v->titulo), v->x + 4, v->y + 2);
    maxc = (v->w - 120) / 8;
    if (maxc > 23) maxc = 23;
    if (maxc < 0) maxc = 0;
    while (v->titulo[i] && n < maxc) {
        t[n++] = v->titulo[i++];
    }
    t[n] = 0;
    u_texto(v->x + 24, v->y + 3, t, T_TIT_TEXTO);
}

/* Tamano minimo de ventana (v2.2-post): 320x200 (40x11 celdas) para
   que el texto siempre quepa; antes 120x60 dejaba ventanas ilegibles.
   Centralizado: vale para todas las apps (registro y arrastre). */
#define VENT_MIN_W 320
#define VENT_MIN_H 200

/* Agarre de esquina (redimensionar): 3 diagonales sobre la esquina
   inferior derecha. Se dibuja tras el ACK (el DRAW del cliente lo
   taparia si fuese en el cromo). */
static void dibuja_agarre(ventana_t *v) {
    int gx = v->x + v->w - 3, gy = v->y + v->h - 3, d;
    asegura_propiedad();
    for (d = 0; d < 9; d += 3) {
        u_pixel(gx - d, gy, T_PANEL_SEP);
        u_pixel(gx - d - 1, gy, T_PANEL_SEP);
        u_pixel(gx, gy - d, T_PANEL_SEP);
        u_pixel(gx, gy - d - 1, T_PANEL_SEP);
    }
}

/* Sombra simulada 3px (derecha+abajo, tonos progresivos). Se dibuja en
   el pase de cromo; el borrado lo cubre bg_dirty (mover/cerrar/min).
   Nunca invade el panel: recorte a AREA_H. */
static void sombra(ventana_t *v) {
    int s;
    for (s = 1; s <= T_SOMBRA_PX; s++) {
        int col = (s == 1) ? T_SOMBRA_1
                : (s == 2) ? T_SOMBRA_2 : T_SOMBRA_3;
        int sy = v->y + s, sh = v->h;
        int sx = v->x + v->w + s - 1, sw = 1;
        if (sy < 0) { sh += sy; sy = 0; }
        if (sy + sh > AREA_H) sh = AREA_H - sy;
        if (sh > 0 && sx >= 0 && sx < PANT_W)
            u_rectangulo(sx, sy, sw, sh, col);
        sx = v->x + s;
        sw = v->w;
        sy = v->y + v->h + s - 1;
        if (sx < 0) { sw += sx; sx = 0; }
        if (sx + sw > PANT_W) sw = PANT_W - sx;
        if (sw > 0 && sy >= 0 && sy < AREA_H)
            u_rectangulo(sx, sy, sw, 1, col);
    }
}

/* Reloj HH:MM del CMOS (syscall 44). */
static void reloj_txt(char *d) {
    unsigned int v[6];
    if (u_reloj(v)) {
        d[0] = '-';
        d[1] = '-';
        d[2] = ':';
        d[3] = '-';
        d[4] = '-';
        d[5] = 0;
        return;
    }
    d[0] = (char)('0' + (v[3] / 10) % 10);
    d[1] = (char)('0' + v[3] % 10);
    d[2] = ':';
    d[3] = (char)('0' + (v[4] / 10) % 10);
    d[4] = (char)('0' + v[4] % 10);
    d[5] = 0;
}

static const char *menu_app(int i) {
    if (i == 0) return "terminal";
    if (i == 1) return "explorador";
    if (i == 2) return "fbrowser";
    if (i == 3) return "sysinfo";
    if (i == 4) return "editor";
    if (i == 5) return "configura";
    if (i == 6) return "apagar";
    return "reiniciar";
}

static void barra(void) {
    /* v2.1-P0: panel 30px gris azulado, boton F32 con icono, tasklist
       con iconos + indicador de activa, separadores, reloj. */
    int x = 100, k;
    char rh[8];
    u_rectangulo(0, AREA_H, PANT_W, T_BARRA_H, T_PANEL);
    u_rectangulo(0, AREA_H, PANT_W, 1, T_PANEL_BORDE);
    u_rectangulo(2, AREA_H + 3, 94, T_BARRA_H - 6,
                 menu_abierto ? T_SELECCION : T_ACENTO);
    dibuja_icono(0, 6, AREA_H + 6);
    u_texto(26, AREA_H + 9, "FacilOS", T_PANEL_TEXTO);
    u_rectangulo(98, AREA_H + 4, 1, T_BARRA_H - 8, T_PANEL_SEP);
    for (k = 0; k < NVENT; k++) {
        ventana_t *v = &vents[k];
        char t[14];
        int i = 0;
        int act;
        if (!v->usado) continue;
        if (x + 132 > PANT_W - 110) break;
        act = (v->foco && !v->minimizada) ? 1 : 0;
        u_rectangulo(x, AREA_H + 3, 130, T_BARRA_H - 6,
                     act ? T_SELECCION : T_BTN_CARA);
        if (act)
            u_rectangulo(x, AREA_H + T_BARRA_H - 5, 130, 2, T_ACENTO);
        dibuja_icono(icono_app(v->titulo), x + 4, AREA_H + 6);
        while (v->titulo[i] && i < 12) {
            t[i] = v->titulo[i];
            i++;
        }
        t[i] = 0;
        u_texto(x + 24, AREA_H + 9, t, T_PANEL_TEXTO);
        x += 132;
    }
    u_rectangulo(PANT_W - 108, AREA_H + 4, 1, T_BARRA_H - 8,
                 T_PANEL_SEP);
    reloj_txt(rh);
    u_texto(PANT_W - 74, AREA_H + 9, rh, T_PANEL_TEXTO);
}

static void dibuja_menu(void) {
    /* v2.1-P0: menu con cabecera, iconos y seleccion tematizada. */
    int y0 = AREA_H - (CROMO_H + NMENU * 26), i;
    if (!menu_abierto) return;
    u_rectangulo(2, y0, 220, CROMO_H + NMENU * 26, T_MENU_FONDO);
    u_rectangulo(2, y0, 220, CROMO_H, T_MENU_CAB);
    dibuja_icono(0, 6, y0 + 2);
    u_texto(26, y0 + 4, "FacilOS32", T_MENU_TEXTO);
    for (i = 0; i < NMENU; i++) {
        int yy = y0 + CROMO_H + i * 26;
        /* menu_app: 0 terminal, 1 explorador, 2 fbrowser, 3 sysinfo,
           4 editor -> iconos 1, 2, 43, 4, 3; 5 configura -> 5;
           6 apagar -> 41, 7 reiniciar -> 42. */
        int ic = (i == 2) ? 43 : (i == 3) ? 4 : (i == 4) ? 3 :
                 (i == 6) ? 41 : (i == 7) ? 42 : i + 1;
        if (i == menu_sel)
            u_rectangulo(2, yy, 220, 26, T_MENU_SEL);
        dibuja_icono(ic, 8, yy + 5);
        u_texto(30, yy + 6, menu_app(i),
                i == menu_sel ? T_MENU_TEXTO : T_MENU_TEXTO);
    }
    u_rectangulo(2, y0, 220, 1, T_BORDE);
    u_rectangulo(2, y0 + CROMO_H + NMENU * 26 - 1, 220, 1, T_BORDE);
    u_rectangulo(2, y0, 1, CROMO_H + NMENU * 26, T_BORDE);
    u_rectangulo(2 + 220 - 1, y0, 1, CROMO_H + NMENU * 26, T_BORDE);
}

/* Item del menu bajo (x,y) o -1. */
static int menu_en(int x, int y) {
    int y0 = AREA_H - (CROMO_H + NMENU * 26), i;
    if (!menu_abierto) return -1;
    if (x < 2 || x >= 222 || y < y0) return -1;
    i = (y - y0 - CROMO_H) / 26;
    if (i < 0 || i >= NMENU) return -1;
    return i;
}

/* Cursor (etapa 6, Fase P; corregido tras restos vistos en QA): el fondo
   guardado solo vale si nada pinto encima desde la captura. Regla: cada
   frame BORRA primero el cursor viejo (restaura) y lo redibuja al final
   (captura fresca); el movimiento puro hace borra+show sin recomponer.
   Los repintes selectivos nunca tocan el cursor viejo sin borrarlo. */
/* Cursor flecha 12x16 (etapa v2.1-P0): mascara propia; contorno oscuro
   calculado (pixel de forma junto a vacio) + interior claro. Guardado y
   restauracion en crudo (sin cuantizar a paleta: el fondo es RGB). Misma
   disciplina etapa6: borra primero, redibuja al final. */
static unsigned int cur_px[16 * 16];
static int cur_sx = -1, cur_sy = -1;
#define CUR_W 12
#define CUR_H 16

static const char *const CUR_FLECHA[16] = {
    "X...........",
    "XX..........",
    "XXX.........",
    "XXXX........",
    "XXXXX.......",
    "XXXXXX......",
    "XXXXXXX.....",
    "XXXXXXXX....",
    "XXXXXXXXX...",
    "XXXXXXX.....",
    "XXXX.XX.....",
    "XXX...XX....",
    "XX.....XX...",
    "X.......XX..",
    "............",
    "............"
};

static int cur_forma(int dx, int dy) {
    if (dx < 0 || dx >= CUR_W || dy < 0 || dy >= CUR_H) return 0;
    return CUR_FLECHA[dy][dx] == 'X';
}

/* 1 si el pixel de forma es borde (vecino vacio o limite). */
static int cur_borde(int dx, int dy) {
    int ax, ay;
    for (ay = -1; ay <= 1; ay++) {
        for (ax = -1; ax <= 1; ax++) {
            if (!ax && !ay) continue;
            if (!cur_forma(dx + ax, dy + ay)) return 1;
        }
    }
    return 0;
}

/* Vuelca el cursor (posicion vieja ox,oy + nueva mx,my): con doble
   bufer hay que presentar AMBAS; solo la nueva dejaba estelas. */
static void cursor_volcar(int ox, int oy) {
    int x0 = mx, y0 = my, x1 = mx + CUR_W, y1 = my + CUR_H;
    if (ox >= 0 && oy >= 0) {
        if (ox < x0) x0 = ox;
        if (oy < y0) y0 = oy;
        if (ox + CUR_W > x1) x1 = ox + CUR_W;
        if (oy + CUR_H > y1) y1 = oy + CUR_H;
    }
    u_presentar(x0, y0, x1 - x0, y1 - y0);
}

static void cursor_borra(void) {
    int dx, dy;
    if (cur_sx < 0) return;
    asegura_propiedad();
    for (dy = 0; dy < CUR_H; dy++) {
        for (dx = 0; dx < CUR_W; dx++) {
            unsigned int esp, cur, sav;
            if (!cur_forma(dx, dy)) continue;
            /* Color crudo que dibujo show (u_punto devuelve sin
               el bit 24 de marca RGB). */
            esp = cur_borde(dx, dy) ?
                (16u << 16) | (22u << 8) | 31u :
                (242u << 16) | (245u << 8) | 249u;
            cur = u_punto(cur_sx + dx, cur_sy + dy);
            sav = cur_px[dy * CUR_W + dx];
            /* Solo restaura si el pixel sigue siendo el cursor o ya
               es el fondo guardado. Si un repintado (TICK, DRAW
               tardio) lo cambio, el dorso ya esta bien y restaurar
               pisaria contenido nuevo: huella fantasma al sacar el
               raton de una ventana solapada. */
            if (cur == esp || cur == sav)
                u_pixel(cur_sx + dx, cur_sy + dy,
                        (int)(0x01000000u | sav));
        }
    }
    cur_sx = -1;
}

static void cursor_show(void) {
    int dx, dy;
    asegura_propiedad();
    for (dy = 0; dy < CUR_H; dy++) {
        for (dx = 0; dx < CUR_W; dx++) {
            if (!cur_forma(dx, dy)) continue;
            cur_px[dy * CUR_W + dx] = u_punto(mx + dx, my + dy);
        }
    }
    cur_sx = mx;
    cur_sy = my;
    for (dy = 0; dy < CUR_H; dy++) {
        for (dx = 0; dx < CUR_W; dx++) {
            if (!cur_forma(dx, dy)) continue;
            u_pixel(mx + dx, my + dy,
                    cur_borde(dx, dy) ? RGB24(16, 22, 31)
                                     : RGB24(242, 245, 249));
        }
    }
}

/* Ventana superior bajo (x,y) o -1 (minimizadas no cuentan). */
static int ventana_en(int x, int y) {
    int o, k, mejor = -1, mejorz = -1;
    for (o = 0; o < nz + 4; o++) {
        for (k = 0; k < NVENT; k++) {
            ventana_t *v = &vents[k];
            if (!v->usado || !v->visible || v->minimizada || v->z != o)
                continue;
            if (x >= v->x && x < v->x + v->w && y >= v->y &&
                y < v->y + v->h) {
                if (v->z > mejorz) {
                    mejorz = v->z;
                    mejor = k;
                }
            }
        }
    }
    return mejor;
}

static int registra(int pid, int w, int h, const char *tit) {
    int i, z, q = 0;
    if (w < VENT_MIN_W) w = VENT_MIN_W;
    if (h < VENT_MIN_H) h = VENT_MIN_H;
    if (w > PANT_W) w = PANT_W;
    if (h > AREA_H) h = AREA_H;
    for (i = 0; i < NVENT; i++) {
        if (!vents[i].usado) break;
    }
    if (i >= NVENT) return -1;
    z = nz++;
    vents[i].usado = 1;
    vents[i].pid = pid;
    vents[i].w = w;
    vents[i].h = h;
    vents[i].x = (PANT_W - w) / 2 + (i * 60) % 320;
    vents[i].y = (AREA_H - h) / 2 + (i * 40) % 200;
    if (vents[i].x + w > PANT_W) vents[i].x = PANT_W - w;
    if (vents[i].y + h > AREA_H) vents[i].y = AREA_H - h;
    if (vents[i].x < 0) vents[i].x = 0;
    if (vents[i].y < 0) vents[i].y = 0;
    vents[i].z = z;
    vents[i].visible = 1;
    vents[i].minimizada = 0;
    vents[i].max = 0;
    vents[i].edit = 0;
    vents[i].foco = 0;
    vents[i].ack_pend = 0;
    espera_paciencia[i] = 0;
    tick_sec[i] = 0;
    win_dirty[i] = 1;
    win_cromo[i] = 1;
    if (!tit || !tit[0]) tit = "ventana";
    while (tit[q] && q < 23) {
        vents[i].titulo[q] = tit[q];
        q++;
    }
    vents[i].titulo[q] = 0;
    return i;
}

static void libera(int i) {
    if (i < 0 || i >= NVENT) return;
    vents[i].usado = 0;
    vents[i].visible = 0;
    vents[i].minimizada = 0;
    vents[i].max = 0;
    tick_sec[i] = 0;
    if (foco_id == i) foco_id = -1;
    if (arr_id == i) arr_id = -1;
}

/* Libera ventanas de duenos muertos (clientes caidos sin BYE). */
/* Devuelve cuantas libero (para repintar fondo si destapo algo). */
static int barre_muertos(void) {
    int k, n = 0;
    for (k = 0; k < NVENT; k++) {
        unsigned int st[5];
        if (!vents[k].usado) continue;
        if (u_proc_stat(vents[k].pid, st) != 0 || st[0] == 4) {
            libera(k);
            n++;
        }
    }
    return n;
}

/* Avisa FOCO 1/0 a las ventanas afectadas (asincrono, sin ACK). */
static void avisa_foco(int id) {
    int k;
    for (k = 0; k < NVENT; k++) {
        if (!vents[k].usado) continue;
        if (k == id)
            u_ipc_enviar(vents[k].pid, "FOCUS 1", 8);
        else if (vents[k].foco)
            u_ipc_enviar(vents[k].pid, "FOCUS 0", 8);
    }
}

static void fija_foco(int id) {
    int i;
    marca_sucia(foco_id);
    for (i = 0; i < NVENT; i++) vents[i].foco = 0;
    foco_id = -1;
    if (id >= 0 && id < NVENT && vents[id].usado && !vents[id].minimizada) {
        vents[id].foco = 1;
        foco_id = id;
    }
    marca_sucia(foco_id);
    avisa_foco(foco_id);
}

static int siguiente_foco(void) {
    int i, ini = (foco_id + 1) % NVENT;
    for (i = 0; i < NVENT; i++) {
        int k = (ini + i) % NVENT;
        if (vents[k].usado && vents[k].visible && !vents[k].minimizada)
            return k;
    }
    return -1;
}

/* REG en bucle (diferido=0: el frame que sigue ya incluye la ventana)
   o en espera de ACK (diferido=1: entra en la pasada extra). */
static void atiende_reg(int de, const char *b, int r, int diferido) {
    int k = 4, w = 0, h = 0, id;
    char idm[16], tit[24];
    int q = 0;
    /* Un hijo grafico cancela el modal de su padre (ver modal_pid):
       pide ventana en vez de pantalla completa, asi que el escritorio
       sigue vivo. Sin esto, lanzar terminal/editor desde el
       explorador colgaba todo (DRAW nunca llegaba). */
    if (modal_pid && de != modal_pid) {
        int j;
        modal_pid = 0;
        bg_dirty = 1;
        for (j = 0; j < NVENT; j++) {
            win_dirty[j] = 1;
            win_cromo[j] = 1;
        }
    }
    while (k < r && (b[k] < '0' || b[k] > '9')) k++;
    w = u_atoi(b + k);
    while (k < r && b[k] >= '0' && b[k] <= '9') k++;
    while (k < r && (b[k] < '0' || b[k] > '9')) k++;
    h = u_atoi(b + k);
    while (k < r && b[k] >= '0' && b[k] <= '9') k++;
    while (k < r && b[k] == ' ') k++;
    while (k < r && b[k] && q < 23) tit[q++] = b[k++];
    tit[q] = 0;
    id = registra(de, w, h, tit);
    u_print("reg pid=");
    u_print_num(de);
    u_print(" id=");
    u_print_num(id);
    u_print("\n");
    idm[0] = 'I';
    idm[1] = 'D';
    idm[2] = ' ';
    {
        int t = 3;
        char nb[12];
        int j = 0, v = id >= 0 ? id : 0;
        if (!v) nb[j++] = '0';
        else {
            char rv[12];
            int ri = 0;
            while (v && ri < 11) {
                rv[ri++] = (char)('0' + v % 10);
                v /= 10;
            }
            while (ri) nb[j++] = rv[--ri];
        }
        nb[j] = 0;
        j = 0;
        while (nb[j] && t < 15) idm[t++] = nb[j++];
        idm[t] = 0;
    }
    u_ipc_enviar(de, idm, u_strlen(idm) + 1);
    trae_frente(id);
    fija_foco(id);
    if (diferido) nuevo_marco = 1;
}

/* Responde "INF w h" (pantalla real del modo VESA) a un cliente. */
static void envia_inf(int de) {
    char m[24];
    int t = 0, q = 0;
    char nb[12];
    m[t++] = 'I';
    m[t++] = 'N';
    m[t++] = 'F';
    m[t++] = ' ';
    u_itoa(PANT_W, nb);
    while (nb[q] && t < 22) m[t++] = nb[q++];
    m[t++] = ' ';
    q = 0;
    u_itoa(PANT_H, nb);
    while (nb[q] && t < 22) m[t++] = nb[q++];
    m[t] = 0;
    u_ipc_enviar(de, m, t + 1);
}

/* Espera el ACK de la ventana ids[idx] (lease). Nunca bloquea indefinido:
   - cliente muerto -> libera y vuelve;
   - cliente BLOQUEADO -> vuelve al instante (el DRAW ya se envio: si lo
     esperaba despierta con el; si no, lo vera al desatascarse; el bucle
     reenvia cada frame y SYNC repara);
   - cliente vivo pero lento -> espera hasta 2 s y marca ack_pend (se
     salta su DRAW hasta el ACK tardio).
   Sin esto, un cliente bloqueado congela todo el escritorio. */

/* Cubre con las de encima lo que pinto tarde la ventana k (su ACK
   llego cuando otra ya ocupaba su sitio): marca contenido de las que
   la tapan con z mayor. Devuelve cuantas marco. Sin frame aqui. */
/* Espera con presupuesto adaptativo: normal 100 ms; 300 ms si la
   ventana dio un planton reciente. Las apps rapidas nunca esperan. */
static void espera_ack_h(int idx, int tope);
static void espera_ack(int idx) {
    int tope = (idx >= 0 && idx < NVENT && espera_paciencia[idx]) ? 30 : 10;
    espera_ack_h(idx, tope);
}

static int cubre_encima(int k) {
    int r2, n = 0;
    if (k < 0 || k >= NVENT || !vents[k].usado) return 0;
    for (r2 = 0; r2 < NVENT; r2++) {
        ventana_t *a = &vents[r2];
        if (r2 != k && a->usado && !a->minimizada && a->visible &&
            a->z > vents[k].z &&
            a->x < vents[k].x + vents[k].w &&
            vents[k].x < a->x + a->w &&
            a->y < vents[k].y + vents[k].h &&
            vents[k].y < a->y + a->h) {
            win_dirty[r2] = 1;
            n++;
        }
    }
    return n;
}

static void espera_ack_h(int idx, int tope) {
    char b[64];
    int de = 0, t = 0, pid;
    unsigned int st[5];
    if (idx < 0 || idx >= NVENT || !vents[idx].usado) return;
    pid = vents[idx].pid;
    for (;;) {
        if (u_ipc_hay() > 0) {
            int r = u_ipc_recibir(b, 64, &de);
            if (r < 0) continue;
            if (de == pid && r >= 3 && b[0] == 'A' && b[1] == 'C' &&
                b[2] == 'K') {
                if (vents[idx].usado && vents[idx].pid == pid) {
                    vents[idx].ack_pend = 0;
                    dano_pintado(idx);
                    if (t <= 10) espera_paciencia[idx] = 0;
                }
                return;
            }
            if (r >= 3 && b[0] == 'A' && b[1] == 'C' &&
                b[2] == 'K') {
                /* ACK ajeno en espera ajena: no perderlo (antes se
                   tragaba y la ventana quedaba congelada para siempre,
                   con basura de solape persistente). Se cubre encima
                   y se pide marco extra al salir de la espera. */
                int kk;
                for (kk = 0; kk < NVENT; kk++) {
                    if (vents[kk].usado && vents[kk].pid == de) {
                        vents[kk].ack_pend = 0;
                        dano_pintado(kk);
                        if (cubre_encima(kk)) nuevo_marco = 1;
                    }
                }
                continue;
            }
            if (r >= 3 && b[0] == 'B' && b[1] == 'Y' && b[2] == 'E') {
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de) libera(k);
                }
                if (!vents[idx].usado || vents[idx].pid != pid) return;
                continue;
            }
            if (r >= 3 && b[0] == 'R' && b[1] == 'E' && b[2] == 'G') {
                atiende_reg(de, b, r, 1);
                if (!vents[idx].usado || vents[idx].pid != pid) return;
                continue;
            }
            if (r >= 4 && b[0] == 'S' && b[1] == 'Y' && b[2] == 'N' &&
                b[3] == 'C') {
                nuevo_marco = 1;
                continue;
            }
            if (r >= 3 && b[0] == 'I' && b[1] == 'N' && b[2] == 'F') {
                envia_inf(de);
                continue;
            }
            if (r >= 4 && b[0] == 'T' && b[1] == 'I' && b[2] == 'C' &&
                b[3] == 'K') {
                atiende_tick(de, b, r);
                continue;
            }
            if (r >= 4 && b[0] == 'E' && b[1] == 'D' && b[2] == 'I' &&
                b[3] == 'T') {
                int on = (r >= 6 && b[5] == '1') ? 1 : 0;
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de)
                        vents[k].edit = on;
                }
                continue;
            }
            if (r >= 5 && b[0] == 'F' && b[1] == 'O' && b[2] == 'N' &&
                b[3] == 'D') {
                atiende_fondo(de, b, r);
                continue;
            }
            if (r >= 5 && b[0] == 'V' && b[1] == 'I' && b[2] == 'D' &&
                b[3] == 'E') {
                /* VIDEO en espera: reencaja y pide reframe al salir. */
                pantalla_recarga();
                nuevo_marco = 1;
                continue;
            }
            /* MODAL tambien aqui: si llega en espera (el cliente lo manda
               justo antes de bloquearse en ejecutar) y se descarta, el
               modal no se activa nunca. Sin frame: ya estamos en uno. */
            if (r >= 5 && b[0] == 'M' && b[1] == 'O' && b[2] == 'D') {
                int on = (r >= 7 && b[6] == '1') ? 1 : 0;
                int k;
                if (on) {
                    if (!modal_pid) modal_pid = de;
                } else if (de == modal_pid) {
                    modal_pid = 0;
                    bg_dirty = 1;
                    for (k = 0; k < NVENT; k++) {
                        win_dirty[k] = 1;
                        win_cromo[k] = 1;
                    }
                }
                continue;
            }
            continue;
        }
        if (u_proc_stat(pid, st) != 0 || st[0] == 4) {
            int k;
            for (k = 0; k < NVENT; k++) {
                if (vents[k].usado && vents[k].pid == pid) libera(k);
            }
            return;
        }
        if (st[0] == 3) {
            /* BLOQ: no esperar (el DRAW ya se envio; si esperaba este
               DRAW despierta con el, si no lo vera al desatascarse).
               No se marca pendiente: el bucle reenvia cada frame. */
            return;
        }
        if (++t > tope) {
            /* Sin confirmacion: se sigue sin el (carrera de
               planificacion); el ACK tardio reanuda + repinta (ver
               bucle ppal). Con paciencia se anota para la proxima. */
            if (vents[idx].usado && vents[idx].pid == pid) {
                vents[idx].ack_pend = 1;
                espera_paciencia[idx] = 1;
#if SRV_DBG
                u_print("PEND ");
                u_print_num(idx);
                u_print("\n");
#endif
            }
            return;
        }
        u_dormir(10);
    }
}

/* Repintado por dano (etapa 6, Fase P): el fondo azul completo es el
   parpadeo visible; solo se repinta si algo lo destapo (mover, cerrar,
   minimizar, menu, SYNC tras consola). Los clientes solo reciben DRAW si
   su ventana esta sucia (evento, geometria, foco); el cromo/barra/menu/
   cursor (baratos, del servidor) van siempre. (Variables arriba.) */
static void marca_sucia(int id) {
    if (id >= 0 && id < NVENT && vents[id].usado) {
        win_dirty[id] = 1;
        win_cromo[id] = 1;
    }
}

/* Solo contenido (tecla/raton/ACK tardio): el cromo intacto no se toca
   (repintarlo gris en cada tecla era el parpadeo al escribir). */
static void marca_contenido(int id) {
    if (id >= 0 && id < NVENT && vents[id].usado) win_dirty[id] = 1;
}

/* Dana la ventana k tras pintar (ACK): el present unico de frame()
   la vuelca en orden Z. Sin esto solo el auto-present del cliente
   la mostraba: cada cliente volcaba lo suyo en orden de llegada y
   las solapadas parpadeaban/pegaban cada TICK. */
static void dano_pintado(int k) {
    if (k >= 0 && k < NVENT && vents[k].usado)
        marca_dano(vents[k].x, vents[k].y, vents[k].w, vents[k].h);
}

/* Rectangulos que se tocan (bordes excluidos igual que el pintado). */
static int toca(int ax, int ay, int aw, int ah,
                int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

/* Marca contenido+cromo de las ventanas que pisan (x,y,w,h):
   su franja quedo bajo el fondo recien reparado o bajo otra ventana
   repintada y deben redibujarse en este pase (orden Z). */
static void marca_toca(int x, int y, int w, int h) {
    int k;
    for (k = 0; k < NVENT; k++) {
        ventana_t *a = &vents[k];
        if (!a->usado || a->minimizada || !a->visible) continue;
        if (toca(x, y, w, h, a->x, a->y, a->w, a->h)) {
            win_dirty[k] = 1;
            win_cromo[k] = 1;
        }
    }
}

/* TICK s del pid de: refresco cada s segundos (0 cancela). */
static void atiende_tick(int de, char *b, int r) {
    int k = 5, s = 0, j;
    while (k < r && (b[k] < '0' || b[k] > '9')) k++;
    while (k < r && b[k] >= '0' && b[k] <= '9') {
        s = s * 10 + (b[k] - '0');
        k++;
    }
    if (s < 0) s = 0;
    if (s > 60) s = 60;
    for (j = 0; j < NVENT; j++) {
        if (vents[j].usado && vents[j].pid == de) {
            tick_sec[j] = s;
            tick_due[j] = u_ticks() + (unsigned)s * 100u;
        }
    }
}

/* FONDO n: cambia el degradado (0-4), repinta y persiste. */
static void guarda_fondo(void) {
    UFILE *f = u_fopen("fondo.cfg", "w");
    if (f) {
        char c = (char)('0' + fondo_idx);
        u_fwrite(&c, 1, f);
        u_fclose(f);
    }
}

static void atiende_fondo(int de, char *b, int r) {
    int k = 6, n = 0, j;
    (void)de;
    while (k < r && (b[k] < '0' || b[k] > '9')) k++;
    if (k < r) n = b[k] - '0';
    if (n < 0 || n >= N_FONDOS) return;
    if (n == fondo_idx) return;
    fondo_idx = n;
    bg_dirty = 1;
    for (j = 0; j < NVENT; j++) {
        win_dirty[j] = 1;
        win_cromo[j] = 1;
    }
    guarda_fondo();
}

/* Carga fondo.cfg (si existe): un digito 0-4. */
static void carga_fondo(void) {
    UFILE *f = u_fopen("fondo.cfg", "r");
    if (f) {
        char c = 0;
        if (u_fread(&c, 1, f) == 1 && c >= '0' && c < '0' + N_FONDOS)
            fondo_idx = c - '0';
        u_fclose(f);
    }
}

/* Lanza una app del menu con arg = pid del servidor. */
static void lanza(const char *app) {
    char arg[16];
    int pid;
    /* v2.2-f32: apagar/reiniciar son acciones del sistema, no apps:
       syscalls 50/51 (con flush de disco en el nucleo). */
    if (app && app[0] == 'a' && app[1] == 'p') {
        u_print("apagando\n");
        u_apagar();
        return;
    }
    if (app && app[0] == 'r' && app[1] == 'e') {
        u_print("reiniciando\n");
        u_reiniciar();
        return;
    }
    u_itoa(u_mi_pid(), arg);
    pid = u_crear(app, arg);
    u_print("lanza ");
    u_print(app);
    u_print(" pid=");
    u_print_num(pid);
    u_print("\n");
}

/* VIDEO (cambio de resolucion en vivo): relee las dimensiones del
   nucleo y reencaja todo (ventanas sujetas, maximizadas al nuevo
   tamano, fondo y cromos a repintar). */
static void pantalla_recarga(void) {
    unsigned int v[4];
    int k;
    if (u_video_info(v)) return;
    if ((int)v[0] < 640 || (int)v[0] > 2048 ||
        (int)v[1] < 400 || (int)v[1] > 2048) return;
    pan_w = (int)v[0];
    pan_h = (int)v[1];
    area_h = pan_h - T_BARRA_H;
    if (mx > pan_w - 12) mx = pan_w - 12;
    if (my > pan_h - 12) my = pan_h - 12;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    for (k = 0; k < NVENT; k++) {
        ventana_t *w = &vents[k];
        if (!w->usado) continue;
        if (w->max) {
            w->w = pan_w;
            w->h = area_h;
        }
        if (w->w < VENT_MIN_W) w->w = VENT_MIN_W;
        if (w->h < VENT_MIN_H) w->h = VENT_MIN_H;
        if (w->w > pan_w) w->w = pan_w;
        if (w->h > area_h) w->h = area_h;
        if (w->x + w->w > pan_w) w->x = pan_w - w->w;
        if (w->y + w->h > area_h) w->y = area_h - w->h;
        if (w->x < 0) w->x = 0;
        if (w->y < 0) w->y = 0;
        /* La geometria previa de una maximizada se reencaja al
           restaurar (conmuta_max la sujeta al area nueva). */
        if (w->px < 0) w->px = 0;
        if (w->py < 0) w->py = 0;
        if (w->px + w->pw > pan_w) w->px = pan_w - w->pw;
        if (w->py + w->ph > area_h) w->py = area_h - w->ph;
        if (w->px < 0) w->px = 0;
        if (w->py < 0) w->py = 0;
    }
    bg_dirty = 1;
    for (k = 0; k < NVENT; k++) {
        win_dirty[k] = 1;
        win_cromo[k] = 1;
    }
}

static void frame(void) {
    int ids[NVENT], nn = 0, o, k, q;
    char m[32];
    if (barre_muertos()) bg_dirty = 1;
    /* La ventana con foco pudo morir (hijo que sale): pasa el foco a
       la siguiente viva o las teclas no llegan a nadie. */
    if (foco_id >= 0 && foco_id < NVENT && !vents[foco_id].usado)
        fija_foco(siguiente_foco());
    if (modal_pid) {
        /* Modal: no pintar (pantalla+teclas del hijo). Si el dueno murio
           sin avisar, se reanuda con repintado total. */
        unsigned int st[5];
        if (u_proc_stat(modal_pid, st) != 0 || st[0] == 4) {
            modal_pid = 0;
            bg_dirty = 1;
            for (k = 0; k < NVENT; k++) {
                win_dirty[k] = 1;
                win_cromo[k] = 1;
            }
        } else {
            return;
        }
    }
    for (o = 0; o < nz + 4; o++) {
        for (k = 0; k < NVENT; k++) {
            if (vents[k].usado && vents[k].visible &&
                !vents[k].minimizada && vents[k].z == o)
                ids[nn++] = k;
        }
    }
    asegura_propiedad();
    /* Sin reset aqui (F8-compositor): el dano anotado antes de frame
       (fondo_clip de la union viejo/nuevo en arrastre) debe sobrevivir
       hasta el volcado unico del final; resetearlo aqui tiraba el
       borrado y dejaba cromas fantasma. Se resetea tras presentar. */
    if (cur_sx >= 0 && cur_sy >= 0)
        marca_dano(cur_sx, cur_sy, 12, 16);
    cursor_borra();
    if (bg_dirty) {
        int w;
        /* v2.2-post: el fondo pinta sobre las ventanas: todas deben
           repintarse (antes quedaban borradas hasta el proximo clic;
           al lanzar se salvaban por el cambio de foco, pero F1+ESC
           las hacia desaparecer). */
        for (w = 0; w < NVENT; w++) marca_sucia(w);
        fondo();
        marca_dano(0, 0, PANT_W, PANT_H);
        bg_dirty = 0;
    }
    for (q = 0; q < nn; q++) {
        ventana_t *v = &vents[ids[q]];
        int t = 0;
        char nb[12];
        int j, neg, val;
        if (!v->usado || !v->visible || v->minimizada) continue;
        if (v->ack_pend) continue;   /* congelada: nada (cromo intacto) */
        if (!win_dirty[ids[q]] && !win_cromo[ids[q]]) continue;
        asegura_propiedad();
        if (win_cromo[ids[q]]) {
            sombra(v);
            cromo(v);
            win_cromo[ids[q]] = 0;
#if SRV_DBG
            u_print("CROMO ");
            u_print_num(ids[q]);
            u_print(" ");
            u_print_num(v->x);
            u_print(" ");
            u_print_num(v->y);
            u_print("\n");
#endif
            marca_dano(v->x - 6, v->y - 6, v->w + 12, v->h + 12);
        }
        if (!win_dirty[ids[q]]) continue;
        /* La de abajo repinta: las de encima que la toquen tambien,
           o su present deja basura sobre ellas (parpadeo al solapar).
           Van despues en este mismo pase (orden Z). */
        {
            int r2;
            for (r2 = q + 1; r2 < nn; r2++) {
                ventana_t *a = &vents[ids[r2]];
                if (a->x < v->x + v->w && v->x < a->x + a->w &&
                    a->y < v->y + v->h && v->y < a->y + a->h)
                    win_dirty[ids[r2]] = 1;
            }
        }
        m[t++] = 'D';
        m[t++] = 'R';
        m[t++] = 'A';
        m[t++] = 'W';
        m[t++] = ' ';
        /* x */
        j = 0;
        neg = 0;
        val = v->x;
        if (val < 0) { neg = 1; val = -val; }
        if (!val) nb[j++] = '0';
        else {
            char rv[12];
            int ri = 0;
            while (val && ri < 11) {
                rv[ri++] = (char)('0' + val % 10);
                val /= 10;
            }
            while (ri) nb[j++] = rv[--ri];
        }
        nb[j] = 0;
        j = 0;
        if (neg && t < 30) m[t++] = '-';
        while (nb[j] && t < 30) m[t++] = nb[j++];
        m[t++] = ' ';
        /* y */
        j = 0;
        neg = 0;
        val = v->y;
        if (val < 0) { neg = 1; val = -val; }
        if (!val) nb[j++] = '0';
        else {
            char rv[12];
            int ri = 0;
            while (val && ri < 11) {
                rv[ri++] = (char)('0' + val % 10);
                val /= 10;
            }
            while (ri) nb[j++] = rv[--ri];
        }
        nb[j] = 0;
        j = 0;
        if (neg && t < 30) m[t++] = '-';
        while (nb[j] && t < 30) m[t++] = nb[j++];
        m[t++] = ' ';
        /* w h (los clientes viejos los ignoran) */
        {
            int ww[2], f;
            ww[0] = v->w;
            ww[1] = v->h;
            for (f = 0; f < 2; f++) {
                char rb[12];
                int rj = 0, vv = ww[f];
                if (!vv) rb[rj++] = '0';
                else {
                    char rv[12];
                    int ri = 0;
                    while (vv && ri < 11) {
                        rv[ri++] = (char)('0' + vv % 10);
                        vv /= 10;
                    }
                    while (ri) rb[rj++] = rv[--ri];
                }
                rb[rj] = 0;
                rj = 0;
                while (rb[rj] && t < 30) m[t++] = rb[rj++];
                if (f == 0 && t < 30) m[t++] = ' ';
            }
        }
        m[t] = 0;
        /* v2.1-p1: si el buzon esta lleno (rafaga de MOU) el DRAW se
           pierde; no se limpia el flag para reintentarlo (antes quedaba
           la ventana en blanco para siempre). */
        /* Arrastre en curso con geometria cambiada: contenido diferido
           al soltar (ver repara_suelta). Ninguna app pinta a medias de
           un movimiento: ni restos fuera de la union ni parpadeo por
           espera/ACK. El cromo de arriba ya se dibujo; win_dirty queda
           puesto para el repintado de suelta. */
        if (!(arr_id >= 0 && arr_movido)) {
            if (!u_ipc_enviar(v->pid, m, u_strlen(m) + 1)) {
                win_dirty[ids[q]] = 0;
#if SRV_DBG
                u_print("DRAW ");
                u_print_num(ids[q]);
                u_print(" ");
                u_print_num(v->x);
                u_print(" ");
                u_print_num(v->y);
                u_print("\n");
#endif
            }
            espera_ack(ids[q]);
        }
        if (vents[ids[q]].usado) dibuja_agarre(&vents[ids[q]]);
    }
    asegura_propiedad();
    barra();
    marca_dano(0, AREA_H, PANT_W, PANT_H - AREA_H);
    dibuja_menu();
    if (menu_abierto)
        marca_dano(0, AREA_H - (CROMO_H + NMENU * 26) - 2,
                   224, CROMO_H + NMENU * 26 + 4);
    /* v2.2-post: vuelco atomico del dorso (sin esto cada primitiva
       se veia pintar: parpadeo al escribir/arrastrar/redimensionar).
       F8-compositor: solo la region danada (fondo completo si hubo
       bg_dirty); el cursor tiene su propio volcado tras esto. */
    if (dmg_hay) {
#if SRV_DBG
        u_print("VOLC ");
        u_print_num(dmg_x0);
        u_print(" ");
        u_print_num(dmg_y0);
        u_print(" ");
        u_print_num(dmg_x1 - dmg_x0);
        u_print(" ");
        u_print_num(dmg_y1 - dmg_y0);
        u_print("\n");
#endif
        u_presentar(dmg_x0, dmg_y0, dmg_x1 - dmg_x0, dmg_y1 - dmg_y0);
        dmg_hay = 0;
    }
    cursor_show();
    cursor_volcar(-1, -1);   /* el fullscreen ya cubre; solo la nueva */
    if (nuevo_marco) {
        nuevo_marco = 0;
        frame();
    }
}

/* Fin de arrastre/redimension: repintado total + espera acotada a
   que la app vacie sus DRAW pendientes + repintado final. El camino
   incremental por union puede dejar restos (un DRAW tardio pinta
   fuera de su union ya reparada); al soltar no hay mas lotes que lo
   curen. La espera esta acotada (25 x 100 ms max) y el pase extra es
   unico: sin bucle sobre ACKs (evita tormenta con apps lentas). */
static void repara_suelta(int rid) {
    int w2;
#if SRV_DBG
    if (rid >= 0 && rid < NVENT && vents[rid].usado) {
        u_print("SUELTA ");
        u_print_num(rid);
        u_print(" ");
        u_print_num(vents[rid].x);
        u_print(" ");
        u_print_num(vents[rid].y);
        u_print(" mov=");
        u_print_num(arr_movido);
        u_print("\n");
    }
#endif
    bg_dirty = 1;
    frame();
    for (w2 = 0; w2 < 25 && rid >= 0 && rid < NVENT &&
         vents[rid].usado && vents[rid].ack_pend; w2++)
        espera_ack_h(rid, 10);
    if (w2 > 0 && rid >= 0 && rid < NVENT && vents[rid].usado) {
        bg_dirty = 1;
        frame();
    }
}

/* Maximizar/restaurar (v2.1-p1): guarda geometria, ocupa el area de
   trabajo (0,0,PANT_W,AREA_H); restaurar devuelve la previa. */
static void conmuta_max(int id) {
    ventana_t *v;
    if (id < 0 || id >= NVENT || !vents[id].usado) return;
    v = &vents[id];
    if (!v->max) {
        v->px = v->x;
        v->py = v->y;
        v->pw = v->w;
        v->ph = v->h;
        v->x = 0;
        v->y = 0;
        v->w = PANT_W;
        v->h = AREA_H;
        v->max = 1;
    } else {
        v->x = v->px;
        v->y = v->py;
        v->w = v->pw;
        v->h = v->ph;
        v->max = 0;
    }
    bg_dirty = 1;
    marca_sucia(id);
    frame();
}

/* Cierra la ventana id: QUIT + libera SIN esperar (fire-and-forget).
   Esperar aqui colgaria al servidor si el cliente esta bloqueado en
   otra syscall; el cliente sale al desatascarse (atiende su QUIT). */
static void cierra_ventana(int id) {
    if (id < 0 || id >= NVENT || !vents[id].usado) return;
    u_ipc_enviar(vents[id].pid, "QUIT", 5);
    libera(id);
    bg_dirty = 1;
    if (foco_id < 0) fija_foco(siguiente_foco());
    frame();
}

/* Enviar tecla al foco (cualquier codigo salvo TAB/ESC, ya filtrados). */
static void tecla_a_foco(int code) {
    char m[16];
    int t = 4, v = code, j = 0;
    char nb[12];
    if (foco_id < 0) {
        return;
    }
    m[0] = 'K';
    m[1] = 'E';
    m[2] = 'Y';
    m[3] = ' ';
    if (v < 0) {
        m[t++] = '-';
        v = -v;
    }
    if (!v) nb[j++] = '0';
    else {
        char rv[12];
        int ri = 0;
        while (v && ri < 11) {
            rv[ri++] = (char)('0' + v % 10);
            v /= 10;
        }
        while (ri) nb[j++] = rv[--ri];
    }
    nb[j] = 0;
    j = 0;
    while (nb[j] && t < 15) m[t++] = nb[j++];
    m[t] = 0;
    u_ipc_enviar(vents[foco_id].pid, m, u_strlen(m) + 1);
    marca_contenido(foco_id);
    frame();
}

/* Avisa MOU x y bot giro a la ventana (giro 0 si no hubo rueda). */
static void avisa_mou(int id) {
    char m[40], nx[12], ny[12], nb[12], nw[12];
    int t = 0, q = 0;
    if (id < 0 || id >= NVENT || !vents[id].usado) return;
    u_itoa(mx, nx);
    u_itoa(my, ny);
    u_itoa(mb, nb);
    u_itoa(giro_lote, nw);
    m[t++] = 'M';
    m[t++] = 'O';
    m[t++] = 'U';
    m[t++] = ' ';
    while (nx[q] && t < 37) m[t++] = nx[q++];
    m[t++] = ' ';
    q = 0;
    while (ny[q] && t < 37) m[t++] = ny[q++];
    m[t++] = ' ';
    q = 0;
    while (nb[q] && t < 37) m[t++] = nb[q++];
    m[t++] = ' ';
    q = 0;
    while (nw[q] && t < 37) m[t++] = nw[q++];
    m[t] = 0;
    u_ipc_enviar(vents[id].pid, m, t + 1);
}

static void clic_izquierdo(void) {
    int id, mi, bx;
    /* 1. menu abierto: item o cierre por clic fuera */
    if (menu_abierto) {
        mi = menu_en(mx, my);
        if (mi >= 0) {
            menu_abierto = 0;
            bg_dirty = 1;
            lanza(menu_app(mi));
            frame();
            return;
        }
        if (!(mx >= 2 && mx < 96 && my >= AREA_H)) {
            menu_abierto = 0;
            bg_dirty = 1;
            frame();
            return;
        }
    }
    /* 2. boton FacilOS */
    if (mx >= 2 && mx < 96 && my >= AREA_H) {
        menu_abierto = 1;
        menu_sel = -1;
        bg_dirty = 1;
        frame();
        return;
    }
    /* 3. botones de la barra (ventanas) */
    bx = 100;
    for (id = 0; id < NVENT; id++) {
        ventana_t *v = &vents[id];
        if (!v->usado) continue;
        if (bx + 132 > PANT_W - 110) break;
        if (mx >= bx && mx < bx + 130 && my >= AREA_H) {
            if (v->minimizada) {
                v->minimizada = 0;
                v->visible = 1;
                trae_frente(id);
                fija_foco(id);
                marca_sucia(id);
            } else if (foco_id == id) {
                v->minimizada = 1;
                v->visible = 0;
                bg_dirty = 1;
                fija_foco(siguiente_foco());
            } else {
                trae_frente(id);
                fija_foco(id);
            }
            frame();
            return;
        }
        bx += 132;
    }
    /* 4. ventana bajo el cursor */
    id = ventana_en(mx, my);
    if (id < 0) {
        int ox = cur_sx, oy = cur_sy;
        cursor_borra();
        cursor_show();
        cursor_volcar(ox, oy);   /* v2.2-post: ver camino puro */
        return;
    }
    trae_frente(id);
    fija_foco(id);
    {
        ventana_t *v = &vents[id];
        /* [X] cerrar */
        if (mx >= v->x + v->w - 20 && mx < v->x + v->w &&
            my >= v->y && my < v->y + CROMO_H) {
            cierra_ventana(id);
            return;
        }
        /* [_] minimizar */
        if (mx >= v->x + v->w - 40 && mx < v->x + v->w - 20 &&
            my >= v->y && my < v->y + CROMO_H) {
            v->minimizada = 1;
            v->visible = 0;
            bg_dirty = 1;
            fija_foco(siguiente_foco());
            frame();
            return;
        }
        /* [cuadrado] maximizar/restaurar (v2.1-p1) */
        if (mx >= v->x + v->w - 60 && mx < v->x + v->w - 40 &&
            my >= v->y && my < v->y + CROMO_H) {
            conmuta_max(id);
            return;
        }
        /* esquina: redimensionar */
        if (mx >= v->x + v->w - 14 && my >= v->y + v->h - 14) {
            arr_id = id;
            arr_modo = 1;
            arr_movido = 0;
            frame();
            return;
        }
        /* titulo: arrastrar (si esta maximizada, primero restaura) */
        if (my < v->y + CROMO_H) {
            arr_id = id;
            arr_movido = 0;
            if (v->max) {
                v->x = v->px;
                v->y = v->py;
                v->w = v->pw;
                v->h = v->ph;
                v->max = 0;
                bg_dirty = 1;
                marca_sucia(id);
            }
            arr_dx = mx - v->x;
            arr_dy = my - v->y;
            arr_modo = 0;
            frame();
            return;
        }
        /* contenido: avisa MOU (lo hace la rama de raton) */
        avisa_mou(id);
        marca_contenido(id);
        frame();
    }
}

int main(void) {
    char b[64];
    int de = 0;
    u_print("servidor escritorio v1 (etapa 5/6)\n");
    {
        /* v2.1-p1: dimensiones reales del nucleo ("W H"). */
        const char *a = u_args();
        int w = 0, h = 0;
        while (*a == ' ') a++;
        while (*a >= '0' && *a <= '9') {
            w = w * 10 + (*a - '0');
            a++;
        }
        while (*a == ' ') a++;
        while (*a >= '0' && *a <= '9') {
            h = h * 10 + (*a - '0');
            a++;
        }
        if (w >= 640 && w <= 2048 && h >= 400 && h <= 2048) {
            pan_w = w;
            pan_h = h;
            area_h = h - T_BARRA_H;
            mx = w / 2;
            my = h / 2;
        }
    }
    carga_fondo();
    asegura_propiedad();
    frame();
    for (;;) {
        if (u_ipc_hay() > 0) {
            int r = u_ipc_recibir(b, 64, &de);
            if (r < 0) continue;
            if (r >= 3 && b[0] == 'R' && b[1] == 'E' && b[2] == 'G') {
                atiende_reg(de, b, r, 0);
                frame();
            } else if (r >= 3 && b[0] == 'B' && b[1] == 'Y' &&
                       b[2] == 'E') {
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de) libera(k);
                }
                bg_dirty = 1;
                if (foco_id < 0) fija_foco(siguiente_foco());
                frame();
            } else if (r >= 4 && b[0] == 'S' && b[1] == 'Y' && b[2] == 'N' &&
                       b[3] == 'C') {
                int k;
                bg_dirty = 1;
                for (k = 0; k < NVENT; k++) marca_sucia(k);
                frame();
            } else if (r >= 3 && b[0] == 'I' && b[1] == 'N' &&
                       b[2] == 'F') {
                envia_inf(de);
            } else if (r >= 5 && b[0] == 'M' && b[1] == 'O' &&
                       b[2] == 'D') {
                /* MODAL 1 (suspender, dueno=emisor) / MODAL 0 (reanudar).
                   Solo el dueno reanuda; un segundo modal se ignora. */
                int on = (r >= 7 && b[6] == '1') ? 1 : 0;
                if (on) {
                    if (!modal_pid) modal_pid = de;
                } else if (de == modal_pid) {
                    modal_pid = 0;
                    bg_dirty = 1;
                    {
                        int k;
                        for (k = 0; k < NVENT; k++) {
                            win_dirty[k] = 1;
                            win_cromo[k] = 1;
                        }
                    }
                    frame();
                }
            } else if (r >= 4 && b[0] == 'T' && b[1] == 'I' && b[2] == 'C' &&
                       b[3] == 'K') {
                atiende_tick(de, b, r);
            } else if (r >= 4 && b[0] == 'E' && b[1] == 'D' && b[2] == 'I' &&
                       b[3] == 'T') {
                int on = (r >= 6 && b[5] == '1') ? 1 : 0;
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de)
                        vents[k].edit = on;
                }
            } else if (r >= 5 && b[0] == 'F' && b[1] == 'O' && b[2] == 'N' &&
                       b[3] == 'D') {
                atiende_fondo(de, b, r);
                frame();
            } else if (r >= 5 && b[0] == 'V' && b[1] == 'I' && b[2] == 'D' &&
                       b[3] == 'E') {
                /* VIDEO: la resolucion cambio en vivo (ver configura). */
                pantalla_recarga();
                frame();
            } else if (r >= 3 && b[0] == 'A' && b[1] == 'C' &&
                       b[2] == 'K') {
                /* ACK tardio: la app pinto tarde (ya sin auto-present:
                   vuelca el servidor). Lo que haya encima queda tapado
                   en el dorso: reponen encima las que la pisen (ver
                   cubre_encima); a esta no se le reenvia DRAW (acaba
                   de pintar). Sin nada encima se vuelca su rectangulo
                   directo (si no, quedaria invisible hasta el proximo
                   frame). */
                int urge = 0, k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de) {
                        vents[k].ack_pend = 0;
                        dano_pintado(k);
                        if (cubre_encima(k)) urge = 1;
                    }
                }
#if SRV_DBG
                u_print("TARD pid=");
                u_print_num(de);
                u_print(" urge=");
                u_print_num(urge);
                u_print("\n");
#endif
                if (urge) {
                    frame();
                } else {
                    for (k = 0; k < NVENT; k++) {
                        if (vents[k].usado && vents[k].pid == de)
                            u_presentar(vents[k].x, vents[k].y,
                                        vents[k].w, vents[k].h);
                    }
                }
            }
            /* ACK y otros se consumen sin mas */
        } else if (!modal_pid && u_hay_tecla()) {
            int k = u_tecla();
            if (k == 27) {
                if (menu_abierto) {
                    menu_abierto = 0;
                    bg_dirty = 1;
                    frame();
                } else if (foco_id >= 0 && vents[foco_id].usado &&
                           vents[foco_id].edit) {
                    /* v2.2: modo edicion (terminal): ESC llega a la app
                       (cierra su selector / limpia linea). Salir del
                       escritorio sigue con la X o sin ventanas en edicion. */
                    tecla_a_foco(k);
                } else {
                    int i;
                    for (i = 0; i < NVENT; i++) {
                        if (!vents[i].usado) continue;
                        u_ipc_enviar(vents[i].pid, "QUIT", 5);
                    }
                    for (i = 0; i < NVENT; i++) vents[i].usado = 0;
                    u_print("servidor terminado\n");
                    return 0;
                }
            } else if (k == TEC_MENU_U) {
                menu_abierto = !menu_abierto;
                menu_sel = -1;
                bg_dirty = 1;
                frame();
            } else if (menu_abierto &&
                       (k == TEC_ARRIBA_U || k == TEC_ABAJO_U)) {
                if (k == TEC_ARRIBA_U)
                    menu_sel = menu_sel <= 0 ? NMENU - 1 : menu_sel - 1;
                else
                    menu_sel = menu_sel < 0 ? 0 : (menu_sel + 1) % NMENU;
                frame();
            } else if (menu_abierto &&
                       (k == 13 || k == ' ')) {
                if (menu_sel >= 0) {
                    int mi = menu_sel;
                    menu_abierto = 0;
                    bg_dirty = 1;
                    lanza(menu_app(mi));
                    frame();
                }
            } else if (k == 9) {
                if (foco_id >= 0 && vents[foco_id].usado &&
                    vents[foco_id].edit) {
                    /* v2.2: modo edicion: TAB llega a la app
                       (autocompletado). Sin edicion sigue ciclando foco
                       (el driver ya emite TAB: antes era codigo muerto). */
                    tecla_a_foco(k);
                } else {
                    fija_foco(siguiente_foco());
                    frame();
                }
            } else {
                tecla_a_foco(k);
            }
        } else if (!modal_pid && u_hay_raton()) {
            unsigned int e[4];
            int pulsado = 0;
            /* Drena la rafaga: una sola composicion por lote de paquetes
               (si no, cada paquete reharía toda la pantalla). Solo importan
               la ultima posicion, si hubo pulsacion y el giro total. */
            while (u_hay_raton()) {
                int nb;
                if (u_raton(e)) break;
                nb = (int)e[2];
                if (!(mb & 1) && (nb & 1)) pulsado = 1;
                mx = (int)e[0];
                my = (int)e[1];
                mb = nb;
                giro_lote += (int)e[3];
            }
            if (mx > PANT_W - 12) mx = PANT_W - 12;
            if (my > PANT_H - 12) my = PANT_H - 12;
            if (mx < 0) mx = 0;
            if (my < 0) my = 0;
            /* arrastre en curso con boton */
            if (arr_id >= 0 && (mb & 1)) {
                ventana_t *v = &vents[arr_id];
                if (v->usado) {
                    int ox = v->x, oy = v->y, ow = v->w, oh = v->h;
                    if (arr_modo == 0) {
                        v->x = mx - arr_dx;
                        v->y = my - arr_dy;
                        if (v->x < 0) v->x = 0;
                        if (v->y < 0) v->y = 0;
                        if (v->x + v->w > PANT_W)
                            v->x = PANT_W - v->w;
                        if (v->y + v->h > AREA_H)
                            v->y = AREA_H - v->h;
                    } else {
                        v->w = mx - v->x;
                        v->h = my - v->y;
                        if (v->w < VENT_MIN_W) v->w = VENT_MIN_W;
                        if (v->h < VENT_MIN_H) v->h = VENT_MIN_H;
                        if (v->x + v->w > PANT_W)
                            v->w = PANT_W - v->x;
                        if (v->y + v->h > AREA_H)
                            v->h = AREA_H - v->y;
                    }
                    if (v->x != ox || v->y != oy ||
                        v->w != ow || v->h != oh) {
                        arr_movido = 1;
                        /* Solo la union viejo/nuevo (+sombra): fondo
                           recortado y repintan las que la pisan. Sin
                           el bg_dirty global, que borraba todas las
                           ventanas y las repintaba una a una con ACK
                           (carrera que pegaba y parpadeaba al solapar). */
                        int ux = ox < v->x ? ox : v->x;
                        int uy = oy < v->y ? oy : v->y;
                        int ex = ox + ow > v->x + v->w ?
                               ox + ow : v->x + v->w;
                        int ey = oy + oh > v->y + v->h ?
                               oy + oh : v->y + v->h;
                        /* +8/-4 simetrico: cubre la sombra (3px) tambien a
                           derecha/abajo; con +4 solo izq/arriba y la
                           franja vieja sobrevivía al arrastre. */
                        int uw = ex - ux + 8, uh = ey - uy + 8;
                        ux -= 4;
                        uy -= 4;
                        if (ux < 0) { uw += ux; ux = 0; }
                        if (uy < 0) { uh += uy; uy = 0; }
                        if (ux + uw > PANT_W) uw = PANT_W - ux;
                        if (uy + uh > AREA_H) uh = AREA_H - uy;
#if SRV_DBG
                        u_print("MOV ");
                        u_print_num(ox);
                        u_print(" ");
                        u_print_num(oy);
                        u_print("->");
                        u_print_num(v->x);
                        u_print(" ");
                        u_print_num(v->y);
                        u_print(" u");
                        u_print_num(ux);
                        u_print(" ");
                        u_print_num(uy);
                        u_print(" ");
                        u_print_num(uw);
                        u_print(" ");
                        u_print_num(uh);
                        u_print("\n");
#endif
                        fondo_clip(ux, uy, uw, uh);
                        marca_toca(ux, uy, uw, uh);
                    }
                }
                frame();
            } else {
                if (arr_id >= 0 && !(mb & 1)) {
                    int rid = arr_id;
                    int mov = arr_movido;
                    arr_id = -1;
                    arr_movido = 0;
                    /* Solo si hubo movimiento: un clic sin
                       desplazar no necesita repintado total. */
                    if (mov) repara_suelta(rid);
                }
                /* hover del menu */
                if (menu_abierto) {
                    int mi = menu_en(mx, my);
                    if (mi != menu_sel) {
                        menu_sel = mi;
                        frame();
                        mb_antes = mb;
                        continue;
                    }
                }
                /* flanco de clic izquierdo (del lote) */
                if (pulsado) {
                    clic_izquierdo();
                } else {
                    /* movimiento sobre contenido: MOU siempre (estado
                       para la app); DRAW solo si cambia algo visible
                       (ventana, botones o arrastre con boton): el
                       reframe ~7/s por movimiento era el parpadeo
                       constante (v2.1-p1). */
                    int id = ventana_en(mx, my);
                    if (id >= 0) {
                        avisa_mou(id);
                        if (id != mou_id || mb != mou_bot || (mb & 1) ||
                            giro_lote) {
                            mou_id = id;
                            mou_bot = mb;
                            marca_contenido(id);
                            frame();
                        } else {
                            int ox = cur_sx, oy = cur_sy;
                            cursor_borra();
                            cursor_show();
                            /* v2.2-post: con doble bufer el cursor solo
                               existe en el dorso hasta volcarlo */
                            cursor_volcar(ox, oy);
                        }
                    } else {
                        int ox = cur_sx, oy = cur_sy;
                        mou_id = -1;
                        /* escritorio: via rapida sin recomponer */
                        cursor_borra();
                        cursor_show();
                        /* v2.2-post: igual que el camino con ventana */
                        cursor_volcar(ox, oy);
                    }
                }
                giro_lote = 0;   /* consumido por el aviso de este lote */
            }
            mb_antes = mb;
        } else {
            /* Reposo: TICKs vencidos + cambio de minuto del panel.
               Sin esto, relojes en vivo se congelaban hasta el proximo
               clic (v2.1-p1). */
            unsigned int tj[6];
            unsigned ahora = (unsigned)u_ticks();
            int urge = 0, k;
            for (k = 0; k < NVENT; k++) {
                if (vents[k].usado && vents[k].visible &&
                    !vents[k].minimizada && tick_sec[k] > 0 &&
                    ahora >= tick_due[k]) {
                    tick_due[k] = ahora + (unsigned)tick_sec[k] * 100u;
                    win_dirty[k] = 1;
                    urge = 1;
                }
            }
            /* Reintento: un DRAW perdido (buzon lleno) deja el flag
               puesto; se reenvia aqui (v2.1-p1). */
            if (!urge) {
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].visible &&
                        !vents[k].minimizada &&
                        (win_dirty[k] || win_cromo[k])) {
                        urge = 1;
                        break;
                    }
                }
            }
            if (!u_reloj(tj) && (int)tj[4] != ultimo_min) {
                ultimo_min = (int)tj[4];
                urge = 1;
            }
            if (urge) frame();
            u_dormir(20);
        }
    }
}
