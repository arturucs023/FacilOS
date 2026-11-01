#include "ugfx.h"

void __main(void) {
}

/* editor (etapa 6, Fase M): editor de texto sobre el FS persistente.
 * Abre/crea, edita (insertar, borrar, enter, flechas), guarda (w real),
 * avisa si sales con cambios (doble Salir). Buffer 4 KB; en RAM el
 * fichero queda en <=512 B (limite del backend) y se informa lo escrito.
 * Arg = pid del servidor. */

#define NBUF 4096
#define NLIN 200
#define NNOM 40

static char buf[NBUF];
static int len = 0;
static int li = 0, co = 0, scroll = 0;
static int sucio = 0;
static char ruta[NNOM];
static int modo = 0;            /* 0 pedir archivo, 1 editar */
static char pbuf[40];
static int ppos = 0;
static char msg[64];
static int confirma = 0;

/* lineas: inicio/longitud (recalculadas al dibujar). */
static int ini[NLIN];
static int lon[NLIN];
static int nlin = 0;

static void mide(void) {
    int p = 0;
    nlin = 0;
    ini[0] = 0;
    while (p < len && nlin < NLIN) {
        int k = 0;
        while (p + k < len && buf[p + k] != '\n' && k < 200) k++;
        lon[nlin] = k;
        nlin++;
        p += k;
        if (p < len && buf[p] == '\n') {
            p++;
            if (nlin < NLIN) ini[nlin] = p;
        }
    }
    if (!nlin) {
        nlin = 1;
        lon[0] = 0;
        ini[0] = 0;
    }
    if (li >= nlin) li = nlin - 1;
    if (li < 0) li = 0;
    if (co > lon[li]) co = lon[li];
    if (co < 0) co = 0;
}

static void carga(const char *r) {
    UFILE *f = u_fopen(r, "r");
    len = 0;
    if (f) {
        int n;
        while (len < NBUF && (n = u_fread(buf + len, NBUF - len, f)) > 0)
            len += n;
        u_fclose(f);
    }
    li = 0;
    co = 0;
    scroll = 0;
    sucio = 0;
}

static void guarda(ug_win_t *v) {
    UFILE *f;
    int n;
    (void)v;
    if (!ruta[0]) {
        u_strcpy(msg, "sin nombre");
        return;
    }
    f = u_fopen(ruta, "w");
    if (!f) {
        u_strcpy(msg, "no se guarda");
        return;
    }
    n = u_fwrite(buf, len, f);
    u_fclose(f);
    if (n == len) {
        char b[40];
        int t = 0, q = 0;
        char nb[12];
        b[t++] = 'g';
        b[t++] = 'u';
        b[t++] = 'a';
        b[t++] = 'r';
        b[t++] = 'd';
        b[t++] = 'a';
        b[t++] = 'd';
        b[t++] = 'o';
        b[t++] = ' ';
        u_itoa(n, nb);
        while (nb[q] && t < 36) b[t++] = nb[q++];
        b[t++] = 'B';
        b[t] = 0;
        u_strcpy(msg, b);
        sucio = 0;
    } else {
        u_strcpy(msg, "corto: backend lleno?");
    }
}

static void inserta(int c) {
    int p, i;
    mide();
    if (len + 1 >= NBUF) return;
    p = ini[li] + co;
    for (i = len; i > p; i--) buf[i] = buf[i - 1];
    buf[p] = (char)c;
    len++;
    if (c == '\n') {
        li++;
        co = 0;
    } else {
        co++;
    }
    sucio = 1;
}

static void borra(void) {
    int p, i;
    mide();
    if (!len) return;
    p = ini[li] + co;
    if (co > 0) {
        for (i = p - 1; i < len; i++) buf[i] = buf[i + 1];
        len--;
        co--;
    } else if (li > 0) {
        co = lon[li - 1];
        for (i = p - 1; i < len; i++) buf[i] = buf[i + 1];
        len--;
        li--;
    }
    sucio = 1;
}

static void dibuja(ug_win_t *v) {
    int vis = (v->h - UG_CHROME - 64) / UG_FH, i;
    char cab[64];
    int t = 0, q = 0;
    ug_comienza(v);
    ug_limpia(v, C_NEGRO);
    if (!modo) {
        ug_texto(v, 8, 8, "archivo a abrir/crear:", C_AMARILLO);
        ug_campo(v, 8, 30, v->w - 16, pbuf, ppos, 1);
        ug_texto(v, 8, 56, "enter=abrir  (vacio=nuevo)", C_GRIS);
        if (msg[0]) ug_texto(v, 8, 76, msg, C_ROJO_CLARO);
        ug_fin(v);
        return;
    }
    while (ruta[q] && t < 30) {
        cab[t] = ruta[q];
        t++;
        q++;
    }
    if (sucio && t < 60) {
        cab[t++] = ' ';
        cab[t++] = '*';
    }
    cab[t] = 0;
    ug_texto(v, 8, 2, cab, C_AMARILLO);
    mide();
    if (vis < 1) vis = 1;
    if (li < scroll) scroll = li;
    if (li >= scroll + vis) scroll = li - vis + 1;
    for (i = 0; i < vis && scroll + i < nlin; i++) {
        char ln[62];
        int k = 0, p = ini[scroll + i];
        while (k < 58 && k < lon[scroll + i]) {
            ln[k] = buf[p + k];
            k++;
        }
        ln[k] = 0;
        ug_texto(v, 8, 22 + i * UG_FH, ln, C_GRIS);
    }
    if (v->foco) {
        int cx = 8 + co * UG_FW, cy = 22 + (li - scroll) * UG_FH;
        if (li >= scroll && li < scroll + vis)
            ug_relleno(v, cx, cy, 2, UG_FH, C_BLANCO);
    }
    /* botones */
    ug_boton(v, 8, v->h - UG_CHROME - 26, 90, 22, "Guardar", 0);
    ug_boton(v, 104, v->h - UG_CHROME - 26, 90, 22, "Abrir", 0);
    ug_boton(v, 200, v->h - UG_CHROME - 26, 90, 22, "Salir", 0);
    if (msg[0]) ug_texto(v, 300, v->h - UG_CHROME - 22, msg, C_ROJO_CLARO);
    ug_fin(v);
}

static int en_boton(int lx, int ly, int h, int n) {
    int y0 = h - UG_CHROME - 26;
    if (ly < y0 || ly >= y0 + 22) return 0;
    if (n == 0) return lx >= 8 && lx < 98;
    if (n == 1) return lx >= 104 && lx < 194;
    return lx >= 200 && lx < 290;
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0, bot_antes = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    ruta[0] = 0;
    pbuf[0] = 0;
    msg[0] = 0;
    if (ug_abre(&v, srv, 500, 340, "editor")) return 98;
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        if (t == UG_EV_SALIR) {
            if (modo && sucio && !confirma) {
                confirma = 1;
                u_strcpy(msg, "con cambios: otra vez Salir");
                continue;
            }
            return 0;
        }
        confirma = 0;
        if (t == UG_EV_TECLA) {
            int k = ev.tecla;
            if (!modo) {
                if (k == 13) {
                    int i = 0;
                    while (i < 39 && pbuf[i]) {
                        ruta[i] = pbuf[i];
                        i++;
                    }
                    ruta[i] = 0;
                    carga(ruta);
                    modo = 1;
                    msg[0] = 0;
                } else {
                    ug_campo_tecla(pbuf, 40, &ppos, k);
                }
            } else if (k == 0x101) {
                mide();
                if (li > 0) {
                    li--;
                    if (co > lon[li]) co = lon[li];
                }
            } else if (k == 0x102) {
                mide();
                if (li + 1 < nlin) {
                    li++;
                    if (co > lon[li]) co = lon[li];
                }
            } else if (k == 0x103) {
                if (co > 0) co--;
            } else if (k == 0x104) {
                mide();
                if (co < lon[li]) co++;
            } else if (k == 8) {
                borra();
            } else if (k == 13 || (k >= 32 && k < 127)) {
                inserta(k == 13 ? '\n' : k);
            }
        } else if (t == UG_EV_RATON) {
            int lx = ev.rx - v.x, ly = ev.ry - (v.y + UG_CHROME);
            if (modo && ug_clic(bot_antes, ev.rbot)) {
                if (en_boton(lx, ly, v.h, 0)) guarda(&v);
                else if (en_boton(lx, ly, v.h, 1)) {
                    modo = 0;
                    pbuf[0] = 0;
                    ppos = 0;
                } else if (en_boton(lx, ly, v.h, 2)) {
                    if (sucio && !confirma) {
                        confirma = 1;
                        u_strcpy(msg, "con cambios: otra vez");
                    } else {
                        ug_cierra(&v);
                        return 0;
                    }
                }
            }
            bot_antes = ev.rbot;
        } else if (t == UG_EV_DRAW) {
            dibuja(&v);
        }
    }
}
