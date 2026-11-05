#include "ugfx.h"
#include "utext.h"

void __main(void) {
}

/* Explorador renovado (v2.2): gestor grafico con barra de herramientas,
 * direccion, arbol de carpetas, vista de detalles (nombre, tamano,
 * tipo), estado, iconos propios y portapapeles. Todo Ring3 con las
 * llamadas ya verificadas: lista (41), crear (fopen-w/42), renombrar
 * (43), copiar (leer+escribir), mover (renombrar o copiar+borrar),
 * borrar (16; -2 si la carpeta no esta vacia) y ejecutar CD. Sin
 * simulacros: cada fallo deja mensaje. Arg = pid del servidor. */

#define NENT 96
#define NNOM 40
#define NARB 128
#define NHIST 16
#define NEXP 32

static int tipos[NENT];
static char nombres[NENT][NNOM];
static unsigned int tams[NENT];
static int idxs[NENT];
static int nn = 0;

static int dir = 0;
static int pila[8];
static char pnom[8][NNOM];
static int prof = 0;

static int sel = 0, scroll = 0, orden = 0;   /* orden: 0 nombre, 1 tipo */

/* Arbol de carpetas. */
typedef struct {
    int didx, prof, padre;
    char nombre[NNOM];
    int expandido, tiene;
} nodo_t;
static nodo_t arb[NARB];
static int narb = 0, asel = 0, ascroll = 0;
static int expandidos[NEXP];
static int nexp = 0;

/* Atras/adelante: indice + ruta absoluta. */
static int hatras[NHIST];
static char hatras_r[NHIST][128];
static int natras = 0;
static int hadel[NHIST];
static char hadel_r[NHIST][128];
static int nadel = 0;

/* Portapapeles: 0 vacio, 1 copiar, 2 cortar (solo archivos). */
static int porta = 0;
static char porta_ruta[128], porta_nom[NNOM];

/* Pedir nombre: 0 no, 1 archivo, 2 carpeta, 3 renombrar. */
static int pidiendo = 0;
static char pbuf[NNOM];
static int ppos = 0;

/* Propiedades: 0 no, 1 si. */
static int viendo_prop = 0;

/* pid del servidor (argumento): lo heredan los programas lanzados. */
static int mi_srv = 0;

static char msg[80];
static unsigned int ultimo_clic = 0;
static int ultimo_id = -1;

/* ---- rutas ---- */

/* Absoluta del directorio actual ("/" si raiz). */
static void ruta_dir_abs(char *d) {
    int t = 0, i;
    d[t++] = '/';
    for (i = 0; i < prof; i++) {
        int j = 0;
        if (t > 1) d[t++] = '/';
        while (pnom[i][j] && t < 120) d[t++] = pnom[i][j++];
    }
    d[t] = 0;
}

/* Absoluta de un elemento del directorio actual. */
static void ruta_abs(char *d, const char *nom) {
    int t = 0, i;
    d[t++] = '/';
    for (i = 0; i < prof; i++) {
        int j = 0;
        if (t > 1) d[t++] = '/';
        while (pnom[i][j] && t < 100) d[t++] = pnom[i][j++];
    }
    if (nom && nom[0]) {
        int j = 0;
        if (t > 1) d[t++] = '/';
        while (nom[j] && t < 120) d[t++] = nom[j++];
    }
    d[t] = 0;
}

/* Parte una ruta absoluta en pila/pnom (para volver del historial). */
static void parte_ruta(const char *r) {
    int i = 0, n = 0;
    prof = 0;
    if (!r || r[0] != '/') return;
    i = 1;
    while (r[i] && prof < 8) {
        int j = 0;
        while (r[i] == '/') i++;
        if (!r[i]) break;
        while (r[i] && r[i] != '/' && j < NNOM - 1) pnom[prof][j++] = r[i++];
        pnom[prof][j] = 0;
        if (j) prof++;
        if (++n > 40) break;
    }
}

/* Copia sin partir multibyte (maxb incluye el 0). */
static void corta_utf8(char *d, const char *s, int maxb) {
    int t = 0;
    if (maxb < 1) return;
    while (s[t] && t < maxb - 1) {
        int av = 0;
        ut_dec(s + t, 4, &av);
        if (av <= 0) av = 1;
        if (t + av >= maxb) break;
        {
            int k;
            for (k = 0; k < av; k++) d[t + k] = s[t + k];
        }
        t += av;
    }
    d[t] = 0;
}

/* ---- iconos (tabla propia del tema, como el servidor) ---- */

static void icono(int idx, int x, int y, ug_win_t *v) {
    int r, c;
    if (idx < 0 || idx > 23) return;
    for (r = 0; r < 16; r++) {
        const char *fila = T_ICONOS[idx][r];
        for (c = 0; c < 16; c++) {
            char ch = fila[c];
            if (ch == '.' || ch == 0) continue;
            {
                int col = t_icon_rgb(ch);
                if (col) ug_pixel(v, x + c, y + r, col);
            }
        }
    }
}

/* Icono segun tipo y extension. */
static int icono_de(int tipo, const char *nom) {
    int i, p = -1;
    if (tipo == 1) return 6;
    if (tipo == 3) return 8;
    for (i = 0; nom[i]; i++) if (nom[i] == '.') p = i;
    if (p >= 0) {
        const char *e = nom + p + 1;
        char b[8];
        int k = 0;
        while (e[k] && k < 7) {
            char c = e[k];
            if (c >= 'A' && c <= 'Z') c += 32;
            b[k] = c;
            k++;
        }
        b[k] = 0;
        if (!u_strcmp(b, "txt") || !u_strcmp(b, "md") ||
            !u_strcmp(b, "cfg") || !u_strcmp(b, "log") ||
            !u_strcmp(b, "ini") || !u_strcmp(b, "c") ||
            !u_strcmp(b, "h") || !u_strcmp(b, "f32"))
            return 7;
        if (!u_strcmp(b, "f32x") || !u_strcmp(b, "bin") ||
            !u_strcmp(b, "exe"))
            return 8;
        return 3;
    }
    return 3;
}

static const char *texto_tipo(int tipo, const char *nom) {
    if (tipo == 1) return "Carpeta";
    if (tipo == 3) return "Programa";
    {
        int ic = icono_de(tipo, nom);
        if (ic == 7) return "Texto";
        if (ic == 8) return "Programa";
    }
    return "Archivo";
}

/* Tamano corto: "12B" o "127KB". */
static void texto_tam(char *d, unsigned int n) {
    int t = 0, q = 0;
    char nb[12];
    if (n < 1024) {
        u_itoa((int)n, nb);
        while (nb[q] && t < 10) d[t++] = nb[q++];
        d[t++] = 'B';
    } else {
        u_itoa((int)((n + 1023) / 1024), nb);
        while (nb[q] && t < 8) d[t++] = nb[q++];
        d[t++] = 'K';
        d[t++] = 'B';
    }
    d[t] = 0;
}

/* ---- lista y orden ---- */

static void ordena(void) {
    int i, j;
    for (i = 0; i < nn; i++) {
        for (j = i + 1; j < nn; j++) {
            int sw = 0;
            if (orden == 1) {
                if (tipos[j] < tipos[i]) sw = 1;
                else if (tipos[j] == tipos[i] &&
                         u_strcmp(nombres[j], nombres[i]) < 0)
                    sw = 1;
            } else {
                if (u_strcmp(nombres[j], nombres[i]) < 0) sw = 1;
            }
            if (sw) {
                char tn[NNOM];
                int tt, ti;
                unsigned int tm;
                u_strcpy(tn, nombres[i]);
                u_strcpy(nombres[i], nombres[j]);
                u_strcpy(nombres[j], tn);
                tt = tipos[i];
                tipos[i] = tipos[j];
                tipos[j] = tt;
                tm = tams[i];
                tams[i] = tams[j];
                tams[j] = tm;
                ti = idxs[i];
                idxs[i] = idxs[j];
                idxs[j] = ti;
            }
        }
    }
}

static void recarga(void);

static int esta_expandido(int didx) {
    int i;
    for (i = 0; i < nexp; i++)
        if (expandidos[i] == didx) return 1;
    return 0;
}

static void mete_expandido(int didx) {
    int i;
    for (i = 0; i < nexp; i++)
        if (expandidos[i] == didx) return;
    if (nexp < NEXP) expandidos[nexp++] = didx;
}

static void saca_expandido(int didx) {
    int i, j;
    for (i = 0; i < nexp; i++) {
        if (expandidos[i] == didx) {
            for (j = i; j + 1 < nexp; j++) expandidos[j] = expandidos[j + 1];
            nexp--;
            return;
        }
    }
}

/* hijas carpetas de d ordenadas por nombre */
static int hijas_carp(int d, int out_idx[NENT], char out_nom[NENT][NNOM]) {
    int n = 0, k = 0, i, j, r = 0;
    udir_t e;
    while (r < NENT && u_dir_hijo(d, k, &e) >= 0) {
        if (e.tipo == 1) {
            corta_utf8(out_nom[r], e.nombre, NNOM);
            out_idx[r] = e.idx;
            r++;
        }
        k++;
        if (k > 200) break;
        n++;
    }
    (void)n;
    for (i = 0; i < r; i++) {
        for (j = i + 1; j < r; j++) {
            if (u_strcmp(out_nom[j], out_nom[i]) < 0) {
                char tn[NNOM];
                int ti;
                u_strcpy(tn, out_nom[i]);
                u_strcpy(out_nom[i], out_nom[j]);
                u_strcpy(out_nom[j], tn);
                ti = out_idx[i];
                out_idx[i] = out_idx[j];
                out_idx[j] = ti;
            }
        }
    }
    return r;
}

static void arbol_reconstruye(void) {
    int i, k;
    narb = 0;
    arb[narb].didx = 0;
    arb[narb].prof = 0;
    arb[narb].padre = -1;
    arb[narb].nombre[0] = '/';
    arb[narb].nombre[1] = 0;
    arb[narb].expandido = 1;
    arb[narb].tiene = 1;
    narb++;
    for (i = 0; i < prof; i++) mete_expandido(pila[i]);
    mete_expandido(dir);
    for (i = 0; i < narb; i++) {
        if (!arb[i].expandido || arb[i].prof >= 8) continue;
        {
            int oidx[NENT], r, q;
            char onom[NENT][NNOM];
            r = hijas_carp(arb[i].didx, oidx, onom);
            for (q = 0; q < r && narb < NARB; q++) {
                arb[narb].didx = oidx[q];
                arb[narb].prof = arb[i].prof + 1;
                arb[narb].padre = i;
                u_strcpy(arb[narb].nombre, onom[q]);
                arb[narb].expandido = esta_expandido(oidx[q]);
                arb[narb].tiene = 0;
                {
                    udir_t e2;
                    int k2 = 0;
                    while (k2 < 200 && u_dir_hijo(oidx[q], k2, &e2) >= 0) {
                        if (e2.tipo == 1) {
                            arb[narb].tiene = 1;
                            break;
                        }
                        k2++;
                    }
                }
                narb++;
            }
        }
    }
    asel = 0;
    for (k = 0; k < narb; k++) {
        if (arb[k].didx == dir) asel = k;
    }
    if (asel < ascroll) ascroll = asel;
}

static void recarga(void) {
    int n = 0;
    udir_t d;
    if (dir != 0) {
        /* El directorio pudo borrarse desde otra aplicacion: si ya no
           cuelga del padre, vuelta a la raiz (un directorio vacio es
           valido: dir_hijo falla tambien sin hijos). */
        int pdir = prof ? pila[prof - 1] : 0, k = 0, ok = 0;
        while (k < 200 && u_dir_hijo(pdir, k, &d) >= 0) {
            if (d.idx == dir && d.tipo == 1) {
                ok = 1;
                break;
            }
            k++;
        }
        if (!ok) {
            dir = 0;
            prof = 0;
            u_strcpy(msg, "ya no existe: a raiz");
        }
    }
    nn = 0;
    while (nn < NENT && u_dir_hijo(dir, n, &d) >= 0) {
        /* Los programas del CD no se listan (solo archivos y
           carpetas del FS; se lanzan desde la terminal). */
        if (d.tipo == 3) {
            n++;
            if (n > 200) break;
            continue;
        }
        tipos[nn] = d.tipo;
        corta_utf8(nombres[nn], d.nombre, NNOM);
        tams[nn] = d.tam;
        idxs[nn] = d.idx;
        nn++;
        n++;
        if (n > 200) break;
    }
    ordena();
    if (sel >= nn) sel = nn ? nn - 1 : 0;
    if (sel < 0) sel = 0;
    if (scroll > sel) scroll = sel;
    arbol_reconstruye();
}

/* ---- navegacion e historial ---- */

static void guarda_hist(int h[NHIST], char hr[NHIST][128], int *n,
                        int didx, const char *ruta) {
    int i;
    if (*n >= NHIST) {
        for (i = 0; i + 1 < NHIST; i++) {
            h[i] = h[i + 1];
            u_strcpy(hr[i], hr[i + 1]);
        }
        (*n)--;
    }
    h[*n] = didx;
    u_strcpy(hr[*n], ruta);
    (*n)++;
}

static int saca_hist(int h[NHIST], char hr[NHIST][128], int *n,
                     int *didx, char *ruta) {
    if (*n <= 0) return -1;
    (*n)--;
    *didx = h[*n];
    u_strcpy(ruta, hr[*n]);
    return 0;
}

/* Ir a un directorio (empuja: guarda el actual en atras). */
static void ir_a(int didx, const char *ruta, int empuja) {
    char ahora[128];
    if (empuja) {
        ruta_dir_abs(ahora);
        guarda_hist(hatras, hatras_r, &natras, dir, ahora);
        nadel = 0;
    }
    dir = didx;
    parte_ruta(ruta);
    sel = 0;
    scroll = 0;
    viendo_prop = 0;
    recarga();
}

/* Ir siguiendo un nodo del arbol (reconstruye la pila). */
static void ir_a_nodo(int ni) {
    int camino[10], cn = 0, i = ni;
    char ruta[128];
    int t = 0, k;
    while (i >= 0 && cn < 10) {
        camino[cn++] = i;
        i = arb[i].padre;
    }
    ruta[t++] = '/';
    for (k = cn - 2; k >= 0; k--) {
        int j = 0;
        if (t > 1) ruta[t++] = '/';
        while (arb[camino[k]].nombre[j] && t < 120)
            ruta[t++] = arb[camino[k]].nombre[j++];
    }
    ruta[t] = 0;
    ir_a(arb[ni].didx, ruta, 1);
}

static void ir_atras(void) {
    int d;
    char r[128], ahora[128];
    if (saca_hist(hatras, hatras_r, &natras, &d, r)) {
        u_strcpy(msg, "no hay anterior");
        return;
    }
    ruta_dir_abs(ahora);
    guarda_hist(hadel, hadel_r, &nadel, dir, ahora);
    dir = d;
    parte_ruta(r);
    sel = 0;
    scroll = 0;
    viendo_prop = 0;
    recarga();
}

static void ir_adelante(void) {
    int d;
    char r[128], ahora[128];
    if (saca_hist(hadel, hadel_r, &nadel, &d, r)) {
        u_strcpy(msg, "no hay siguiente");
        return;
    }
    ruta_dir_abs(ahora);
    guarda_hist(hatras, hatras_r, &natras, dir, ahora);
    dir = d;
    parte_ruta(r);
    sel = 0;
    scroll = 0;
    viendo_prop = 0;
    recarga();
}

static void ir_arriba(void) {
    char ahora[128], ruta[128];
    if (prof <= 0) {
        u_strcpy(msg, "ya es raiz");
        return;
    }
    ruta_dir_abs(ahora);
    guarda_hist(hatras, hatras_r, &natras, dir, ahora);
    nadel = 0;
    prof--;
    dir = prof ? pila[prof] : 0;
    ruta_dir_abs(ruta);
    (void)ruta;
    sel = 0;
    scroll = 0;
    viendo_prop = 0;
    recarga();
}

/* ---- operaciones de archivos (patrones verificados de terminal) ---- */

static int existe(const char *ruta) {
    UFILE *f = u_fopen(ruta, "r");
    if (!f) return 0;
    u_fclose(f);
    return 1;
}

/* 0 ok, 1 abrir, 2 crear, 3 escribir, 4 ya existe. */
static int copia_archivo(const char *src, const char *dst) {
    UFILE *fo = u_fopen(src, "r"), *fd;
    char buf[256];
    int n;
    if (!fo) return 1;
    if (existe(dst)) {
        u_fclose(fo);
        return 4;
    }
    fd = u_fopen(dst, "w");
    if (!fd) {
        u_fclose(fo);
        return 2;
    }
    while ((n = u_fread(buf, 256, fo)) > 0) {
        if (u_fwrite(buf, n, fd) != n) {
            u_fclose(fo);
            u_fclose(fd);
            u_remove(dst);
            return 3;
        }
    }
    u_fclose(fo);
    u_fclose(fd);
    return 0;
}

static void confirma_nombre(ug_win_t *v) {
    char dst[128];
    (void)v;
    if (!pbuf[0]) {
        pidiendo = 0;
        return;
    }
    if (pidiendo == 1) {
        UFILE *f;
        ruta_abs(dst, pbuf);
        f = u_fopen(dst, "w");
        if (!f) u_strcpy(msg, "no se crea");
        else {
            u_fclose(f);
            u_strcpy(msg, "creado");
        }
    } else if (pidiendo == 2) {
        ruta_abs(dst, pbuf);
        if (u_dir_crear(dst)) u_strcpy(msg, "no se crea");
        else u_strcpy(msg, "carpeta ok");
    } else if (pidiendo == 3) {
        if (sel < 0 || sel >= nn || tipos[sel] == 3) {
            u_strcpy(msg, "no renombrable");
        } else {
            char src[128];
            int r;
            ruta_abs(src, nombres[sel]);
            r = u_renombrar(src, pbuf);
            if (r == -2) u_strcpy(msg, "ya existe");
            else if (r) u_strcpy(msg, "no se renombra");
            else u_strcpy(msg, "ok");
        }
    }
    pidiendo = 0;
    recarga();
}

static void pega(void) {
    char dst[128];
    if (!porta) {
        u_strcpy(msg, "nada que pegar");
        return;
    }
    ruta_abs(dst, porta_nom);
    if (porta == 1) {
        int r = copia_archivo(porta_ruta, dst);
        if (r == 4) u_strcpy(msg, "ya existe");
        else if (r) u_strcpy(msg, "no se copia");
        else {
            u_strcpy(msg, "copiado");
            recarga();
        }
    } else {
        /* cortar: mismo sitio no hace nada */
        char srcdir[128];
        int t = 0, i, q = 0;
        while (porta_ruta[t]) t++;
        while (t > 0 && porta_ruta[t - 1] != '/') t--;
        for (i = 0; i < t && q < 120; i++) srcdir[q++] = porta_ruta[i];
        if (q > 1 && srcdir[q - 1] == '/') q--;
        srcdir[q] = 0;
        {
            char ah[128];
            ruta_dir_abs(ah);
            if (!u_strcmp(ah, srcdir[0] ? srcdir : "/")) {
                u_strcpy(msg, "ya esta aqui");
                return;
            }
        }
        {
            int r = copia_archivo(porta_ruta, dst);
            if (r == 4) u_strcpy(msg, "ya existe");
            else if (r) u_strcpy(msg, "no se mueve");
            else if (u_remove(porta_ruta)) {
                u_strcpy(msg, "copiado pero no borrado");
                recarga();
            } else {
                u_strcpy(msg, "movido");
                porta = 0;
                recarga();
            }
        }
    }
}

static void borra_sel(void) {
    if (sel < 0 || sel >= nn) return;
    if (tipos[sel] == 3) {
        u_strcpy(msg, "CD: solo lectura");
        return;
    }
    {
        char ruta[128];
        int r;
        ruta_abs(ruta, nombres[sel]);
        r = u_remove(ruta);
        if (r == -2) u_strcpy(msg, "no vacia");
        else if (r) u_strcpy(msg, "no se borra");
        else {
            u_strcpy(msg, tipos[sel] == 1 ? "carpeta borrada" : "borrado");
            recarga();
        }
    }
}

static void abre_sel(ug_win_t *v, int empuja_hist) {
    char ruta[128];
    if (sel < 0 || sel >= nn) return;
    if (tipos[sel] == 1) {
        ruta_abs(ruta, nombres[sel]);
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
            viendo_prop = 0;
            if (empuja_hist) {
                /* guarda el anterior */
                char ant[128];
                int t = 0, i;
                ant[t++] = '/';
                for (i = 0; i < prof - 1; i++) {
                    int k = 0;
                    if (t > 1) ant[t++] = '/';
                    while (pnom[i][k] && t < 120) ant[t++] = pnom[i][k++];
                }
                ant[t] = 0;
                guarda_hist(hatras, hatras_r, &natras,
                            prof - 1 ? pila[prof - 1] : 0, ant);
                nadel = 0;
            }
            recarga();
        }
    } else if (tipos[sel] == 2) {
        /* Edita en el editor (ventana propia del escritorio). */
        char arg[140];
        int t = 0, q = 0;
        char nb[12];
        u_itoa(mi_srv, nb);
        while (nb[q] && t < 130) arg[t++] = nb[q++];
        arg[t++] = ' ';
        ruta_abs(ruta, nombres[sel]);
        q = 0;
        while (ruta[q] && t < 138) arg[t++] = ruta[q++];
        arg[t] = 0;
        {
            int rc = u_ejecutar("editor", arg);
            char b[32];
            int t2 = 0, q2 = 0;
            b[t2++] = 'r';
            b[t2++] = 'c';
            b[t2++] = '=';
            u_itoa(rc, nb);
            while (nb[q2] && t2 < 30) b[t2++] = nb[q2++];
            b[t2] = 0;
            u_strcpy(msg, b);
        }
        recarga();
    }
}

/* ---- dibujo ---- */

/* Geometria (contenido): barra 38, direccion 18, cabecera 18,
   lista, estado 18, ayuda/msg 18. */
#define TB_H 38
#define TB_CEL 52
#define DIR_ALT 18
#define CAB_ALT 18
#define PIE_ALT 36

static int lista_y0(void) {
    return TB_H + 2 + DIR_ALT + CAB_ALT + 2;
}

static int lista_y1(ug_win_t *v) {
    return (v->h - UG_CHROME) - PIE_ALT;
}

static int lista_vis(ug_win_t *v) {
    int vis = (lista_y1(v) - lista_y0()) / UG_FH;
    return vis < 1 ? 1 : vis;
}

/* Iconos de la barra (ver tema.h 12-23) + etiqueta corta. */
static const int icono_boton[12] = {
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
};
static const char *etiquetas[12] = {
    "Atras", "Adel.", "Subir", "Act.", "Nuevo", "Carp.",
    "Renom.", "Copiar", "Cortar", "Pegar", "Borrar", "Propi."
};

/* Ultima posicion del raton (para resaltar la etiqueta). */
static int raton_lx = -1, raton_ly = -1;

static void dibuja_barra(ug_win_t *v) {
    int i;
    ug_relleno(v, 0, 0, v->w, TB_H + 2, T_PANEL);
    ug_rect(v, 0, TB_H + 2, v->w, 1, T_PANEL_BORDE);
    for (i = 0; i < 12; i++) {
        int x = 4 + i * TB_CEL, tw = 0;
        int realzado = (raton_lx >= x && raton_lx < x + TB_CEL &&
                        raton_ly >= 2 && raton_ly < 2 + TB_H);
        while (etiquetas[i][tw]) tw++;
        tw *= UG_FW;
        if (x + TB_CEL > v->w - 4) break;
        if (realzado)
            ug_relleno(v, x, 2, TB_CEL, TB_H, T_SELECCION);
        icono(icono_boton[i], x + (TB_CEL - 16) / 2, 4, v);
        ug_texto(v, x + (TB_CEL - tw) / 2, 22, etiquetas[i],
                 i == 0 && natras <= 0 ? C_GRIS_OSCURO : T_PANEL_TEXTO);
    }
}

static void dibuja(ug_win_t *v) {
    char rt[128];
    int vis = lista_vis(v), y0 = lista_y0(), li;
    ug_comienza(v);
    ug_limpia(v, T_VENT_FONDO);
    dibuja_barra(v);
    ruta_dir_abs(rt);
    {
        char adr[128];
        int t = 0, q = 0;
        while (rt[q] && t < 120) adr[t++] = rt[q++];
        adr[t] = 0;
        ug_rect(v, 4, TB_H + 4, v->w - 8, DIR_ALT, T_PANEL_BORDE);
        ug_texto(v, 8, TB_H + 6, adr, T_PANEL_TEXTO);
    }
    /* cabeceras */
    ug_texto(v, 8, TB_H + 2 + DIR_ALT + 4, "Carpetas", C_AMARILLO);
    {
        char c1[32], c3[16];
        int t = 0, q = 0;
        const char *s = orden ? "Nombre v" : "Nombre ^";
        while (s[q] && t < 30) c1[t++] = s[q++];
        c1[t] = 0;
        q = 0;
        t = 0;
        s = orden ? "Tipo ^" : "Tipo";
        while (s[q] && t < 14) c3[t++] = s[q++];
        c3[t] = 0;
        ug_texto(v, 196, TB_H + 2 + DIR_ALT + 4, c1, C_AMARILLO);
        if (v->w > 480) {
            ug_texto(v, 470, TB_H + 2 + DIR_ALT + 4, "Tamano",
                     C_AMARILLO);
            ug_texto(v, 540, TB_H + 2 + DIR_ALT + 4, c3, C_AMARILLO);
        }
    }
    /* arbol + detalle (siempre visibles; el archivo se edita en
       el editor con Enter o doble clic) */
    {
        int ini, i;
        if (vis < 1) vis = 1;
        ini = scroll;
        /* arbol */
        {
            int ay = y0, ai;
            for (ai = ascroll; ai < narb && ay + UG_FH <= lista_y1(v);
                 ai++) {
                int xx = 6 + arb[ai].prof * 12;
                if (ai == asel)
                    ug_relleno(v, 4, ay, 184, UG_FH, T_SELECCION);
                if (arb[ai].tiene) {
                    char sgn[2];
                    sgn[0] = arb[ai].expandido ? '-' : '+';
                    sgn[1] = 0;
                    ug_rect(v, xx, ay + 3, 10, 10, T_PANEL_BORDE);
                    ug_texto(v, xx + 2, ay + 1, sgn, T_PANEL_TEXTO);
                }
                icono(6, xx + 14, ay, v);
                {
                    char nm[24];
                    corta_utf8(nm, arb[ai].nombre, 20);
                    ug_texto(v, xx + 32, ay + 1, nm, T_PANEL_TEXTO);
                }
                ay += UG_FH;
            }
        }
        /* detalle */
        for (i = 0; i < vis && ini + i < nn; i++) {
            int yy = y0 + i * UG_FH, id = ini + i;
            char nm[48], tm[12];
            if (id == sel)
                ug_relleno(v, 192, yy, v->w - 196, UG_FH, T_SELECCION);
            icono(icono_de(tipos[id], nombres[id]), 196, yy, v);
            corta_utf8(nm, nombres[id], 34);
            ug_texto(v, 216, yy + 1, nm,
                     id == sel ? C_BLANCO : C_GRIS);
            if (v->w > 480) {
                if (tipos[id] == 2) {
                    texto_tam(tm, tams[id]);
                    ug_texto(v, 470, yy + 1, tm, C_GRIS);
                }
                ug_texto(v, 540, yy + 1,
                         texto_tipo(tipos[id], nombres[id]), C_GRIS);
            }
        }
        if (!nn)
            ug_texto(v, 216, y0 + 4, "(vacia)", C_GRIS_OSCURO);
    }
    /* pie: estado + ayuda o mensaje */
    {
        char est[128];
        int t = 0, q = 0;
        char nb[12];
        int yb = (v->h - UG_CHROME) - PIE_ALT;
        u_itoa(nn, nb);
        while (nb[q] && t < 120) est[t++] = nb[q++];
        {
            const char *s = nn == 1 ? " elemento | " : " elementos | ";
            q = 0;
            while (s[q] && t < 120) est[t++] = s[q++];
        }
        {
            const char *s = orden ? "orden: tipo | " : "orden: nombre | ";
            q = 0;
            while (s[q] && t < 120) est[t++] = s[q++];
        }
        q = 0;
        while (rt[q] && t < 120) est[t++] = rt[q++];
        est[t] = 0;
        ug_texto(v, 4, yb, est, C_GRIS);
        if (msg[0]) ug_texto(v, 4, yb + 18, msg, C_ROJO_CLARO);
        else ug_texto(v, 4, yb + 18,
                      "enter=abrir retroceso=subir n/d/r/c/t/v/x/p/o/u",
                      C_GRIS_OSCURO);
    }
    /* pedir nombre */
    if (pidiendo) {
        const char *tit = pidiendo == 1 ? "Nuevo archivo:" :
            pidiendo == 2 ? "Nueva carpeta:" : "Renombrar a:";
        int yw = (v->h - UG_CHROME) / 2 - 30;
        if (yw < 60) yw = 60;
        ug_panel(v, 60, yw, v->w - 120, 64, T_PANEL_BORDE, T_PANEL);
        ug_texto(v, 68, yw + 6, tit, C_AMARILLO);
        ug_campo(v, 68, yw + 24, v->w - 136, pbuf, ppos, 1);
        ug_texto(v, 68, yw + 46, "enter=ok esc=nada", C_GRIS_OSCURO);
    }
    /* propiedades */
    if (viendo_prop && sel >= 0 && sel < nn) {
        char ubi[128], det[64];
        int yw = (v->h - UG_CHROME) / 2 - 44, t = 0, q = 0;
        if (yw < 60) yw = 60;
        ug_panel(v, 60, yw, v->w - 120, 92, T_PANEL_BORDE, T_PANEL);
        ug_texto(v, 68, yw + 6, "Propiedades", C_AMARILLO);
        ruta_abs(ubi, nombres[sel]);
        {
            char nm[48];
            corta_utf8(nm, nombres[sel], 40);
            t = 0;
            q = 0;
            while (nm[q] && t < 56) det[t++] = nm[q++];
            det[t] = 0;
            ug_texto(v, 68, yw + 24, det, C_BLANCO);
            t = 0;
            q = 0;
            {
                const char *s = "Tipo: ";
                while (s[q] && t < 56) det[t++] = s[q++];
            }
            q = 0;
            {
                const char *s = texto_tipo(tipos[sel], nombres[sel]);
                while (s[q] && t < 56) det[t++] = s[q++];
            }
            det[t] = 0;
            ug_texto(v, 68, yw + 40, det, C_GRIS);
            if (tipos[sel] == 2) {
                t = 0;
                q = 0;
                {
                    const char *s = "Tamano: ";
                    while (s[q] && t < 56) det[t++] = s[q++];
                }
                {
                    char nb[12];
                    int qq = 0;
                    u_itoa((int)tams[sel], nb);
                    while (nb[qq] && t < 48) det[t++] = nb[qq++];
                }
                {
                    const char *s = " bytes";
                    q = 0;
                    while (s[q] && t < 56) det[t++] = s[q++];
                }
                det[t] = 0;
            } else if (tipos[sel] == 1) {
                int c = 0, k = 0;
                udir_t e;
                while (k < 200 && u_dir_hijo(idxs[sel], k, &e) >= 0) {
                    c++;
                    k++;
                }
                t = 0;
                {
                    const char *s = "Contiene: ";
                    q = 0;
                    while (s[q] && t < 56) det[t++] = s[q++];
                }
                {
                    char nb[12];
                    int qq = 0;
                    u_itoa(c, nb);
                    while (nb[qq] && t < 56) det[t++] = nb[qq++];
                }
                det[t] = 0;
            } else {
                u_strcpy(det, "Solo lectura (CD)");
            }
            ug_texto(v, 68, yw + 56, det, C_GRIS);
            corta_utf8(det, ubi, 52);
            ug_texto(v, 68, yw + 72, det, C_GRIS_OSCURO);
        }
    }
    /* visor: nada mas (ya dibujado) */
    (void)li;
    ug_fin(v);
}

static int en_boton_barra(int lx, int ly, int *b) {
    int i;
    for (i = 0; i < 12; i++) {
        int x = 4 + i * TB_CEL;
        if (ug_dentro(lx, ly, x, 2, TB_CEL, TB_H)) {
            *b = i;
            return 1;
        }
    }
    return 0;
}

static void pulsa_boton(int b) {
    if (b == 0) ir_atras();
    else if (b == 1) ir_adelante();
    else if (b == 2) ir_arriba();
    else if (b == 3) {
        recarga();
        u_strcpy(msg, "actualizado");
    } else if (b == 11) {
        /* Propiedades alterna: si ya se ve, la cierra (antes el
           retorno de viendo_prop de abajo la dejaba clavada y el
           raton no tenia forma de quitarla). */
        if (sel >= 0 && sel < nn) viendo_prop = !viendo_prop;
    } else if (viendo_prop) {
        return;
    } else if (b == 4) {
        pidiendo = 1;
        pbuf[0] = 0;
        ppos = 0;
    } else if (b == 5) {
        pidiendo = 2;
        pbuf[0] = 0;
        ppos = 0;
    } else if (b == 6) {
        pidiendo = 3;
        pbuf[0] = 0;
        ppos = 0;
    } else if (b == 7) {
        if (sel >= 0 && sel < nn && tipos[sel] == 2) {
            ruta_abs(porta_ruta, nombres[sel]);
            u_strcpy(porta_nom, nombres[sel]);
            porta = 1;
            u_strcpy(msg, "copiado: elige destino y Pegar");
        } else {
            u_strcpy(msg, tipos[sel] == 3 ? "CD: solo lectura" :
                     "solo archivos");
        }
    } else if (b == 8) {
        if (sel >= 0 && sel < nn && tipos[sel] == 2) {
            ruta_abs(porta_ruta, nombres[sel]);
            u_strcpy(porta_nom, nombres[sel]);
            porta = 2;
            u_strcpy(msg, "cortado: elige destino y Pegar");
        } else {
            u_strcpy(msg, tipos[sel] == 3 ? "CD: solo lectura" :
                     "solo archivos");
        }
    } else if (b == 9) {
        pega();
    } else if (b == 10) {
        borra_sel();
    }
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    mi_srv = srv;
    msg[0] = 0;
    if (ug_abre(&v, srv, 640, 460, "explorador")) return 98;
    ug_edit(&v, 1);   /* TAB y ESC llegan a la app (dialogos y visor) */
    recarga();
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        int vis = lista_vis(&v);
        if (t == UG_EV_SALIR) return 0;
        if (t == UG_EV_TECLA) {
            int k = ev.tecla;
            if (pidiendo) {
                if (k == 13) confirma_nombre(&v);
                else if (k == 8 && ppos == 0) pidiendo = 0;
                else if (k == 27) pidiendo = 0;
                else ug_campo_tecla(pbuf, NNOM, &ppos, k);
            } else if (viendo_prop) {
                if (k == 27 || k == 13 || k == 8 || k == 'p' ||
                    k == 'P')
                    viendo_prop = 0;
            } else if (k == TEC_ARRIBA_U) {
                if (sel > 0) {
                    sel--;
                    if (sel < scroll) scroll = sel;
                    msg[0] = 0;
                }
            } else if (k == TEC_ABAJO_U) {
                if (sel + 1 < nn) {
                    sel++;
                    if (sel >= scroll + vis) scroll = sel - vis + 1;
                    msg[0] = 0;
                }
            } else if (k == TEC_INICIO_U) {
                sel = 0;
                scroll = 0;
            } else if (k == TEC_FIN_U) {
                if (nn > 0) {
                    sel = nn - 1;
                    scroll = sel - vis + 1;
                    if (scroll < 0) scroll = 0;
                }
            } else if (k == 13) {
                abre_sel(&v, 1);
            } else if (k == 8) {
                ir_arriba();
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
            } else if (k == 'c' || k == 'C') {
                pulsa_boton(7);
            } else if (k == 't' || k == 'T') {
                pulsa_boton(8);
            } else if (k == 'v' || k == 'V') {
                pulsa_boton(9);
            } else if (k == 'x' || k == 'X') {
                borra_sel();
            } else if (k == 'p' || k == 'P') {
                if (sel >= 0 && sel < nn) viendo_prop = !viendo_prop;
            } else if (k == 'o' || k == 'O') {
                orden = orden ? 0 : 1;
                recarga();
                u_strcpy(msg, orden ? "orden: tipo" : "orden: nombre");
            } else if (k == 'u' || k == 'U') {
                recarga();
                u_strcpy(msg, "actualizado");
            } else if (k == 27 || k == 9) {
                /* se consumen: no salen al escritorio (ver ug_edit) */
            }
        } else if (t == UG_EV_RATON) {
            int lx = ev.rx - v.x, ly = ev.ry - (v.y + UG_CHROME);
            static int antes = 0;
            int b = -1;
            raton_lx = lx;
            raton_ly = ly;
            if (en_boton_barra(lx, ly, &b) && ug_clic(antes, ev.rbot)) {
                pulsa_boton(b);
            } else if (!pidiendo && viendo_prop) {
                /* Clic fuera del panel de propiedades lo cierra
                   (el panel no tiene [X]; antes solo ESC/Enter
                   por teclado lo quitaban). */
                int yw = (v.h - UG_CHROME) / 2 - 44;
                if (yw < 60) yw = 60;
                if (ug_clic(antes, ev.rbot) &&
                    !(lx >= 60 && lx < v.w - 60 &&
                      ly >= yw && ly < yw + 92))
                    viendo_prop = 0;
            } else if (!pidiendo && !viendo_prop) {
                /* cabeceras: ordena */
                int yc = TB_H + 2 + DIR_ALT + 4;
                if (ly >= yc && ly < yc + 16 && ug_clic(antes, ev.rbot)) {
                    if (lx >= 196 && (lx < 470 || v.w <= 480)) {
                        orden = 0;
                        recarga();
                    } else if (lx >= 540 || (lx >= 470 && v.w > 480)) {
                        orden = 1;
                        recarga();
                    }
                } else if (lx < 190) {
                    /* arbol */
                    int ay = lista_y0(), ai;
                    for (ai = ascroll; ai < narb; ai++) {
                        int xx = 6 + arb[ai].prof * 12;
                        if (ay + UG_FH > lista_y1(&v)) break;
                        if (ly >= ay && ly < ay + UG_FH &&
                            ug_clic(antes, ev.rbot)) {
                            if (arb[ai].tiene && lx >= xx &&
                                lx < xx + 10) {
                                if (arb[ai].expandido)
                                    saca_expandido(arb[ai].didx);
                                else
                                    mete_expandido(arb[ai].didx);
                                arbol_reconstruye();
                            } else {
                                ir_a_nodo(ai);
                            }
                            break;
                        }
                        ay += UG_FH;
                    }
                } else {
                    /* detalle: selecciona, doble clic abre */
                    int id = (ly - lista_y0()) / UG_FH + scroll;
                    if (ly >= lista_y0() && id >= 0 && id < nn &&
                        ug_clic(antes, ev.rbot)) {
                        unsigned int ahora = (unsigned int)u_ticks();
                        sel = id;
                        msg[0] = 0;
                        if (id == ultimo_id &&
                            ahora - ultimo_clic < 50) {
                            abre_sel(&v, 1);
                            ultimo_id = -1;
                        } else {
                            ultimo_id = id;
                            ultimo_clic = ahora;
                        }
                    }
                }
            }
            if (ev.rueda) {
                /* rueda sobre el arbol o el detalle */
                if (lx < 190) {
                    ascroll += ev.rueda > 0 ? -2 : 2;
                    if (ascroll < 0) ascroll = 0;
                } else {
                    scroll += ev.rueda > 0 ? -3 : 3;
                    if (scroll < 0) scroll = 0;
                    if (scroll > sel) scroll = sel;
                }
            }
            antes = ev.rbot;
            (void)vis;
        } else if (t == UG_EV_DRAW) {
            dibuja(&v);
        }
    }
}
