#include "ulib.h"
#include "tema.h"

void __main(void) {
}

/* servidor (etapa 5/6): escritorio FacilOS32 por leases.
 * Compone fondo + ventanas + barra de tareas + menu + cursor; gestiona
 * foco, orden Z, arrastre, botones de ventana, menu de apps y reloj.
 * Protocolo (<=64 B, texto). C->S: REG w h titulo / BYE / ACK / SYNC / INF.
 * S->C: ID n / DRAW x y w h / FOCUS 0-1 / KEY code / MOU x y bot /
 * QUIT / INF w h. La shell queda excluida mientras corre (como el menu).
 */

#define NVENT 16
#define PANT_W 800
#define PANT_H 600
#define BARRA_H T_BARRA_H
#define AREA_H (PANT_H - BARRA_H)
#define CROMO_H 20
#define NMENU 5

typedef struct {
    int usado;
    int pid;
    int w, h, x, y, z;
    int visible;
    int minimizada;
    int foco;
    int ack_pend;       /* DRAW sin confirmar (cliente lento/bloqueado) */
    char titulo[24];
} ventana_t;

static ventana_t vents[NVENT];
static int nz = 0;
static int foco_id = -1;
static int mx = 400, my = 300, mb = 0, mb_antes = 0;
/* Antiparadeo en movimiento (etapa 6, Fase P): como mucho un reframe por
   movimiento cada 150 ms; entre medias el MOU llega igual (estado) y el
   cursor va por via rapida. Clics/teclas/drags no limitan. */
static int ult_mou = 0;
static int arr_id = -1, arr_dx = 0, arr_dy = 0, arr_modo = 0;
static int menu_abierto = 0, menu_sel = -1, ultimo_min = -1;

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

static void marca_sucia(int id);

/* Repintado por dano (etapa 6, Fase P): ver definicion junto a frame(). */
static int bg_dirty = 1;
static int win_dirty[NVENT];
static int win_cromo[NVENT];

static void fondo(void) {
    /* v2.1-P0: degradado vertical por bandas (16 bandas de 36 px) +
       formas discretas. Solo se llama con bg_dirty (nunca por cursor). */
    static const unsigned char rt[16] = {
        27, 26, 25, 24, 23, 22, 21, 20,
        19, 18, 17, 16, 15, 14, 13, 12
    };
    static const unsigned char gt[16] = {
        42, 41, 40, 39, 37, 36, 35, 34,
        32, 31, 30, 29, 27, 26, 24, 20
    };
    static const unsigned char bt[16] = {
        74, 72, 70, 68, 66, 64, 62, 60,
        58, 56, 54, 52, 49, 47, 44, 40
    };
    int i;
    u_destino(0);
    for (i = 0; i < 16; i++) {
        int y = i * (AREA_H / 16);
        int h = (i == 15) ? (AREA_H - y) : (AREA_H / 16);
        u_rectangulo(0, y, PANT_W, h, RGB24(rt[i], gt[i], bt[i]));
    }
    /* Banda diagonal discreta + linea de acento + medallon. */
    u_rectangulo(0, 120, PANT_W, 46, T_FONDO_BANDA);
    u_rectangulo(0, 166, PANT_W, 2, T_FONDO_LINEA);
    u_rectangulo(560, 300, 140, 90, T_FONDO_BANDA);
    u_rectangulo(560, 300, 140, 2, T_FONDO_LINEA);
    u_rectangulo(120, 380, 220, 60, T_FONDO_BANDA);
    u_rectangulo(120, 438, 220, 2, T_FONDO_LINEA);
}

/* Icono 16x16 propio en (x,y); '.' y letras ajenas = transparente. */
static void dibuja_icono(int idx, int x, int y) {
    int r, c;
    if (idx < 0 || idx > 11) return;
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

/* Icono segun titulo de ventana (titulos reales de las 5 apps). */
static int icono_app(const char *t) {
    if (!t) return 11;
    if (t[0] == 't' && t[1] == 'e') return 1;   /* terminal */
    if (t[0] == 'e' && t[1] == 'x' && t[2] == 'p') return 2;   /* explorador */
    if (t[0] == 'e' && t[1] == 'd') return 3;   /* editor */
    if (t[0] == 's') return 4;                  /* sysinfo */
    if (t[0] == 'c') return 5;                  /* configura */
    return 11;
}

static void asegura_propiedad(void) {
    u_texto(0, 0, "", C_NEGRO);
}

static void trae_frente(int id) {
    if (id < 0 || id >= NVENT || !vents[id].usado) return;
    vents[id].z = nz++;
}

/* Barra de titulo tematizada + botones con iconos; zonas de
   interaccion intactas (X: w-20, _: w-40, arrastre: resto). */
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
    if (i == 2) return "sysinfo";
    if (i == 3) return "editor";
    return "configura";
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
        if (x + 132 > 690) break;
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
    u_rectangulo(692, AREA_H + 4, 1, T_BARRA_H - 8, T_PANEL_SEP);
    reloj_txt(rh);
    u_texto(726, AREA_H + 9, rh, T_PANEL_TEXTO);
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
        /* menu_app: 0 terminal, 1 explorador, 2 sysinfo, 3 editor,
           4 configura -> iconos 1, 2, 4, 3, 5. */
        int ic = (i == 2) ? 4 : (i == 3) ? 3 : i + 1;
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

static void cursor_borra(void) {
    int dx, dy;
    if (cur_sx < 0) return;
    asegura_propiedad();
    for (dy = 0; dy < CUR_H; dy++) {
        for (dx = 0; dx < CUR_W; dx++) {
            if (!cur_forma(dx, dy)) continue;
            /* Restaura crudo con marca RGB (el fondo es RGB). */
            u_pixel(cur_sx + dx, cur_sy + dy,
                    (int)(0x01000000u | cur_px[dy * CUR_W + dx]));
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
    if (w < 120) w = 120;
    if (h < 60) h = 60;
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
    vents[i].foco = 0;
    vents[i].ack_pend = 0;
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

/* Responde "INF w h" (geometria util) a un cliente. */
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
    u_itoa(AREA_H, nb);
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
static void espera_ack(int idx) {
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
                if (vents[idx].usado && vents[idx].pid == pid)
                    vents[idx].ack_pend = 0;
                return;
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
        if (++t > 10) {    /* ~100 ms: si no confirmo rapido se sigue
                             sin el (carrera de planificacion); el ACK
                             tardio reanuda + repinta (ver bucle ppal) */
            if (vents[idx].usado && vents[idx].pid == pid)
                vents[idx].ack_pend = 1;
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

/* Lanza una app del menu con arg = pid del servidor. */
static void lanza(const char *app) {
    char arg[16];
    int pid;
    u_itoa(u_mi_pid(), arg);
    pid = u_crear(app, arg);
    u_print("lanza ");
    u_print(app);
    u_print(" pid=");
    u_print_num(pid);
    u_print("\n");
}

static void frame(void) {
    int ids[NVENT], nn = 0, o, k, q;
    char m[32];
    if (barre_muertos()) bg_dirty = 1;
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
    cursor_borra();
    if (bg_dirty) {
        fondo();
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
        }
        if (!win_dirty[ids[q]]) continue;
        win_dirty[ids[q]] = 0;
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
        u_ipc_enviar(v->pid, m, u_strlen(m) + 1);
        espera_ack(ids[q]);
    }
    asegura_propiedad();
    barra();
    dibuja_menu();
    cursor_show();
    if (nuevo_marco) {
        nuevo_marco = 0;
        frame();
    }
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
    if (foco_id < 0) return;
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
        if (bx + 132 > 690) break;
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
        cursor_borra();
        cursor_show();
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
        /* esquina: redimensionar */
        if (mx >= v->x + v->w - 14 && my >= v->y + v->h - 14) {
            arr_id = id;
            arr_modo = 1;
            frame();
            return;
        }
        /* titulo: arrastrar */
        if (my < v->y + CROMO_H) {
            arr_id = id;
            arr_dx = mx - v->x;
            arr_dy = my - v->y;
            arr_modo = 0;
            frame();
            return;
        }
        /* contenido: avisa MOU (lo hace la rama de raton) */
        {
            char m[32], nx[12], ny[12], nb[12];
            int t = 0, q = 0;
            u_itoa(mx, nx);
            u_itoa(my, ny);
            u_itoa(mb, nb);
            m[t++] = 'M';
            m[t++] = 'O';
            m[t++] = 'U';
            m[t++] = ' ';
            while (nx[q] && t < 29) m[t++] = nx[q++];
            m[t++] = ' ';
            q = 0;
            while (ny[q] && t < 29) m[t++] = ny[q++];
            m[t++] = ' ';
            q = 0;
            while (nb[q] && t < 29) m[t++] = nb[q++];
            m[t] = 0;
            u_ipc_enviar(vents[id].pid, m, t + 1);
            marca_contenido(id);
        }
        frame();
    }
}

int main(void) {
    char b[64];
    int de = 0;
    u_print("servidor escritorio v1 (etapa 5/6)\n");
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
            } else if (r >= 3 && b[0] == 'A' && b[1] == 'C' &&
                       b[2] == 'K') {
                /* ACK tardio: reanuda + repinta (el contenido llego tarde
                   y nadie lo redibuja si no). */
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de) {
                        vents[k].ack_pend = 0;
                        marca_contenido(k);
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
                fija_foco(siguiente_foco());
                frame();
            } else {
                tecla_a_foco(k);
            }
        } else if (!modal_pid && u_hay_raton()) {
            unsigned int e[3];
            int pulsado = 0;
            /* Drena la rafaga: una sola composicion por lote de paquetes
               (si no, cada paquete reharía toda la pantalla). Solo importan
               la ultima posicion y si hubo pulsacion. */
            while (u_hay_raton()) {
                int nb;
                if (u_raton(e)) break;
                nb = (int)e[2];
                if (!(mb & 1) && (nb & 1)) pulsado = 1;
                mx = (int)e[0];
                my = (int)e[1];
                mb = nb;
            }
            if (mx > PANT_W - 12) mx = PANT_W - 12;
            if (my > PANT_H - 12) my = PANT_H - 12;
            if (mx < 0) mx = 0;
            if (my < 0) my = 0;
            /* arrastre en curso con boton */
            if (arr_id >= 0 && (mb & 1)) {
                ventana_t *v = &vents[arr_id];
                if (v->usado) {
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
                        if (v->w < 120) v->w = 120;
                        if (v->h < 60) v->h = 60;
                        if (v->x + v->w > PANT_W)
                            v->w = PANT_W - v->x;
                        if (v->y + v->h > AREA_H)
                            v->h = AREA_H - v->y;
                    }
                    bg_dirty = 1;
                    marca_sucia(arr_id);
                }
                frame();
            } else {
                if (arr_id >= 0 && !(mb & 1)) arr_id = -1;
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
                    /* movimiento sobre contenido: reenvia MOU siempre;
                       reframe limitado (ver ult_mou) */
                    int id = ventana_en(mx, my);
                    if (id >= 0) {
                        char m[32], nx[12], ny[12], nb[12];
                        int t = 0, q = 0;
                        u_itoa(mx, nx);
                        u_itoa(my, ny);
                        u_itoa(mb, nb);
                        m[t++] = 'M';
                        m[t++] = 'O';
                        m[t++] = 'U';
                        m[t++] = ' ';
                        while (nx[q] && t < 29) m[t++] = nx[q++];
                        m[t++] = ' ';
                        q = 0;
                        while (ny[q] && t < 29) m[t++] = ny[q++];
                        m[t++] = ' ';
                        q = 0;
                        while (nb[q] && t < 29) m[t++] = nb[q++];
                        m[t] = 0;
                        u_ipc_enviar(vents[id].pid, m, t + 1);
                        if (u_ticks() - ult_mou >= 15) {
                            ult_mou = u_ticks();
                            marca_contenido(id);
                            frame();
                        } else {
                            cursor_borra();
                            cursor_show();
                        }
                    } else {
                        /* escritorio: via rapida sin recomponer */
                        cursor_borra();
                        cursor_show();
                    }
                }
            }
            mb_antes = mb;
        } else {
            unsigned int tj[6];
            if (!u_reloj(tj) && (int)tj[4] != ultimo_min) {
                ultimo_min = (int)tj[4];
                frame();
            } else {
                u_dormir(20);
            }
        }
    }
}
