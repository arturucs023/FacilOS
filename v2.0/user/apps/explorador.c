#include "ugfx.h"

void __main(void) {
}

/* explorador (etapa 6, Fase K): navegador del FS persistente + CD.
 * Lista con syscall 41, crea (42/abrir-w), renombra (43), borra (16),
 * ejecuta F32X (del FS o del CD) y ve texto. Tipos: 1 carp, 2 arch, 3 CD.
 * Arg = pid del servidor. */

#define NENT 96
#define NNOM 40

static int tipos[NENT];
static char nombres[NENT][NNOM];
static unsigned int tams[NENT];
static int idxs[NENT];
static int nn = 0;

static int dir = 0;
static int pila[8];
static char pnom[8][NNOM];
static int prof = 0;

static int sel = 0, scroll = 0;
/* visor: 0 no, 1 viendo archivo */
static int viendo = 0;
static char vnom[NNOM];
static char vbuf[2048];
static int vlen = 0, vscroll = 0;
/* entrada de nombre: 0 no, 1 archivo, 2 carpeta, 3 renombrar */
static int pidiendo = 0;
static char pbuf[NNOM];
static int ppos = 0;
static char msg[64];

static void recarga(void) {
    int n = 0;
    udir_t d;
    nn = 0;
    while (nn < NENT && u_dir_hijo(dir, n, &d) >= 0) {
        int j = 0;
        tipos[nn] = d.tipo;
        while (j < NNOM - 1 && d.nombre[j]) {
            nombres[nn][j] = d.nombre[j];
            j++;
        }
        nombres[nn][j] = 0;
        tams[nn] = d.tam;
        idxs[nn] = d.idx;
        nn++;
        n++;
    }
    if (sel >= nn) sel = nn ? nn - 1 : 0;
    if (sel < 0) sel = 0;
    if (scroll > sel) scroll = sel;
}

static void ruta_txt(char *d) {
    int t = 0, i;
    d[t++] = '/';
    for (i = 0; i < prof; i++) {
        int j = 0;
        if (t > 1) d[t++] = '/';
        while (pnom[i][j] && t < 60) d[t++] = pnom[i][j++];
    }
    d[t] = 0;
}

static void abre_sel(ug_win_t *v) {
    if (sel < 0 || sel >= nn) return;
    if (tipos[sel] == 1) {
        if (prof < 8) {
            int j = 0;
            pila[prof] = dir;
            while (j < NNOM - 1 && nombres[sel][j]) {
                pnom[prof][j] = nombres[sel][j];
                j++;
            }
            pnom[prof][j] = 0;
            prof++;
            dir = idxs[sel];
            sel = 0;
            scroll = 0;
            viendo = 0;
            recarga();
        }
    } else if (tipos[sel] == 2) {
        UFILE *f = u_fopen(nombres[sel], "r");
        if (!f) {
            u_strcpy(msg, "no se abre");
            return;
        }
        vlen = u_fread(vbuf, 2047, f);
        if (vlen < 0) vlen = 0;
        vbuf[vlen] = 0;
        u_fclose(f);
        {
            int j = 0;
            while (j < NNOM - 1 && nombres[sel][j]) {
                vnom[j] = nombres[sel][j];
                j++;
            }
            vnom[j] = 0;
        }
        viendo = 1;
        vscroll = 0;
        msg[0] = 0;
    } else {
        ug_modal(v, 1);
        {
            int rc = u_ejecutar(nombres[sel], "");
            char b[32];
            int t = 0, q = 0;
            char nb[12];
            b[t++] = 'r';
            b[t++] = 'c';
            b[t++] = '=';
            u_itoa(rc, nb);
            while (nb[q] && t < 30) b[t++] = nb[q++];
            b[t] = 0;
            u_strcpy(msg, b);
        }
        ug_modal(v, 0);
    }
    (void)v;
}

static void confirma_nombre(ug_win_t *v) {
    (void)v;
    if (!pbuf[0]) {
        pidiendo = 0;
        return;
    }
    if (pidiendo == 1) {
        UFILE *f = u_fopen(pbuf, "w");
        if (!f) u_strcpy(msg, "no se crea");
        else {
            u_fclose(f);
            u_strcpy(msg, "creado");
        }
    } else if (pidiendo == 2) {
        if (u_dir_crear(pbuf)) u_strcpy(msg, "no se crea");
        else u_strcpy(msg, "carpeta ok");
    } else if (pidiendo == 3) {
        int r;
        if (sel < 0 || sel >= nn || tipos[sel] == 3)
            u_strcpy(msg, "no renombrable");
        else {
            r = u_renombrar(nombres[sel], pbuf);
            if (r == -2) u_strcpy(msg, "ya existe");
            else if (r) u_strcpy(msg, "no se renombra");
            else u_strcpy(msg, "ok");
        }
    }
    pidiendo = 0;
    recarga();
}

static void dibuja(ug_win_t *v) {
    char rt[64];
    int vis = (v->h - UG_CHROME - 60) / UG_FH, ini, i;
    ug_comienza(v);
    ug_limpia(v, C_NEGRO);
    ruta_txt(rt);
    ug_texto(v, 4, 2, rt, C_AMARILLO);
    if (viendo) {
        int li, fila = 0, p = 0;
        ug_texto(v, 4, 20, vnom, C_VERDE_CLARO);
        ug_texto(v, 4, 36, "[retroceso=volver]", C_GRIS);
        while (p < vlen && fila < vscroll) {
            while (p < vlen && vbuf[p] != '\n') p++;
            if (p < vlen) p++;
            fila++;
        }
        for (li = 0; li < vis && p < vlen; li++) {
            char ln[60];
            int k = 0;
            while (p < vlen && vbuf[p] != '\n' && k < 56) ln[k++] = vbuf[p++];
            ln[k] = 0;
            if (p < vlen && vbuf[p] == '\n') p++;
            ug_texto(v, 4, 56 + li * UG_FH, ln, C_GRIS);
        }
    } else {
        const char *items[NENT];
        char lineas[NENT][NNOM + 12];
        for (i = 0; i < nn; i++) {
            int t = 0, j = 0;
            lineas[i][t++] = tipos[i] == 1 ? 'D' : tipos[i] == 2 ? 'F' : 'C';
            lineas[i][t++] = ' ';
            while (j < NNOM - 1 && nombres[i][j]) lineas[i][t++] = nombres[i][j++];
            if (tipos[i] == 2) {
                char nb[12];
                int q = 0;
                lineas[i][t++] = ' ';
                lineas[i][t++] = '(';
                u_itoa((int)tams[i], nb);
                while (nb[q] && t < NNOM + 10) lineas[i][t++] = nb[q++];
                lineas[i][t++] = ')';
            }
            lineas[i][t] = 0;
            items[i] = lineas[i];
        }
        if (vis < 1) vis = 1;
        ini = scroll;
        ug_lista(v, 4, 22, v->w - 8, vis * UG_FH, items, nn, sel, ini);
        /* la lista ya aplica scroll; sincroniza base visible */
        ug_texto(v, 4, 22 + vis * UG_FH + 4,
                 "enter=abrir retroceso=atras n/d/r/x", C_GRIS);
        if (msg[0]) ug_texto(v, 4, 22 + vis * UG_FH + 20, msg, C_ROJO_CLARO);
        if (pidiendo) {
            ug_campo(v, 4, 22 + vis * UG_FH + 36, v->w - 8, pbuf, ppos, 1);
        }
    }
    (void)ini;
    ug_fin(v);
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    msg[0] = 0;
    if (ug_abre(&v, srv, 500, 340, "explorador")) return 98;
    recarga();
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        if (t == UG_EV_SALIR) return 0;
        if (t == UG_EV_TECLA) {
            int k = ev.tecla, vis = (v.h - UG_CHROME - 60) / UG_FH;
            if (vis < 1) vis = 1;
            if (pidiendo) {
                if (k == 13) confirma_nombre(&v);
                else if (k == 8 && ppos == 0) pidiendo = 0;
                else ug_campo_tecla(pbuf, NNOM, &ppos, k);
            } else if (viendo) {
                if (k == 8) viendo = 0;
                else if (k == 0x101 && vscroll > 0) vscroll--;
                else if (k == 0x102) vscroll++;
            } else if (k == 0x101) {
                if (ug_lista_tecla(&sel, &scroll, nn, vis, k)) msg[0] = 0;
            } else if (k == 0x102) {
                if (ug_lista_tecla(&sel, &scroll, nn, vis, k)) msg[0] = 0;
            } else if (k == 13) {
                abre_sel(&v);
            } else if (k == 8) {
                if (prof > 0) {
                    prof--;
                    dir = prof ? pila[prof] : 0;
                    sel = 0;
                    scroll = 0;
                    viendo = 0;
                    recarga();
                }
            } else if (k == 'n' || k == 'N') {
                pidiendo = 1;
                pbuf[0] = 0;
                ppos = 0;
            } else if (k == 'd' || k == 'D') {
                pidiendo = 2;
                pbuf[0] = 0;
                ppos = 0;
            } else if (k == 'r' || k == 'R') {
                pidiendo = 3;
                pbuf[0] = 0;
                ppos = 0;
            } else if (k == 'x' || k == 'X') {
                if (sel >= 0 && sel < nn && tipos[sel] != 3) {
                    if (u_remove(nombres[sel])) u_strcpy(msg, "no se borra");
                    else {
                        u_strcpy(msg, "borrado");
                        recarga();
                    }
                } else {
                    u_strcpy(msg, "CD: solo lectura");
                }
            }
        } else if (t == UG_EV_RATON) {
            int lx = ev.rx - v.x, ly = ev.ry - (v.y + UG_CHROME);
            int vis = (v.h - UG_CHROME - 60) / UG_FH, id;
            static int antes = 0;
            if (vis < 1) vis = 1;
            if (!viendo && !pidiendo) {
                id = ug_lista_clic(ly, 22, scroll, nn);
                if (id >= 0 && ug_clic(antes, ev.rbot)) {
                    sel = id;
                    msg[0] = 0;
                }
            }
            antes = ev.rbot;
            (void)lx;
        } else if (t == UG_EV_DRAW) {
            dibuja(&v);
        }
    }
}
