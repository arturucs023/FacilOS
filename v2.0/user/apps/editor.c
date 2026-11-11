#include "ugfx.h"
#include "utext.h"

void __main(void) {
}

/* Editor avanzado v2.2: multidocumento, seleccion raton/teclado, B/I/U +
 * color por software, undo/redo, portapapeles interno, buscar/reemplazar,
 * pestañas, dialogos, iconos propios (tema 24-40), tooltips, temas y
 * ajuste de linea. Persistencia .txt (UTF-8 puro) y .f32r (formato v1).
 * Todo Ring3; unico cambio de nucleo: syscall 49 de modificadores. */

#define MAXDOC 4
#define MAXTXT 32768
#define MAXUND 128
#define MAXCLP 8192
#define MAXHIT 200
#define NNOM 128

#define EST_B 1
#define EST_I 2
#define EST_S 4

/* color indice -> RGB (tema se aplica en dibujo) */
static int color_rgb(int idx, int tema_claro, int pers_r, int pers_g,
                     int pers_b) {
    if (idx == 0) return tema_claro ? RGB24(10, 14, 22) : RGB24(170, 170, 170);
    if (idx == 1) return tema_claro ? RGB24(20, 20, 20) : RGB24(245, 245, 245);
    if (idx == 2) return RGB24(200, 60, 60);
    if (idx == 3) return RGB24(90, 140, 230);
    if (idx == 4) return RGB24(70, 170, 90);
    if (idx == 5) return RGB24(230, 200, 70);
    if (idx == 6) return RGB24(230, 140, 60);
    if (idx == 7) return RGB24(170, 110, 220);
    if (idx == 8) return RGB24(150, 160, 175);
    return RGB24(pers_r, pers_g, pers_b);
}

static const char *color_nom(int i) {
    if (i == 0) return "negro";
    if (i == 1) return "blanco";
    if (i == 2) return "rojo";
    if (i == 3) return "azul";
    if (i == 4) return "verde";
    if (i == 5) return "amarillo";
    if (i == 6) return "naranja";
    if (i == 7) return "morado";
    if (i == 8) return "gris";
    return "propio";
}

/* ---- documentos ---- */
typedef struct {
    int uso;
    char *txt;
    unsigned char *est, *col;
    int len, cap;
    char ruta[NNOM];
    int sucio, es_f32r;
    int cur, anc;               /* cursor y ancla (bytes) */
    int slin, scol;             /* scroll: 1a fila visual + 1a celda */
    int pen_est, pen_col;
    /* undo */
    int u_tipo[MAXUND], u_pos[MAXUND], u_len[MAXUND];
    unsigned char *u_d0[MAXUND], *u_d1[MAXUND];
    int u_ne[MAXUND], u_nc[MAXUND], u_n;
    int r_tipo[MAXUND], r_pos[MAXUND], r_len[MAXUND];
    unsigned char *r_d0[MAXUND], *r_d1[MAXUND];
    int r_ne[MAXUND], r_nc[MAXUND], r_n;
} doc_t;

static doc_t docs[MAXDOC];
static int ndoc = 0, dact = -1;
static int tema_claro = 0, ajuste = 0;
static int pers_r = 0, pers_g = 180, pers_b = 180;

/* portapapeles interno (texto + estilos) */
static char *clip_t = 0;
static unsigned char *clip_e = 0, *clip_c = 0;
static int clip_n = 0;

/* buscar */
static char bus_q[64], bus_r[64];
static int bus_may = 1, bus_pal = 0, bus_nh = 0, bus_sel = -1;
static int bus_pos[MAXHIT];

/* dialogo: 0 ninguno, 1 abrir, 2 guardar-como, 3 confirma-cierre,
   4 sobreescribir, 5 error/info, 6 color, 7 ir-linea */
static int dlg = 0, dlg_accion = 0;
static char dlg_campo[NNOM];
static int dlg_pos = 0;
static char dlg_msg[96], dlg_ruta[NNOM];
static int dlg_cr = 64, dlg_cg = 180, dlg_cb = 180, dlg_ccol = 9;
/* mini selector de archivos (dlg 1 abrir, 2 guardar como): directorio
   actual + pila desde la raiz (el FS no expone padre por syscall). */
static int dlg_dir = 0;
static char dlg_pila[8][32];
static int dlg_pidx[8];
static int dlg_prof = 0;
static char dlg_noms[64][40];
static int dlg_tipos[64], dlg_idx[64];
static int dlg_nn = 0, dlg_sel = 0, dlg_scroll = 0;
static unsigned int dlg_ult_clic = 0;
static int dlg_ult_id = -1;
static char msg[96];
static int confirma_sal = 0;
static int bus_panel = 0;
static int bus_pos_campo = 0;   /* 0 consulta, 1 reemplazo */
static int bus_cpos = 0, bus_rpos = 0;

/* prototipos (el dibujo los usa antes de su definicion) */
static void copia(char *d, int *t, const char *s);
static void num(char *d, int *t, int v);
static int ut_largo_n(const char *s, int n);
static const char *nb_lin(int n);
static void dibuja_bus(ug_win_t *v);
static void dibuja_dlg_arch(ug_win_t *v, const char *tit);
static void dibuja_dlg_conf(ug_win_t *v, const char *tit, const char *pre);
static void dibuja_dlg_msg(ug_win_t *v);
static void dibuja_dlg_color(ug_win_t *v);
static void dibuja_dlg_ir(ug_win_t *v);
static void dlg_browser_ini(const char *ruta);

/* raton */
static int rat_lx = -1, rat_ly = -1, rat_bot = 0;
static int selec_arrastra = 0, hover_btn = -1, hover_tip_t = 0;
static int sb_arrastra = 0;

static int mi_srv = 0;

static void pon_msg(const char *s) {
    int i = 0;
    while (s[i] && i < 95) { msg[i] = s[i]; i++; }
    msg[i] = 0;
}

static doc_t *DA(void) {
    if (dact < 0 || dact >= MAXDOC || !docs[dact].uso) return 0;
    return &docs[dact];
}

/* ---- utilidades UTF-8 sobre el buffer ---- */
static int es_cont(unsigned char b) { return (b & 0xC0) == 0x80; }

static int ant_c(const char *s, int p) {
    int q = p - 1;
    if (q < 0) return 0;
    while (q > 0 && es_cont((unsigned char)s[q])) q--;
    return q;
}

static int sig_c(const char *s, int p, int len) {
    int av = 0;
    if (p >= len) return len;
    ut_dec(s + p, len - p > 4 ? 4 : len - p, &av);
    if (av <= 0) av = 1;
    p += av;
    return p > len ? len : p;
}

static int cuenta_car(const char *s, int ini, int fin) {
    int n = 0, p = ini;
    while (p < fin) { p = sig_c(s, p, fin); n++; }
    return n;
}

static int es_palabra(unsigned cp, const char *p, int av) {
    (void)p;
    if (cp == UT_REEMPLAZO) return 0;
    if (cp == ' ' || cp == '\t' || cp == '\n') return 0;
    if (av > 1) return 1;
    {
        char c = (char)cp;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') return 1;
        if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!' ||
            c == '?' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '"' || c == '\'' || c == '-' ||
            c == '+' || c == '*' || c == '/' || c == '=' || c == '<' ||
            c == '>' || c == '|')
            return 0;
        return 1;
    }
}

/* ---- undo ---- */
static void undo_libera_op(doc_t *d, int redo, int i) {
    if (!redo) {
        if (d->u_d0[i]) u_free(d->u_d0[i]);
        if (d->u_d1[i]) u_free(d->u_d1[i]);
        d->u_d0[i] = d->u_d1[i] = 0;
    } else {
        if (d->r_d0[i]) u_free(d->r_d0[i]);
        if (d->r_d1[i]) u_free(d->r_d1[i]);
        d->r_d0[i] = d->r_d1[i] = 0;
    }
}

static void undo_tira_redo(doc_t *d) {
    int i;
    for (i = 0; i < d->r_n; i++) undo_libera_op(d, 1, i);
    d->r_n = 0;
}

static void undo_mete(doc_t *d, int tipo, int pos, int len,
                      unsigned char *a, unsigned char *b, int ne, int nc) {
    int i;
    if (d->u_n >= MAXUND) {
        undo_libera_op(d, 0, 0);
        for (i = 1; i < MAXUND; i++) {
            d->u_tipo[i - 1] = d->u_tipo[i];
            d->u_pos[i - 1] = d->u_pos[i];
            d->u_len[i - 1] = d->u_len[i];
            d->u_d0[i - 1] = d->u_d0[i];
            d->u_d1[i - 1] = d->u_d1[i];
            d->u_ne[i - 1] = d->u_ne[i];
            d->u_nc[i - 1] = d->u_nc[i];
        }
        d->u_n = MAXUND - 1;
    }
    i = d->u_n++;
    d->u_tipo[i] = tipo;
    d->u_pos[i] = pos;
    d->u_len[i] = len;
    d->u_d0[i] = a;
    d->u_d1[i] = b;
    d->u_ne[i] = ne;
    d->u_nc[i] = nc;
    undo_tira_redo(d);
}

static void redo_mete(doc_t *d, int tipo, int pos, int len,
                      unsigned char *a, unsigned char *b, int ne, int nc) {
    int i;
    if (d->r_n >= MAXUND) {
        undo_libera_op(d, 1, 0);
        for (i = 1; i < MAXUND; i++) {
            d->r_tipo[i - 1] = d->r_tipo[i];
            d->r_pos[i - 1] = d->r_pos[i];
            d->r_len[i - 1] = d->r_len[i];
            d->r_d0[i - 1] = d->r_d0[i];
            d->r_d1[i - 1] = d->r_d1[i];
            d->r_ne[i - 1] = d->r_ne[i];
            d->r_nc[i - 1] = d->r_nc[i];
        }
        d->r_n = MAXUND - 1;
    }
    i = d->r_n++;
    d->r_tipo[i] = tipo;
    d->r_pos[i] = pos;
    d->r_len[i] = len;
    d->r_d0[i] = a;
    d->r_d1[i] = b;
    d->r_ne[i] = ne;
    d->r_nc[i] = nc;
}

/* ---- operaciones de texto (con undo) ---- */
static int cabe(doc_t *d, int n) { return d->len + n <= MAXTXT; }

/* inserta n bytes en pos (est/col paralelos o pen). registra=1 undo. */
static int doc_ins(doc_t *d, int pos, const char *t, const unsigned char *e,
                   const unsigned char *c, int n, int registra) {
    unsigned char *de = 0, *dc = 0;
    int i;
    if (n <= 0) return 0;
    if (!cabe(d, n)) { pon_msg("documento lleno (32KB)"); return -1; }
    if (pos < 0) pos = 0;
    if (pos > d->len) pos = d->len;
    if (registra) {
        if (n <= MAXCLP) {
            de = (unsigned char *)u_malloc(n > 0 ? n : 1);
            dc = (unsigned char *)u_malloc(n > 0 ? n : 1);
            if (!de || !dc) {
                if (de) u_free(de);
                if (dc) u_free(dc);
                de = dc = 0;   /* sin undo en este paso */
            } else {
                for (i = 0; i < n; i++) {
                    de[i] = e ? e[i] : (unsigned char)d->pen_est;
                    dc[i] = c ? c[i] : (unsigned char)d->pen_col;
                }
            }
        }
        /* guarda estilos para rehacer (d0=est, d1=col) */
        if (n > MAXCLP) {
            /* op grande: vacia undo para acotar memoria */
            for (i = 0; i < d->u_n; i++) undo_libera_op(d, 0, i);
            d->u_n = 0;
        } else if (de) {
            undo_mete(d, 1, pos, n, de, dc, 0, 0);
        }
    }
    for (i = d->len - 1; i >= pos; i--) {
        d->txt[i + n] = d->txt[i];
        d->est[i + n] = d->est[i];
        d->col[i + n] = d->col[i];
    }
    for (i = 0; i < n; i++) {
        d->txt[pos + i] = t[i];
        d->est[pos + i] = e ? e[i] : (unsigned char)d->pen_est;
        d->col[pos + i] = c ? c[i] : (unsigned char)d->pen_col;
    }
    d->len += n;
    d->sucio = 1;
    bus_nh = 0;
    bus_sel = -1;
    return 0;
}

/* borra [pos,pos+n). registra undo con los bytes borrados. */
static int doc_del(doc_t *d, int pos, int n, int registra) {
    unsigned char *de = 0, *dc = 0, *dt = 0;
    int i;
    if (n <= 0) return 0;
    if (pos < 0) pos = 0;
    if (pos + n > d->len) n = d->len - pos;
    if (n <= 0) return 0;
    if (registra) {
        if (n <= MAXCLP) {
            dt = (unsigned char *)u_malloc(n);
            de = (unsigned char *)u_malloc(n * 2);
            if (!dt || !de) {
                if (dt) u_free(dt);
                if (de) u_free(de);
                dt = de = 0;
            } else {
                for (i = 0; i < n; i++) {
                    dt[i] = (unsigned char)d->txt[pos + i];
                    de[i] = d->est[pos + i];
                    de[n + i] = d->col[pos + i];
                }
                undo_mete(d, 2, pos, n, dt, de, 0, 0);
            }
        } else {
            for (i = 0; i < d->u_n; i++) undo_libera_op(d, 0, i);
            d->u_n = 0;
        }
        (void)dc;
    }
    for (i = pos; i + n < d->len; i++) {
        d->txt[i] = d->txt[i + n];
        d->est[i] = d->est[i + n];
        d->col[i] = d->col[i + n];
    }
    d->len -= n;
    d->sucio = 1;
    bus_nh = 0;
    bus_sel = -1;
    return 0;
}

static void sel_rango(doc_t *d, int *a, int *b) {
    if (d->anc < d->cur) { *a = d->anc; *b = d->cur; }
    else { *a = d->cur; *b = d->anc; }
}

static int hay_sel(doc_t *d) { return d->anc != d->cur; }

/* aplica estilo/color a [a,b): bits: +1 pon, -1 quita, 0 deja; col -1 deja */
static int doc_estilo(doc_t *d, int a, int b, int bit, int modo, int coln,
                      int registra) {
    int i, n;
    unsigned char *old = 0;
    if (a < 0) a = 0;
    if (b > d->len) b = d->len;
    if (a >= b) return 0;
    n = b - a;
    if (registra) {
        if (n <= MAXCLP) {
            old = (unsigned char *)u_malloc(n * 2);
            if (old) {
                for (i = 0; i < n; i++) {
                    old[i] = d->est[a + i];
                    old[n + i] = d->col[a + i];
                }
            }
        }
        {
            int ne2 = 0, nc2 = -1;
            if (bit == EST_B || bit == (EST_B | EST_I) || bit == 7) {
                /* calcula nuevo para redo: aplica misma regla */
            }
            (void)ne2; (void)nc2;
            undo_mete(d, 3, a, n, old, 0, modo, coln);
            if (!old) {
                /* sin memoria: vacia y sigue sin undo */
            }
            d->u_ne[d->u_n - 1] = bit;
            d->u_nc[d->u_n - 1] = modo;
            /* guarda col nuevo en nc y bit en ne (ver deshacer) */
            d->u_nc[d->u_n - 1] = coln;
        }
    }
    for (i = a; i < b; i++) {
        if (bit) {
            if (modo > 0) d->est[i] |= (unsigned char)bit;
            else if (modo < 0)
                d->est[i] &= (unsigned char)~bit;
            else d->est[i] ^= (unsigned char)bit;
        }
        if (coln >= 0) d->col[i] = (unsigned char)coln;
    }
    d->sucio = 1;
    return 0;
}

static void doc_deshacer(doc_t *d) {
    int i, k;
    (void)k;
    if (!d->u_n) { pon_msg("nada que deshacer"); return; }
    i = --d->u_n;
    if (d->u_tipo[i] == 1) {
        /* ins -> borra sin registrar, guarda redo con txt+estilos */
        int p = d->u_pos[i], n = d->u_len[i];
        unsigned char *re = 0, *rc = 0;
        if (n <= MAXCLP && p + n <= d->len) {
            int k2;
            re = (unsigned char *)u_malloc(n > 0 ? n : 1);
            rc = (unsigned char *)u_malloc(n > 0 ? n * 2 : 2);
            if (re && rc) {
                for (k2 = 0; k2 < n; k2++) {
                    re[k2] = (unsigned char)d->txt[p + k2];
                    rc[k2] = d->est[p + k2];
                    rc[n + k2] = d->col[p + k2];
                }
            } else {
                if (re) u_free(re);
                if (rc) u_free(rc);
                re = rc = 0;
            }
        }
        doc_del(d, p, n, 0);
        redo_mete(d, 1, p, n, re, rc, 0, 0);
        if (d->cur > p) {
            d->cur -= n;
            if (d->cur < p) d->cur = p;
        }
        d->anc = d->cur;
        undo_libera_op(d, 0, i);
    } else if (d->u_tipo[i] == 2) {
        int p = d->u_pos[i], n = d->u_len[i];
        unsigned char *txt = d->u_d0[i], *ec = d->u_d1[i];
        unsigned char *re = 0;
        (void)re;
        if (txt && ec) {
            unsigned char *ee = ec, *cc = ec + n;
            /* reinsertar con estilos guardados */
            int j;
            if (cabe(d, n)) {
                for (j = d->len - 1; j >= p; j--) {
                    d->txt[j + n] = d->txt[j];
                    d->est[j + n] = d->est[j];
                    d->col[j + n] = d->col[j];
                }
                for (j = 0; j < n; j++) {
                    d->txt[p + j] = (char)txt[j];
                    d->est[p + j] = ee[j];
                    d->col[p + j] = cc[j];
                }
                d->len += n;
                d->sucio = 1;
            }
        }
        redo_mete(d, 2, p, n, 0, 0, 0, 0);
        d->cur = p + n;
        d->anc = d->cur;
        undo_libera_op(d, 0, i);
    } else {
        /* estilo: restaura old */
        int p = d->u_pos[i], n = d->u_len[i];
        unsigned char *old = d->u_d0[i];
        unsigned char *nw = 0;
        int bit = d->u_ne[i], coln = d->u_nc[i];
        if (old && p + n <= d->len) {
            int j;
            nw = (unsigned char *)u_malloc(n * 2);
            if (nw) {
                for (j = 0; j < n; j++) {
                    nw[j] = d->est[p + j];
                    nw[n + j] = d->col[p + j];
                }
                for (j = 0; j < n; j++) {
                    d->est[p + j] = old[j];
                    d->col[p + j] = old[n + j];
                }
                d->sucio = 1;
            }
        }
        redo_mete(d, 3, p, n, nw, 0, bit, coln);
        undo_libera_op(d, 0, i);
    }
}

static void doc_rehacer(doc_t *d) {
    int i, k;
    (void)k;
    if (!d->r_n) { pon_msg("nada que rehacer"); return; }
    i = --d->r_n;
    if (d->r_tipo[i] == 1) {
        int p = d->r_pos[i], n = d->r_len[i];
        unsigned char *txt = d->r_d0[i], *ec = d->r_d1[i];
        if (txt && ec && n <= MAXCLP && cabe(d, n)) {
            int j;
            for (j = d->len - 1; j >= p; j--) {
                d->txt[j + n] = d->txt[j];
                d->est[j + n] = d->est[j];
                d->col[j + n] = d->col[j];
            }
            for (j = 0; j < n; j++) {
                d->txt[p + j] = (char)txt[j];
                d->est[p + j] = ec[j];
                d->col[p + j] = ec[n + j];
            }
            d->len += n;
            d->sucio = 1;
            /* re-registra en undo */
            {
                unsigned char *de = (unsigned char *)u_malloc(n);
                unsigned char *dc = (unsigned char *)u_malloc(n);
                if (de && dc) {
                    for (j = 0; j < n; j++) {
                        de[j] = d->est[p + j];
                        dc[j] = d->col[p + j];
                    }
                    undo_mete(d, 1, p, n, de, dc, 0, 0);
                } else {
                    if (de) u_free(de);
                    if (dc) u_free(dc);
                }
            }
            d->cur = p + n;
            d->anc = d->cur;
        }
        undo_libera_op(d, 1, i);
    } else if (d->r_tipo[i] == 2) {
        int p = d->r_pos[i], n = d->r_len[i];
        /* re-registra undo con los bytes que se van a borrar */
        if (n > 0 && n <= MAXCLP && p + n <= d->len) {
            unsigned char *t2 = (unsigned char *)u_malloc(n);
            unsigned char *e2 = (unsigned char *)u_malloc(n * 2);
            if (t2 && e2) {
                int j;
                for (j = 0; j < n; j++) {
                    t2[j] = (unsigned char)d->txt[p + j];
                    e2[j] = d->est[p + j];
                    e2[n + j] = d->col[p + j];
                }
                doc_del(d, p, n, 0);
                undo_mete(d, 2, p, n, t2, e2, 0, 0);
            } else {
                if (t2) u_free(t2);
                if (e2) u_free(e2);
                doc_del(d, p, n, 0);
            }
        } else {
            doc_del(d, p, n, 0);
        }
        undo_libera_op(d, 1, i);
    } else {
        int p = d->r_pos[i], n = d->r_len[i];
        unsigned char *nw = d->r_d0[i];
        int bit = d->r_ne[i], coln = d->r_nc[i];
        if (nw && p + n <= d->len) {
            int j;
            unsigned char *old = (unsigned char *)u_malloc(n * 2);
            if (old) {
                for (j = 0; j < n; j++) {
                    old[j] = d->est[p + j];
                    old[n + j] = d->col[p + j];
                }
                for (j = 0; j < n; j++) {
                    d->est[p + j] = nw[j];
                    d->col[p + j] = nw[n + j];
                }
                d->sucio = 1;
                undo_mete(d, 3, p, n, old, 0, bit, coln);
                d->u_ne[d->u_n - 1] = bit;
                d->u_nc[d->u_n - 1] = coln;
            }
        }
        undo_libera_op(d, 1, i);
    }
}

/* ---- lineas logicas ---- */
static int nlineas(doc_t *d) {
    int n = 1, i;
    for (i = 0; i < d->len; i++)
        if (d->txt[i] == '\n') n++;
    return n;
}

static int lin_inicio(doc_t *d, int nl) {
    int i = 0, l = 0;
    if (nl <= 0) return 0;
    while (i < d->len && l < nl) {
        if (d->txt[i] == '\n') l++;
        i++;
    }
    return i > d->len ? d->len : i;
}

static int lin_fin(doc_t *d, int ini) {
    int i = ini;
    while (i < d->len && d->txt[i] != '\n') i++;
    return i;
}

static int lin_de_pos(doc_t *d, int pos) {
    int l = 0, i;
    if (pos > d->len) pos = d->len;
    for (i = 0; i < pos; i++)
        if (d->txt[i] == '\n') l++;
    return l;
}

/* celdas (caracteres) entre ini y pos */
static int celdas(doc_t *d, int ini, int pos) {
    int n = 0, p = ini;
    if (p < 0) p = 0;
    if (pos > d->len) pos = d->len;
    while (p < pos) { p = sig_c(d->txt, p, pos); n++; }
    return n;
}

/* byte de la celda c en [ini,fin) */
static int byte_celda(doc_t *d, int ini, int fin, int c) {
    int p = ini, n = 0;
    while (p < fin && n < c) { p = sig_c(d->txt, p, fin); n++; }
    return p;
}

/* ---- documentos: crear/cerrar/cargar/guardar ---- */
static int doc_nuevo(void) {
    int i;
    if (ndoc >= MAXDOC) { pon_msg("maximo 4 documentos"); return -1; }
    for (i = 0; i < MAXDOC; i++) {
        if (!docs[i].uso) break;
    }
    if (i >= MAXDOC) return -1;
    docs[i].txt = (char *)u_malloc(MAXTXT + 1);
    docs[i].est = (unsigned char *)u_malloc(MAXTXT + 1);
    docs[i].col = (unsigned char *)u_malloc(MAXTXT + 1);
    if (!docs[i].txt || !docs[i].est || !docs[i].col) {
        if (docs[i].txt) u_free(docs[i].txt);
        if (docs[i].est) u_free(docs[i].est);
        if (docs[i].col) u_free(docs[i].col);
        docs[i].txt = 0;
        pon_msg("sin memoria");
        return -1;
    }
    docs[i].uso = 1;
    docs[i].len = 0;
    docs[i].cap = MAXTXT;
    docs[i].ruta[0] = 0;
    docs[i].sucio = 0;
    docs[i].es_f32r = 0;
    docs[i].cur = docs[i].anc = 0;
    docs[i].slin = docs[i].scol = 0;
    docs[i].pen_est = 0;
    docs[i].pen_col = tema_claro ? 0 : 1;
    docs[i].u_n = docs[i].r_n = 0;
    ndoc++;
    dact = i;
    bus_nh = 0;
    bus_sel = -1;
    return i;
}

static void doc_cierra_mem(int i) {
    int k;
    if (!docs[i].uso) return;
    if (docs[i].txt) u_free(docs[i].txt);
    if (docs[i].est) u_free(docs[i].est);
    if (docs[i].col) u_free(docs[i].col);
    docs[i].txt = 0;
    for (k = 0; k < docs[i].u_n; k++) undo_libera_op(&docs[i], 0, k);
    for (k = 0; k < docs[i].r_n; k++) undo_libera_op(&docs[i], 1, k);
    docs[i].uso = 0;
    ndoc--;
}

static int termina_en(const char *r, const char *ext) {
    int lr = u_strlen(r), le = u_strlen(ext), i;
    if (lr < le) return 0;
    for (i = 0; i < le; i++) {
        char a = r[lr - le + i], b = ext[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b) return 0;
    }
    return 1;
}

/* valida y carga .f32r. 0 ok, -1 corrupto, -2 sin memoria, -3 no es f32r */
static int carga_f32r(doc_t *d, const char *ruta) {
    UFILE *f = u_fopen(ruta, "r");
    char cab[6];
    unsigned char lb[4];
    unsigned int tlen = 0, nrun = 0, i;
    if (!f) return -3;
    if (u_fread(cab, 6, f) != 6 || cab[0] != 'F' || cab[1] != '3' ||
        cab[2] != '2' || cab[3] != 'R' || cab[4] != '1' ||
        cab[5] != '\n') {
        u_fclose(f);
        return -3;
    }
    if (u_fread(lb, 4, f) != 4) { u_fclose(f); return -1; }
    tlen = (unsigned)lb[0] | ((unsigned)lb[1] << 8) |
           ((unsigned)lb[2] << 16) | ((unsigned)lb[3] << 24);
    if (tlen > (unsigned)MAXTXT) { u_fclose(f); return -1; }
    if ((int)tlen > 0 && u_fread(d->txt, (int)tlen, f) != (int)tlen) {
        u_fclose(f);
        return -1;
    }
    if (!ut_valido(d->txt, (int)tlen)) { u_fclose(f); return -1; }
    if (u_fread(lb, 4, f) != 4) { u_fclose(f); return -1; }
    nrun = (unsigned)lb[0] | ((unsigned)lb[1] << 8) |
           ((unsigned)lb[2] << 16) | ((unsigned)lb[3] << 24);
    if (nrun > tlen + 1 || nrun > 4096) { u_fclose(f); return -1; }
    d->len = (int)tlen;
    {
        int k;
        for (k = 0; k < d->len; k++) { d->est[k] = 0; d->col[k] = 1; }
    }
    for (i = 0; i < nrun; i++) {
        unsigned char rb[10];
        unsigned int a, b;
        if (u_fread((char *)rb, 10, f) != 10) { u_fclose(f); return -1; }
        a = (unsigned)rb[0] | ((unsigned)rb[1] << 8) |
            ((unsigned)rb[2] << 16) | ((unsigned)rb[3] << 24);
        b = (unsigned)rb[4] | ((unsigned)rb[5] << 8) |
            ((unsigned)rb[6] << 16) | ((unsigned)rb[7] << 24);
        if (a > b || b > tlen || rb[8] > 7 || rb[9] > 9) {
            u_fclose(f);
            return -1;
        }
        if (i > 0) {
            /* orden: el anterior debe terminar <= este inicio */
            /* (no guardamos anterior: revalida al final por solape) */
        }
        {
            unsigned int k;
            for (k = a; k < b; k++) {
                d->est[k] = rb[8];
                d->col[k] = rb[9];
            }
        }
    }
    u_fclose(f);
    /* solape: revalida rangos UTF-8 (runs no parten caracteres) */
    {
        int p = 0;
        while (p < d->len) {
            int av = 0;
            ut_dec(d->txt + p, d->len - p > 4 ? 4 : d->len - p, &av);
            if (av <= 0) av = 1;
            {
                int k;
                for (k = 1; k < av && p + k < d->len; k++) {
                    d->est[p + k] = d->est[p];
                    d->col[p + k] = d->col[p];
                }
            }
            p += av;
        }
    }
    return 0;
}

static int doc_carga(doc_t *d, const char *ruta) {
    int r = carga_f32r(d, ruta);
    int i;
    if (r == 0) {
        int q = 0;
        while (ruta[q] && q < NNOM - 1) { d->ruta[q] = ruta[q]; q++; }
        d->ruta[q] = 0;
        d->es_f32r = 1;
        d->sucio = 0;
        d->cur = d->anc = 0;
        d->slin = d->scol = 0;
        d->u_n = d->r_n = 0;
        return 0;
    }
    if (r == -1) {
        /* corrupto: conserva archivo, ofrece texto si es UTF-8 */
        UFILE *f = u_fopen(ruta, "r");
        char tmp[512];
        int n, tot = 0, ok = 1;
        d->len = 0;
        if (!f) return -1;
        while (tot < MAXTXT && (n = u_fread(tmp, 512, f)) > 0) {
            int k;
            for (k = 0; k < n && tot < MAXTXT; k++)
                d->txt[tot++] = tmp[k];
            if (tot >= MAXTXT) break;
        }
        u_fclose(f);
        d->len = tot;
        if (!ut_valido(d->txt, d->len)) ok = 0;
        for (i = 0; i < d->len; i++) { d->est[i] = 0; d->col[i] = 1; }
        if (!ok) return -1;
        /* abierto como texto de emergencia (ruta intacta) */
        {
            int q = 0;
            while (ruta[q] && q < NNOM - 1) { d->ruta[q] = ruta[q]; q++; }
            d->ruta[q] = 0;
        }
        d->es_f32r = 0;
        d->sucio = 1;
        d->cur = d->anc = 0;
        return -2;   /* corrupto pero texto recuperado */
    }
    /* texto normal */
    {
        UFILE *f = u_fopen(ruta, "r");
        int n, tot = 0;
        char tmp[512];
        d->len = 0;
        if (!f) return -1;
        while (tot < MAXTXT && (n = u_fread(tmp, 512, f)) > 0) {
            int k;
            for (k = 0; k < n && tot < MAXTXT; k++)
                d->txt[tot++] = tmp[k];
            if (tot >= MAXTXT) break;
        }
        u_fclose(f);
        d->len = tot;
        if (!ut_valido(d->txt, d->len)) {
            /* bytes invalidos: no se abre como texto (evita corrupcion) */
            d->len = 0;
            return -1;
        }
        for (i = 0; i < d->len; i++) { d->est[i] = 0; d->col[i] = 1; }
        {
            int q = 0;
            while (ruta[q] && q < NNOM - 1) { d->ruta[q] = ruta[q]; q++; }
            d->ruta[q] = 0;
        }
        d->es_f32r = 0;
        d->sucio = 0;
        d->cur = d->anc = 0;
        d->slin = d->scol = 0;
        d->u_n = d->r_n = 0;
        return tot >= MAXTXT ? 1 : 0;   /* 1 = truncado */
    }
}

/* guarda: .f32r con formato, resto texto puro. 0 ok. */
static int doc_guarda(doc_t *d, const char *ruta) {
    UFILE *f;
    int n, tot = 0, i;
    int fmt = termina_en(ruta, ".f32r");
    if (!ruta[0]) { pon_msg("sin nombre: usa Guardar como"); return -1; }
    if (fmt) {
        /* runs coalescentes */
        int runs = 0, p = 0;
        while (p < d->len) {
            int q = p + 1;
            while (q < d->len && d->est[q] == d->est[p] &&
                   d->col[q] == d->col[p])
                q++;
            /* no partir caracter: ya son paralelos por byte, pero los
               limites deben caer en frontera (lo garantizan las
               operaciones por caracter) */
            runs++;
            p = q;
        }
        if (d->len == 0) runs = 0;
        f = u_fopen(ruta, "w");
        if (!f) { pon_msg("no se guarda"); return -1; }
        u_fwrite("F32R1\n", 6, f);
        {
            unsigned char lb[4];
            lb[0] = (unsigned char)(d->len & 255);
            lb[1] = (unsigned char)((d->len >> 8) & 255);
            lb[2] = (unsigned char)((d->len >> 16) & 255);
            lb[3] = (unsigned char)((d->len >> 24) & 255);
            if (u_fwrite((char *)lb, 4, f) != 4) {
                u_fclose(f);
                pon_msg("fallo al escribir");
                return -1;
            }
        }
        if (d->len > 0 && u_fwrite(d->txt, d->len, f) != d->len) {
            u_fclose(f);
            pon_msg("fallo al escribir");
            return -1;
        }
        {
            unsigned char lb[4];
            lb[0] = (unsigned char)(runs & 255);
            lb[1] = (unsigned char)((runs >> 8) & 255);
            lb[2] = 0;
            lb[3] = 0;
            if (u_fwrite((char *)lb, 4, f) != 4) {
                u_fclose(f);
                pon_msg("fallo al escribir");
                return -1;
            }
        }
        p = 0;
        while (p < d->len) {
            int q = p + 1;
            unsigned char rb[10];
            while (q < d->len && d->est[q] == d->est[p] &&
                   d->col[q] == d->col[p])
                q++;
            rb[0] = (unsigned char)(p & 255);
            rb[1] = (unsigned char)((p >> 8) & 255);
            rb[2] = (unsigned char)((p >> 16) & 255);
            rb[3] = (unsigned char)((p >> 24) & 255);
            rb[4] = (unsigned char)(q & 255);
            rb[5] = (unsigned char)((q >> 8) & 255);
            rb[6] = (unsigned char)((q >> 16) & 255);
            rb[7] = (unsigned char)((q >> 24) & 255);
            rb[8] = d->est[p];
            rb[9] = d->col[p];
            if (u_fwrite((char *)rb, 10, f) != 10) {
                u_fclose(f);
                pon_msg("fallo al escribir");
                return -1;
            }
            p = q;
        }
        u_fclose(f);
    } else {
        f = u_fopen(ruta, "w");
        if (!f) { pon_msg("no se guarda"); return -1; }
        while (tot < d->len) {
            int trozo = d->len - tot > 512 ? 512 : d->len - tot;
            n = u_fwrite(d->txt + tot, trozo, f);
            if (n != trozo) {
                u_fclose(f);
                pon_msg("fallo al escribir: reintenta");
                return -1;
            }
            tot += n;
        }
        u_fclose(f);
    }
    /* verifica tamano */
    {
        UFILE *v = u_fopen(ruta, "r");
        if (!v) { pon_msg("guardado sin verificar"); return -1; }
        {
            int t = u_fsize(v);
            u_fclose(v);
            if (!fmt && t != d->len) {
                pon_msg("verificacion corta: reintenta");
                return -1;
            }
        }
    }
    {
        int q = 0;
        while (ruta[q] && q < NNOM - 1) { d->ruta[q] = ruta[q]; q++; }
        d->ruta[q] = 0;
    }
    d->es_f32r = fmt;
    d->sucio = 0;
    {
        char b[64];
        int t = 0, q = 0;
        char nb[12];
        const char *s = "guardado ";
        while (*s && t < 50) b[t++] = *s++;
        u_itoa(fmt ? (int)(6 + 4 + d->len + 4 + 10 * 2) : d->len, nb);
        while (nb[q] && t < 60) b[t++] = nb[q++];
        b[t++] = 'B';
        b[t] = 0;
        pon_msg(b);
    }
    for (i = 0; i < d->u_n; i++) {
        /* al guardar no se limpia undo (sigue util) */
    }
    (void)i;
    return 0;
}

/* ---- portapapeles interno ---- */
static void clip_copia(doc_t *d) {
    int a, b, n, i;
    if (!hay_sel(d)) { pon_msg("nada seleccionado"); return; }
    sel_rango(d, &a, &b);
    n = b - a;
    if (n > MAXCLP) n = MAXCLP;
    if (clip_t) { u_free(clip_t); u_free(clip_e); u_free(clip_c); }
    clip_t = (char *)u_malloc(n > 0 ? n : 1);
    clip_e = (unsigned char *)u_malloc(n > 0 ? n : 1);
    clip_c = (unsigned char *)u_malloc(n > 0 ? n : 1);
    if (!clip_t || !clip_e || !clip_c) {
        if (clip_t) u_free(clip_t);
        if (clip_e) u_free(clip_e);
        if (clip_c) u_free(clip_c);
        clip_t = 0;
        pon_msg("sin memoria");
        return;
    }
    for (i = 0; i < n; i++) {
        clip_t[i] = d->txt[a + i];
        clip_e[i] = d->est[a + i];
        clip_c[i] = d->col[a + i];
    }
    clip_n = n;
    pon_msg("copiado (interno)");
}

static void clip_corta(doc_t *d) {
    clip_copia(d);
    if (!hay_sel(d)) return;
    {
        int a, b;
        sel_rango(d, &a, &b);
        doc_del(d, a, b - a, 1);
        d->cur = d->anc = a;
    }
    pon_msg("cortado (interno)");
}

static void clip_pega(doc_t *d) {
    int a, b;
    if (!clip_t || !clip_n) { pon_msg("nada que pegar"); return; }
    if (hay_sel(d)) {
        sel_rango(d, &a, &b);
        doc_del(d, a, b - a, 1);
        d->cur = d->anc = a;
    }
    if (!cabe(d, clip_n)) { pon_msg("no cabe al pegar"); return; }
    doc_ins(d, d->cur, clip_t, clip_e, clip_c, clip_n, 1);
    d->cur += clip_n;
    d->anc = d->cur;
}

/* ---- buscar ---- */
static int minusc(unsigned cp) {
    if (cp >= 'A' && cp <= 'Z') return (int)cp + 32;
    return (int)cp;
}

static int es_borde_pal(doc_t *d, int p) {
    int av = 0;
    unsigned cp;
    if (p <= 0 || p >= d->len) return 1;
    cp = ut_dec(d->txt + p, d->len - p > 4 ? 4 : d->len - p, &av);
    (void)cp;
    return 0;
}

static int coincide_en(doc_t *d, int p, const char *q, int qn) {
    int i = 0, k = p;
    while (i < qn) {
        int av1 = 0, av2 = 0;
        unsigned c1, c2;
        if (k >= d->len) return 0;
        c1 = ut_dec(d->txt + k, d->len - k > 4 ? 4 : d->len - k, &av1);
        c2 = ut_dec(q + i, qn - i > 4 ? 4 : qn - i, &av2);
        if (av1 <= 0) av1 = 1;
        if (av2 <= 0) av2 = 1;
        if (!bus_may) {
            if (minusc(c1) != minusc(c2)) return 0;
        } else {
            if (c1 != c2) return 0;
        }
        k += av1;
        i += av2;
    }
    if (bus_pal) {
        /* anterior debe ser borde */
        if (p > 0) {
            int a = ant_c(d->txt, p), av = 0;
            unsigned ca = ut_dec(d->txt + a, p - a > 4 ? 4 : p - a, &av);
            if (es_palabra(ca, d->txt + a, av)) return 0;
        }
        if (k < d->len) {
            int av = 0;
            unsigned ck =
                ut_dec(d->txt + k, d->len - k > 4 ? 4 : d->len - k, &av);
            if (es_palabra(ck, d->txt + k, av)) return 0;
        }
    }
    return k - p;
}

static void busca_todo(doc_t *d) {
    int qn = u_strlen(bus_q), p = 0;
    bus_nh = 0;
    bus_sel = -1;
    if (!qn || !d) return;
    while (p < d->len && bus_nh < MAXHIT) {
        int n = coincide_en(d, p, bus_q, qn);
        if (n > 0) {
            bus_pos[bus_nh++] = p;
            p += n > 0 ? n : 1;
            if (n <= 0) p = sig_c(d->txt, p, d->len);
        } else {
            p = sig_c(d->txt, p, d->len);
        }
    }
    /* selecciona el siguiente al cursor */
    {
        int i, best = -1;
        for (i = 0; i < bus_nh; i++) {
            if (bus_pos[i] >= d->cur) { best = i; break; }
        }
        if (best < 0 && bus_nh > 0) best = 0;
        bus_sel = best;
        if (best >= 0) {
            int qn2 = u_strlen(bus_q), f = bus_pos[best], k = f, i2 = 0;
            while (i2 < qn2 && k < d->len) {
                int av = 0;
                ut_dec(d->txt + k, d->len - k > 4 ? 4 : d->len - k, &av);
                if (av <= 0) av = 1;
                k += av;
                i2 += av;   /* aprox: q y doc avanzan igual en valido */
                /* correccion exacta: avanza q por decodificacion */
            }
            (void)f;
        }
    }
}

static int hit_fin(doc_t *d, int idx) {
    int qn = u_strlen(bus_q), p, i = 0;
    if (idx < 0 || idx >= bus_nh) return -1;
    p = bus_pos[idx];
    while (i < qn && p < d->len) {
        int av1 = 0, av2 = 0;
        ut_dec(d->txt + p, d->len - p > 4 ? 4 : d->len - p, &av1);
        ut_dec(bus_q + i, qn - i > 4 ? 4 : qn - i, &av2);
        if (av1 <= 0) av1 = 1;
        if (av2 <= 0) av2 = 1;
        p += av1;
        i += av2;
    }
    return p;
}

static void ir_a_hit(doc_t *d, int idx) {
    int f;
    if (idx < 0 || idx >= bus_nh) return;
    bus_sel = idx;
    f = hit_fin(d, idx);
    if (f < 0) return;
    d->anc = bus_pos[idx];
    d->cur = f;
}

/* cambia la coincidencia actual por bus_r */
static void bus_cambia(doc_t *d) {
    int f, p, qn;
    if (!d || bus_sel < 0 || bus_sel >= bus_nh) {
        if (d) {
            busca_todo(d);
            if (bus_nh <= 0) pon_msg("sin coincidencias");
        }
        return;
    }
    f = hit_fin(d, bus_sel);
    p = bus_pos[bus_sel];
    qn = u_strlen(bus_r);
    if (f <= p) return;
    doc_del(d, p, f - p, 1);
    if (qn > 0) doc_ins(d, p, bus_r, 0, 0, qn, 1);
    d->cur = d->anc = p + qn;
    busca_todo(d);
}

/* cambia todas las coincidencias por bus_r */
static void bus_todas(doc_t *d) {
    int cnt = 0;
    char b2[32];
    int t2 = 0;
    if (!d) return;
    busca_todo(d);
    while (bus_nh > 0 && cnt < 200) {
        int f = hit_fin(d, 0);
        int p = bus_pos[0];
        int qn = u_strlen(bus_r);
        if (f <= p) break;
        doc_del(d, p, f - p, 1);
        if (qn > 0) doc_ins(d, p, bus_r, 0, 0, qn, 1);
        cnt++;
        busca_todo(d);
    }
    copia(b2, &t2, "cambios: ");
    num(b2, &t2, cnt);
    b2[t2] = 0;
    pon_msg(b2);
}

/* ---- iconos + botones ---- */
static void icono(int idx, int x, int y, ug_win_t *v) {
    int r, c;
    if (idx < 0 || idx > 40) return;
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

/* 17 botones: icono + tooltip */
static const int BT_ICONO[17] = {
    24, 25, 26, 27, 28, 29, 30, 31, 32, 39, 37, 38, 33, 34, 35, 36, 40
};
static const char *BT_TIP[17] = {
    "Nuevo (Ctrl+N)", "Abrir (Ctrl+O)", "Guardar (Ctrl+S)",
    "Guardar como (Ctrl+G)", "Deshacer (Ctrl+Z)", "Rehacer (Ctrl+Y)",
    "Cortar (Ctrl+X)", "Copiar (Ctrl+C)", "Pegar (Ctrl+V)",
    "Seleccionar todo (Ctrl+E)", "Buscar (Ctrl+F)",
    "Reemplazar (Ctrl+R)", "Negrita (Ctrl+B)", "Cursiva (Ctrl+I)",
    "Subrayado (Ctrl+U)", "Color de texto", "Cerrar documento (Ctrl+W)"
};

static int tb_filas(int w) { return w < 560 ? 2 : 1; }
static int tb_h(int w) { return tb_filas(w) * 34; }

static int btn_activo(int b, doc_t *d) {
    int a, e;
    if (!d || !hay_sel(d)) {
        if (b == 12) return (d && (d->pen_est & EST_B)) ? 1 : 0;
        if (b == 13) return (d && (d->pen_est & EST_I)) ? 1 : 0;
        if (b == 14) return (d && (d->pen_est & EST_S)) ? 1 : 0;
        return 0;
    }
    sel_rango(d, &a, &e);
    if (b == 12 || b == 13 || b == 14) {
        int bit = b == 12 ? EST_B : (b == 13 ? EST_I : EST_S);
        int i, todos = 1, ninguno = 1;
        for (i = a; i < e; i++) {
            if (d->est[i] & bit) ninguno = 0;
            else todos = 0;
        }
        if (todos) return 1;
        return 0;   /* mixto = neutro (apagado) */
    }
    return 0;
}

static int btn_off(int b, doc_t *d) {
    if (!d) return (b != 0 && b != 1) ? 1 : 0;
    if (b == 4) return d->u_n <= 0;
    if (b == 5) return d->r_n <= 0;
    if (b == 6 || b == 7) return !hay_sel(d);
    if (b == 8) return !clip_t || !clip_n;
    return 0;
}

/* geometria de boton b */
static void btn_geo(int b, int w, int *x, int *y) {
    int por_fila = w / 34;
    if (por_fila < 1) por_fila = 1;
    if (tb_filas(w) == 1) {
        *x = 4 + b * 34;
        *y = 26;
    } else {
        int mitad = 9;
        if (b < mitad) {
            *x = 4 + b * 34;
            *y = 26;
        } else {
            *x = 4 + (b - mitad) * 34;
            *y = 60;
        }
    }
}

static void dibuja_toolbar(ug_win_t *v, doc_t *d) {
    int b;
    int w = v->w;
    int f = tb_filas(w);
    (void)f;
    ug_relleno(v, 0, 24, w, tb_h(w), T_PANEL);
    ug_rect(v, 0, 24 + tb_h(w), w, 1, T_PANEL_BORDE);
    for (b = 0; b < 17; b++) {
        int x, y, hov, act, off;
        btn_geo(b, w, &x, &y);
        if (x + 30 > w - 2) continue;
        hov = (rat_lx >= x && rat_lx < x + 30 && rat_ly >= y &&
               rat_ly < y + 30);
        act = btn_activo(b, d);
        off = btn_off(b, d);
        if (off)
            ug_relleno(v, x, y, 30, 30, T_PANEL);
        else if (act)
            ug_relleno(v, x, y, 30, 30, T_SELECCION);
        else if (hov)
            ug_relleno(v, x, y, 30, 30, T_ACENTO);
        else
            ug_relleno(v, x, y, 30, 30, T_BTN_CARA);
        ug_rect(v, x, y, 30, 30, T_PANEL_BORDE);
        icono(BT_ICONO[b], x + 7, y + 7, v);
        if (hov) hover_btn = b;
    }
}

/* ---- dibujo de texto con estilos ---- */
static void dibuja_run(ug_win_t *v, int x, int y, const char *s, int n,
                       int rgb, int negrita, int cursiva, int subray,
                       int fondo_sel) {
    char tmp[260];
    int i, w;
    if (n <= 0) return;
    if (n > 250) n = 250;
    for (i = 0; i < n; i++) tmp[i] = s[i];
    tmp[n] = 0;
    w = n;   /* aprox: ug_texto recorta; el ancho real lo da celdas */
    (void)w;
    (void)fondo_sel;
    u_texto(0, 0, "", 0);   /* afirma lease (ya tomado) */
    ug_texto(v, x, y, tmp, rgb);
    if (negrita) {
        /* segunda pasada 1 px a la derecha */
        ug_texto(v, x + 1, y, tmp, rgb);
    }
    if (cursiva) {
        /* shear: filas superiores 1 px a la derecha */
        int r, ww = 0;
        {
            int k = 0;
            while (tmp[k]) {
                int av = 0;
                ut_dec(tmp + k, 4, &av);
                if (av <= 0) av = 1;
                k += av;
                ww += 8;
            }
            if (negrita) ww += 1;
        }
        for (r = 0; r < 8; r++) {
            int yy = y + r, xx = x;
            int wth = ww > 1 ? ww - 1 : 0;
            int op[6];
            if (wth <= 0) continue;
            /* mueve la fila superior a la derecha y limpia el hueco */
            op[0] = v->ox + xx;
            op[1] = v->oy + yy;
            op[2] = wth;
            op[3] = 1;
            op[4] = v->ox + xx + 1;
            op[5] = v->oy + yy;
            u_blit(op[0], op[1], op[2], op[3], op[4], op[5]);
        }
    }
    if (subray) {
        int ww = 0, k = 0;
        while (tmp[k]) {
            int av = 0;
            ut_dec(tmp + k, 4, &av);
            if (av <= 0) av = 1;
            k += av;
            ww += 8;
        }
        if (negrita) ww += 1;
        ug_linea(v, x, y + 14, x + ww - 1, y + 14, rgb);
    }
}

/* nombre corto para pestaña */
static void nom_corto(const char *r, char *d, int max) {
    int l = u_strlen(r), i, ini = 0;
    if (!l) {
        const char *s = "sin nombre";
        int k = 0;
        while (s[k] && k < max - 1) { d[k] = s[k]; k++; }
        d[k] = 0;
        return;
    }
    for (i = 0; r[i]; i++)
        if (r[i] == '/') ini = i + 1;
    {
        int k = 0;
        while (r[ini + k] && k < max - 1) { d[k] = r[ini + k]; k++; }
        d[k] = 0;
    }
}

/* ---- layout ---- */
static int area_y0(ug_win_t *v) { return 24 + tb_h(v->w) + 2 + 24 + 2; }
static int tabs_h(void) { return 24; }
static int estado_y0(ug_win_t *v) { return (v->h - UG_CHROME) - 36; }
static int num_w(void) { return 42; }

/* filas visuales (con ajuste) */
static int vis_cols(ug_win_t *v) {
    int w = v->w - num_w() - 20;
    int c = w / UG_FW;
    return c < 8 ? 8 : c;
}

/* total de filas visuales del doc */
static int total_vis(doc_t *d, ug_win_t *v) {
    if (!ajuste) return nlineas(d);
    {
        int tot = 0, nl = nlineas(d), i, vc = vis_cols(v);
        for (i = 0; i < nl; i++) {
            int ini = lin_inicio(d, i), fin = lin_fin(d, ini);
            int c = cuenta_car(d->txt, ini, fin);
            int f = c / vc + 1;
            if (c == 0) f = 1;
            tot += f;
        }
        return tot < 1 ? 1 : tot;
    }
}

/* fila visual -> (ini,fin) bytes + es continuacion */
static int vis_fila(doc_t *d, ug_win_t *v, int vf, int *ini, int *fin) {
    if (!ajuste) {
        int nl = nlineas(d);
        if (vf < 0) vf = 0;
        if (vf >= nl) vf = nl - 1;
        *ini = lin_inicio(d, vf);
        *fin = lin_fin(d, *ini);
        return vf;
    }
    {
        int nl = nlineas(d), i, acc = 0, vc = vis_cols(v);
        for (i = 0; i < nl; i++) {
            int li = lin_inicio(d, i), lf = lin_fin(d, li);
            int c = cuenta_car(d->txt, li, lf);
            int f = c / vc + 1;
            int k;
            if (c == 0) f = 1;
            for (k = 0; k < f; k++) {
                if (acc == vf) {
                    int b0 = byte_celda(d, li, lf, k * vc);
                    int b1 = byte_celda(d, li, lf, (k + 1) * vc);
                    *ini = b0;
                    *fin = b1;
                    return i;
                }
                acc++;
            }
        }
        *ini = d->len;
        *fin = d->len;
        return nl - 1;
    }
}

/* fila visual del cursor */
static int vis_de_pos(doc_t *d, ug_win_t *v, int pos) {
    int nl = lin_de_pos(d, pos);
    if (!ajuste) return nl;
    {
        int vc = vis_cols(v), i, acc = 0;
        for (i = 0; i < nl; i++) {
            int li = lin_inicio(d, i), lf = lin_fin(d, li);
            int c = cuenta_car(d->txt, li, lf);
            int f = c / vc + 1;
            if (c == 0) f = 1;
            acc += f;
        }
        {
            int li = lin_inicio(d, nl), cs = celdas(d, li, pos);
            acc += cs / vc;
        }
        return acc;
    }
}

/* asegura cursor visible */
static void asegura_vis(doc_t *d, ug_win_t *v, int filas) {
    int vf = vis_de_pos(d, v, d->cur);
    int cs;
    if (vf < d->slin) d->slin = vf;
    if (vf >= d->slin + filas) d->slin = vf - filas + 1;
    if (d->slin < 0) d->slin = 0;
    if (!ajuste) {
        int li = lin_inicio(d, lin_de_pos(d, d->cur));
        cs = celdas(d, li, d->cur);
        if (cs < d->scol) d->scol = cs;
        {
            int vc = vis_cols(v);
            if (cs >= d->scol + vc) d->scol = cs - vc + 1;
        }
        if (d->scol < 0) d->scol = 0;
    } else {
        d->scol = 0;
    }
}

/* ---- acciones ---- */
static void acc_nuevo(void) {
    if (doc_nuevo() < 0) return;
    dlg = 0;
    pon_msg("documento nuevo");
}

static void acc_abrir_lista(doc_t *d, const char *ruta) {
    (void)d;
    (void)ruta;
}

static void acc_guardar(doc_t *d) {
    if (!d) return;
    if (!d->ruta[0]) {
        dlg = 2;
        dlg_accion = 0;
        dlg_browser_ini(d->ruta);
        return;
    }
    doc_guarda(d, d->ruta);
}

static void acc_todo(doc_t *d) {
    if (!d) return;
    d->anc = 0;
    d->cur = d->len;
}

static void acc_formato(doc_t *d, int bit) {
    int a, b, i, todos;
    if (!d) return;
    if (!hay_sel(d)) {
        d->pen_est ^= bit;
        return;
    }
    sel_rango(d, &a, &b);
    todos = 1;
    for (i = a; i < b; i++) {
        if (!(d->est[i] & bit)) { todos = 0; break; }
    }
    doc_estilo(d, a, b, bit, todos ? -1 : 1, -1, 1);
}

static void acc_color(doc_t *d, int coln) {
    int a, b;
    if (!d) return;
    if (coln < 0 || coln > 9) return;
    d->pen_col = coln;
    if (!hay_sel(d)) return;
    sel_rango(d, &a, &b);
    doc_estilo(d, a, b, 0, 0, coln, 1);
}

/* borra palabra (Ctrl+Retroceso/Supr): devuelve 1 si borro */
static int borra_palabra(doc_t *d, int hacia_delante) {
    int p = d->cur, q;
    if (hay_sel(d)) {
        int a, b;
        sel_rango(d, &a, &b);
        doc_del(d, a, b - a, 1);
        d->cur = d->anc = a;
        return 1;
    }
    if (!hacia_delante) {
        int li;
        if (p <= 0) return 0;
        /* salta espacios */
        q = p;
        while (q > 0) {
            int a = ant_c(d->txt, q), av = 0;
            unsigned cp = ut_dec(d->txt + a, q - a > 4 ? 4 : q - a, &av);
            if (!es_palabra(cp, d->txt + a, av) && cp != ' ' &&
                cp != '\t')
                break;
            if (cp != ' ' && cp != '\t') break;
            q = a;
            if (cp != ' ' && cp != '\t') break;
        }
        /* salta palabra */
        li = lin_inicio(d, lin_de_pos(d, p));
        if (q == p) {
            /* sin espacios: borra hasta inicio de palabra */
            while (q > li) {
                int a = ant_c(d->txt, q), av = 0;
                unsigned cp =
                    ut_dec(d->txt + a, q - a > 4 ? 4 : q - a, &av);
                if (!es_palabra(cp, d->txt + a, av)) break;
                q = a;
            }
            if (q == p && p > 0) q = ant_c(d->txt, p);
        } else {
            /* habia espacios: borra solo espacios */
        }
        if (q < p) {
            doc_del(d, q, p - q, 1);
            d->cur = d->anc = q;
            return 1;
        }
        return 0;
    }
    /* hacia delante */
    {
        int lf = lin_fin(d, lin_inicio(d, lin_de_pos(d, p)));
        q = p;
        if (q < d->len && d->txt[q] == '\n') {
            doc_del(d, q, 1, 1);
            return 1;
        }
        while (q < lf) {
            int av = 0;
            unsigned cp =
                ut_dec(d->txt + q, d->len - q > 4 ? 4 : d->len - q, &av);
            if (av <= 0) av = 1;
            if (es_palabra(cp, d->txt + q, av)) break;
            if (cp != ' ' && cp != '\t') break;
            q += av;
        }
        while (q < lf) {
            int av = 0;
            unsigned cp =
                ut_dec(d->txt + q, d->len - q > 4 ? 4 : d->len - q, &av);
            if (av <= 0) av = 1;
            if (!es_palabra(cp, d->txt + q, av)) break;
            q += av;
        }
        if (q == p) q = sig_c(d->txt, p, d->len);
        if (q > p) {
            doc_del(d, p, q - p, 1);
            d->cur = d->anc = p;
            return 1;
        }
    }
    return 0;
}

/* salto de palabra para Ctrl+flechas */
static int salto_pal(doc_t *d, int p, int dir) {
    int li, lf;
    if (dir < 0) {
        li = lin_inicio(d, lin_de_pos(d, p));
        if (p <= li) return p > 0 ? p - 1 : 0;
        /* salta separadores */
        while (p > li) {
            int a = ant_c(d->txt, p), av = 0;
            unsigned cp = ut_dec(d->txt + a, p - a > 4 ? 4 : p - a, &av);
            if (es_palabra(cp, d->txt + a, av)) break;
            p = a;
        }
        while (p > li) {
            int a = ant_c(d->txt, p), av = 0;
            unsigned cp = ut_dec(d->txt + a, p - a > 4 ? 4 : p - a, &av);
            if (!es_palabra(cp, d->txt + a, av)) break;
            p = a;
        }
        return p;
    }
    lf = lin_fin(d, lin_inicio(d, lin_de_pos(d, p)));
    if (p >= d->len) return d->len;
    if (p < d->len && d->txt[p] == '\n') return p + 1;
    while (p < lf) {
        int av = 0;
        unsigned cp = ut_dec(d->txt + p, d->len - p > 4 ? 4 : d->len - p,
                             &av);
        if (av <= 0) av = 1;
        if (es_palabra(cp, d->txt + p, av)) break;
        p += av;
    }
    while (p < lf) {
        int av = 0;
        unsigned cp = ut_dec(d->txt + p, d->len - p > 4 ? 4 : d->len - p,
                             &av);
        if (av <= 0) av = 1;
        if (!es_palabra(cp, d->txt + p, av)) break;
        p += av;
    }
    return p;
}

/* ---- dibujo principal ---- */
static void dibuja(ug_win_t *v) {
    doc_t *d = DA();
    int y0 = area_y0(v), y1 = estado_y0(v);
    int filas = (y1 - y0) / UG_FH;
    int i, b;
    if (filas < 1) filas = 1;
    ug_comienza(v);
    ug_limpia(v, tema_claro ? RGB24(235, 238, 242) : T_VENT_FONDO);
    /* pestañas */
    {
        int x = 2, k;
        ug_relleno(v, 0, 0, v->w, tabs_h(), T_PANEL);
        for (k = 0; k < MAXDOC; k++) {
            char nm[20], et[24];
            int t = 0, q = 0, tw;
            if (!docs[k].uso) continue;
            nom_corto(docs[k].ruta, nm, 14);
            while (nm[q] && t < 20) et[t++] = nm[q++];
            if (docs[k].sucio && t < 22) et[t++] = '*';
            et[t] = 0;
            tw = u_strlen(et) * UG_FW + 12;
            if (x + tw > v->w - 2) break;
            if (k == dact)
                ug_relleno(v, x, 2, tw, 20, T_SELECCION);
            else
                ug_relleno(v, x, 2, tw, 20, T_BTN_CARA);
            ug_rect(v, x, 2, tw, 20, T_PANEL_BORDE);
            ug_texto(v, x + 6, 5, et,
                     k == dact ? T_PANEL_TEXTO : C_GRIS);
            x += tw + 2;
        }
    }
    dibuja_toolbar(v, d);
    /* area */
    {
        int ax = 2, aw = v->w - 4;
        int ay = y0, ah = y1 - y0;
        int bgp = tema_claro ? RGB24(255, 255, 255) : T_VENT_FONDO;
        int bgnum = tema_claro ? RGB24(225, 228, 233) : T_PANEL;
        ug_relleno(v, ax, ay, aw, ah, bgnum);
        ug_relleno(v, ax + num_w(), ay, aw - num_w() - 12, ah, bgp);
        ug_rect(v, ax, ay, aw, ah, T_PANEL_BORDE);
        if (!d) {
            ug_texto(v, ax + num_w() + 8, ay + 8,
                     "Ctrl+N nuevo, Ctrl+O abrir",
                     tema_claro ? C_NEGRO : C_GRIS);
        } else {
            int vf;
            int tot = total_vis(d, v);
            if (d->slin > tot - 1) d->slin = tot - 1;
            if (d->slin < 0) d->slin = 0;
            asegura_vis(d, v, filas);
            for (i = 0; i < filas; i++) {
                int yy = ay + 1 + i * UG_FH;
                int ini, fin, lig;
                int sa = -1, sb = -1;
                vf = d->slin + i;
                if (vf >= tot) break;
                lig = vis_fila(d, v, vf, &ini, &fin);
                /* numero de linea logica */
                ug_texto(v, ax + 4, yy, nb_lin(lig + 1),
                         tema_claro ? C_NEGRO : C_GRIS);
                /* seleccion en esta fila */
                if (hay_sel(d)) {
                    int a, e;
                    sel_rango(d, &a, &e);
                    if (e > ini && a < fin) {
                        sa = a < ini ? ini : a;
                        sb = e > fin ? fin : e;
                    }
                }
                /* busqueda: resalta */
                {
                    int h;
                    for (h = 0; h < bus_nh; h++) {
                        int f = hit_fin(d, h);
                        if (f < 0) continue;
                        if (f > ini && bus_pos[h] < fin) {
                            int hb0 = bus_pos[h] < ini ? ini : bus_pos[h];
                            int hb1 = f > fin ? fin : f;
                            int c0 = celdas(d, ini, hb0);
                            int c1 = celdas(d, ini, hb1);
                            int xx0 = ax + num_w() + 8 +
                                      (c0 - d->scol) * UG_FW;
                            ug_relleno(v, xx0, yy, (c1 - c0) * UG_FW,
                                       UG_FH, RGB24(120, 90, 20));
                        }
                    }
                }
                if (sa >= 0) {
                    int c0 = celdas(d, ini, sa);
                    int c1 = celdas(d, ini, sb);
                    int xx0 = ax + num_w() + 8 + (c0 - d->scol) * UG_FW;
                    ug_relleno(v, xx0, yy, (c1 - c0) * UG_FW, UG_FH,
                               T_SELECCION);
                }
                /* runs por color/estilo (recorta a scol) */
                {
                    int p = ini;
                    while (p < fin) {
                        int q = p + 1, cc;
                        while (q < fin && d->est[q] == d->est[p] &&
                               d->col[q] == d->col[p])
                            q++;
                        /* celda inicial */
                        cc = celdas(d, ini, p) - d->scol;
                        /* salta lo oculto a la izquierda */
                        {
                            int c0 = celdas(d, ini, p);
                            int c1 = celdas(d, ini, q);
                            int vs0 = c0 < d->scol ? d->scol : c0;
                            int vs1 = c1;
                            int vc = vis_cols(v);
                            if (vs1 > d->scol + vc)
                                vs1 = d->scol + vc;
                            if (vs1 > vs0) {
                                int b0 = byte_celda(d, ini, fin, vs0);
                                int nb = 0, pp = b0;
                                while (pp < fin &&
                                       celdas(d, ini, pp) < vs1) {
                                    int av = 0;
                                    ut_dec(d->txt + pp,
                                           fin - pp > 4 ? 4 : fin - pp,
                                           &av);
                                    if (av <= 0) av = 1;
                                    pp += av;
                                    nb += av;
                                }
                                {
                                    char seg[250];
                                    int sn = pp - b0;
                                    int k2;
                                    if (sn > 240) sn = 240;
                                    for (k2 = 0; k2 < sn; k2++)
                                        seg[k2] = d->txt[b0 + k2];
                                    {
                                        int xx =
                                            ax + num_w() + 8 +
                                            (vs0 - d->scol) * UG_FW;
                                        int rgb = color_rgb(
                                            d->col[p], tema_claro, pers_r,
                                            pers_g, pers_b);
                                        dibuja_run(
                                            v, xx, yy, seg, sn, rgb,
                                            (d->est[p] & EST_B) ? 1 : 0,
                                            (d->est[p] & EST_I) ? 1 : 0,
                                            (d->est[p] & EST_S) ? 1 : 0,
                                            sa >= 0);
                                    }
                                }
                            }
                        }
                        p = q;
                        (void)cc;
                    }
                }
                /* cursor */
                if (v->foco && vf == vis_de_pos(d, v, d->cur)) {
                    int li2 = lin_inicio(d, lig);
                    int cc = celdas(d, li2, d->cur);
                    int cx;
                    if (ajuste) {
                        int li3 = lin_inicio(d, lig);
                        int lf3 = lin_fin(d, li3);
                        int vc = vis_cols(v);
                        int rel = celdas(d, li3, d->cur);
                        int ini_c = 0, kk = 0;
                        /* inicio visual de esta fila */
                        {
                            int acc2 = 0, ii;
                            for (ii = 0; ii < lig; ii++) {
                            }
                            (void)acc2;
                            (void)ii;
                        }
                        ini_c = (rel / vc) * vc;
                        cx = ax + num_w() + 8 + (rel - ini_c) * UG_FW;
                        (void)li3;
                        (void)lf3;
                        (void)ini_c;
                        (void)kk;
                    } else {
                        cx = ax + num_w() + 8 +
                             (cc - d->scol) * UG_FW;
                    }
                    ug_relleno(v, cx, yy, 2, UG_FH, C_BLANCO);
                }
            }
            /* barra de desplazamiento */
            {
                int sx = ax + aw - 12;
                int sh = ah - 2;
                int tot2 = tot;
                int pos = 0, tam = sh;
                if (tot2 > filas && tot2 > 0) {
                    tam = sh * filas / tot2;
                    if (tam < 12) tam = 12;
                    pos = sh * d->slin / tot2;
                    if (pos + tam > sh) pos = sh - tam;
                }
                ug_relleno(v, sx, ay + 1, 12, sh, T_PANEL);
                ug_relleno(v, sx + 2, ay + 1 + pos, 8, tam, T_ACENTO);
            }
        }
    }
    /* estado: linea/col + stats + formato pendiente + tooltip */
    {
        int sy = estado_y0(v);
        char l1[120], l2[120];
        int t = 0;
        ug_relleno(v, 0, sy, v->w, 36, T_PANEL);
        ug_rect(v, 0, sy, v->w, 1, T_PANEL_BORDE);
        if (d) {
            int nl = lin_de_pos(d, d->cur);
            int li = lin_inicio(d, nl);
            int cc = celdas(d, li, d->cur) + 1;
            int ch = 0, wd = 0, k2 = 0, enp = 0;
            ch = ut_largo_n(d->txt, d->len);
            /* palabras */
            {
                int p2 = 0;
                while (p2 < d->len) {
                    int av = 0;
                    unsigned cp = ut_dec(d->txt + p2,
                                         d->len - p2 > 4 ? 4
                                                         : d->len - p2,
                                         &av);
                    if (av <= 0) av = 1;
                    if (es_palabra(cp, d->txt + p2, av)) {
                        if (!enp) {
                            enp = 1;
                            wd++;
                        }
                    } else {
                        enp = 0;
                    }
                    p2 += av;
                }
            }
            t = 0;
            copia(l1, &t, "lin ");
            num(l1, &t, nl + 1);
            copia(l1, &t, " col ");
            num(l1, &t, cc);
            copia(l1, &t, "  ");
            num(l1, &t, ch);
            copia(l1, &t, " car ");
            num(l1, &t, wd);
            copia(l1, &t, " pal ");
            num(l1, &t, nlineas(d));
            copia(l1, &t, " lin ");
            num(l1, &t, d->len);
            copia(l1, &t, "B");
            l1[t] = 0;
            ug_texto(v, 4, sy + 2, l1, T_PANEL_TEXTO);
            t = 0;
            /* formato pendiente */
            if (d->pen_est & EST_B) copia(l2, &t, "[N]");
            if (d->pen_est & EST_I) copia(l2, &t, "[K]");
            if (d->pen_est & EST_S) copia(l2, &t, "[S]");
            copia(l2, &t, "color:");
            copia(l2, &t, color_nom(d->pen_col));
            copia(l2, &t, "  ");
            copia(l2, &t, tema_claro ? "claro" : "oscuro");
            copia(l2, &t, "  ");
            copia(l2, &t, ajuste ? "ajuste:si" : "ajuste:no");
            if (bus_q[0]) {
                char nb[12];
                copia(l2, &t, "  bus:");
                u_itoa(bus_nh, nb);
                {
                    int q = 0;
                    while (nb[q] && t < 110) l2[t++] = nb[q++];
                }
            }
            if (msg[0]) {
                copia(l2, &t, "  ");
                {
                    int q = 0;
                    while (msg[q] && t < 115) l2[t++] = msg[q++];
                }
            } else if (hover_btn >= 0 && hover_btn < 17) {
                copia(l2, &t, "  ");
                copia(l2, &t, BT_TIP[hover_btn]);
            }
            l2[t] = 0;
            ug_texto(v, 4, sy + 19, l2, C_GRIS);
            /* muestra color pendiente */
            ug_relleno(v, v->w - 30, sy + 19, 12, 12,
                       color_rgb(d->pen_col, tema_claro, pers_r, pers_g,
                                 pers_b));
            ug_rect(v, v->w - 30, sy + 19, 12, 12, C_BLANCO);
            (void)k2;
        } else {
            ug_texto(v, 4, sy + 2, "sin documento", C_GRIS);
            if (msg[0]) ug_texto(v, 4, sy + 19, msg, C_ROJO_CLARO);
            else if (hover_btn >= 0 && hover_btn < 17)
                ug_texto(v, 4, sy + 19, BT_TIP[hover_btn], C_GRIS);
        }
    }
    /* tooltip flotante */
    if (hover_btn >= 0 && hover_btn < 17 && rat_lx >= 0) {
        const char *s = BT_TIP[hover_btn];
        int tw = u_strlen(s) * UG_FW + 8;
        int tx = rat_lx + 12, ty = rat_ly + 18;
        if (tx + tw > v->w - 2) tx = v->w - 2 - tw;
        if (ty + 20 > v->h - UG_CHROME - 2) ty = rat_ly - 22;
        if (tw > 0 && tx >= 0 && ty >= 24) {
            ug_relleno(v, tx, ty, tw, 18, RGB24(20, 20, 20));
            ug_rect(v, tx, ty, tw, 18, C_AMARILLO);
            ug_texto(v, tx + 4, ty + 2, s, C_BLANCO);
        }
    }
    /* panel buscar/reemplazar (si hay consulta o visible) */
    if (dlg == 0 && (bus_q[0] || bus_panel)) {
        dibuja_bus(v);
    }
    /* dialogos */
    if (dlg == 1) dibuja_dlg_arch(v, "Abrir archivo:");
    else if (dlg == 2)
        dibuja_dlg_arch(v, "Guardar como (.f32r=formato,.txt=texto):");
    else if (dlg == 3)
        dibuja_dlg_conf(v, "Cerrar con cambios",
                        "Guardar cambios antes de cerrar?");
    else if (dlg == 4)
        dibuja_dlg_conf(v, "Sobrescribir",
                        "El archivo existe. Sobrescribir?");
    else if (dlg == 5)
        dibuja_dlg_msg(v);
    else if (dlg == 6)
        dibuja_dlg_color(v);
    else if (dlg == 7)
        dibuja_dlg_ir(v);
    ug_fin(v);
    (void)b;
    (void)i;
}

/* helpers de texto del estado (evitan sprintf) */
static void copia(char *d, int *t, const char *s) {
    while (*s && *t < 118) d[(*t)++] = *s++;
}
static void num(char *d, int *t, int v) {
    char b[12];
    int q = 0;
    u_itoa(v, b);
    while (b[q] && *t < 116) d[(*t)++] = b[q++];
}
static int ut_largo_n(const char *s, int n) {
    int c = 0, p = 0;
    while (p < n) {
        int av = 0;
        ut_dec(s + p, n - p > 4 ? 4 : n - p, &av);
        if (av <= 0) av = 1;
        p += av;
        c++;
    }
    return c;
}
static const char *nb_lin(int n) {
    static char b[4][8];
    static int q = 0;
    char *d = b[q = (q + 1) % 4];
    u_itoa(n, d);
    return d;
}

/* ---- panel buscar / reemplazar ---- */
static void dibuja_bus(ug_win_t *v) {
    int yb = estado_y0(v) - 62;
    int w = v->w;
    if (yb < 120) yb = 120;
    ug_panel(v, 8, yb, w - 16, 60, T_PANEL_BORDE, T_PANEL);
    ug_texto(v, 16, yb + 4, "Buscar:", C_AMARILLO);
    ug_campo(v, 80, yb + 2, 180, bus_q, bus_cpos, bus_pos_campo == 0);
    ug_texto(v, 270, yb + 4, "por:", C_AMARILLO);
    ug_campo(v, 310, yb + 2, 150, bus_r, bus_rpos, bus_pos_campo == 1);
    ug_boton(v, 470, yb + 2, 70, 20, "Ant", 0);
    ug_boton(v, 544, yb + 2, 70, 20, "Sig", 0);
    {
        char nb[10];
        int t = 0, q = 0;
        u_itoa(bus_nh, nb);
        while (nb[q] && t < 8) { nb[t] = nb[q]; t++; q++; }
        nb[t] = 0;
        ug_texto(v, 16, yb + 24, nb, C_BLANCO);
    }
    ug_texto(v, 60, yb + 24, bus_may ? "[May:si]" : "[May:no]", C_GRIS);
    ug_texto(v, 150, yb + 24, bus_pal ? "[Pal:si]" : "[Pal:no]", C_GRIS);
    ug_boton(v, 250, yb + 24, 90, 20, "Cambiar", 0);
    ug_boton(v, 344, yb + 24, 90, 20, "Todas", 0);
    ug_boton(v, 438, yb + 24, 90, 20, "Opciones", 0);
    ug_boton(v, 532, yb + 24, 70, 20, "X", 0);
}

/* ---- dialogos ---- */
static void marco_dlg(ug_win_t *v, const char *tit, int h) {
    int w = v->w - 120;
    int x = 60, y = (v->h - UG_CHROME) / 2 - h / 2;
    if (w < 200) { w = v->w - 16; x = 8; }
    if (y < 60) y = 60;
    ug_panel(v, x, y, w, h, T_PANEL_BORDE, T_PANEL);
    ug_texto(v, x + 8, y + 6, tit, C_AMARILLO);
}

/* Absoluta del directorio mostrado por el selector. */
static void dlg_abs(char *d) {
    int t = 0, i;
    d[t++] = '/';
    for (i = 0; i < dlg_prof; i++) {
        int j = 0;
        if (t > 1) d[t++] = '/';
        while (dlg_pila[i][j] && t < 120) d[t++] = dlg_pila[i][j++];
    }
    d[t] = 0;
}

/* Hijo de dir con ese nombre (0 ok). */
static int dlg_hijo(int dir, const char *nom, int *tipo, int *idx) {
    int n = 0;
    udir_t e;
    while (u_dir_hijo(dir, n, &e) >= 0) {
        if (!u_strcmp(e.nombre, nom)) {
            if (tipo) *tipo = e.tipo;
            if (idx) *idx = e.idx;
            return 0;
        }
        n++;
        if (n > 300) break;
    }
    return -1;
}

/* Rellena la lista visible (carpetas primero, luego nombre). */
static void dlg_recarga(void) {
    int n = 0, i, j;
    udir_t e;
    dlg_nn = 0;
    while (dlg_nn < 64 && u_dir_hijo(dlg_dir, n, &e) >= 0) {
        if (e.tipo != 3) {
            int q = 0;
            while (e.nombre[q] && q < 39) {
                dlg_noms[dlg_nn][q] = e.nombre[q];
                q++;
            }
            dlg_noms[dlg_nn][q] = 0;
            dlg_tipos[dlg_nn] = e.tipo;
            dlg_idx[dlg_nn] = e.idx;
            dlg_nn++;
        }
        n++;
        if (n > 300) break;
    }
    for (i = 0; i < dlg_nn; i++) {
        for (j = i + 1; j < dlg_nn; j++) {
            int sw = 0;
            if (dlg_tipos[j] < dlg_tipos[i]) sw = 1;
            else if (dlg_tipos[j] == dlg_tipos[i] &&
                     u_strcmp(dlg_noms[j], dlg_noms[i]) < 0)
                sw = 1;
            if (sw) {
                char tn[40];
                int tt, ti;
                u_strcpy(tn, dlg_noms[i]);
                u_strcpy(dlg_noms[i], dlg_noms[j]);
                u_strcpy(dlg_noms[j], tn);
                tt = dlg_tipos[i];
                dlg_tipos[i] = dlg_tipos[j];
                dlg_tipos[j] = tt;
                ti = dlg_idx[i];
                dlg_idx[i] = dlg_idx[j];
                dlg_idx[j] = ti;
            }
        }
    }
    if (dlg_sel >= dlg_nn) dlg_sel = dlg_nn ? dlg_nn - 1 : 0;
    if (dlg_sel < 0) dlg_sel = 0;
    if (dlg_scroll > dlg_sel) dlg_scroll = dlg_sel;
    if (dlg_scroll < 0) dlg_scroll = 0;
}

/* Entrar en la carpeta seleccionada (0 ok). */
static int dlg_entra_sel(void) {
    int i;
    if (dlg_sel < 0 || dlg_sel >= dlg_nn) return -1;
    if (dlg_tipos[dlg_sel] != 1) return -1;
    if (dlg_prof >= 8) {
        u_strcpy(msg, "demasiada profundidad");
        return -1;
    }
    i = 0;
    while (dlg_noms[dlg_sel][i] && i < 31) {
        dlg_pila[dlg_prof][i] = dlg_noms[dlg_sel][i];
        i++;
    }
    dlg_pila[dlg_prof][i] = 0;
    dlg_pidx[dlg_prof] = dlg_idx[dlg_sel];
    dlg_prof++;
    dlg_dir = dlg_idx[dlg_sel];
    dlg_sel = 0;
    dlg_scroll = 0;
    dlg_ult_id = -1;
    dlg_recarga();
    return 0;
}

static void dlg_subir(void) {
    if (dlg_prof <= 0) return;
    dlg_prof--;
    dlg_dir = dlg_prof ? dlg_pidx[dlg_prof - 1] : 0;
    dlg_sel = 0;
    dlg_scroll = 0;
    dlg_ult_id = -1;
    dlg_recarga();
}

/* Prepara el selector: directorio de ruta (o raiz) y campo = nombre. */
static void dlg_browser_ini(const char *ruta) {
    char comps[9][32];
    int nc = 0, i = 0;
    dlg_prof = 0;
    dlg_dir = 0;
    dlg_campo[0] = 0;
    dlg_pos = 0;
    dlg_sel = 0;
    dlg_scroll = 0;
    dlg_ult_id = -1;
    if (ruta && ruta[0]) {
        i = ruta[0] == '/' ? 1 : 0;
        while (ruta[i] && nc < 9) {
            int q = 0;
            while (ruta[i] == '/') i++;
            if (!ruta[i]) break;
            while (ruta[i] && ruta[i] != '/' && q < 31)
                comps[nc][q++] = ruta[i++];
            comps[nc][q] = 0;
            if (q) nc++;
        }
        {
            int cur = 0, c;
            for (c = 0; c < nc; c++) {
                int tipo = 0, idx = -1, ultimo = (c == nc - 1);
                if (dlg_hijo(cur, comps[c], &tipo, &idx)) {
                    /* no existe: el resto es nombre nuevo */
                    int q = 0;
                    while (comps[c][q] && q < NNOM - 1) {
                        dlg_campo[q] = comps[c][q];
                        q++;
                    }
                    dlg_campo[q] = 0;
                    dlg_pos = q;
                    break;
                }
                if (tipo != 1) {
                    /* es archivo: campo = nombre, dir = actual */
                    int q = 0;
                    while (comps[c][q] && q < NNOM - 1) {
                        dlg_campo[q] = comps[c][q];
                        q++;
                    }
                    dlg_campo[q] = 0;
                    dlg_pos = q;
                    break;
                }
                if (dlg_prof >= 8) break;
                {
                    int q = 0;
                    while (comps[c][q] && q < 31) {
                        dlg_pila[dlg_prof][q] = comps[c][q];
                        q++;
                    }
                    dlg_pila[dlg_prof][q] = 0;
                    dlg_pidx[dlg_prof] = idx;
                    dlg_prof++;
                }
                cur = idx;
                if (ultimo) break;   /* era carpeta: campo vacio */
            }
            dlg_dir = cur;
        }
    }
    dlg_recarga();
    /* preselecciona el archivo del campo si existe en la lista */
    if (dlg_campo[0]) {
        int k;
        for (k = 0; k < dlg_nn; k++) {
            if (dlg_tipos[k] == 2 &&
                !u_strcmp(dlg_noms[k], dlg_campo)) {
                dlg_sel = k;
                break;
            }
        }
    }
}

/* Ruta completa: dir mostrado + campo (o el campo tal cual si trae /). */
static void dlg_ruta_full(char *out) {
    int i = 0, l;
    while (dlg_campo[i]) {
        if (dlg_campo[i] == '/') {
            u_strcpy(out, dlg_campo);
            return;
        }
        i++;
    }
    if (!dlg_campo[0]) {
        out[0] = 0;
        return;
    }
    dlg_abs(out);
    l = u_strlen(out);
    if (l > 1) out[l++] = '/';
    i = 0;
    while (dlg_campo[i] && l < NNOM - 1) out[l++] = dlg_campo[i++];
    out[l] = 0;
}

static void dibuja_dlg_arch(ug_win_t *v, const char *tit) {
    int w = v->w - 80, x = 40;
    int H = 236, y = (v->h - UG_CHROME) / 2 - H / 2;
    char ruta[128];
    int i, filas = 6, ay;
    if (w < 240) { w = v->w - 16; x = 8; }
    if (w > 560) { w = 560; x = (v->w - w) / 2; }
    if (y < 56) y = 56;
    ug_panel(v, x, y, w, H, T_PANEL_BORDE, T_PANEL);
    ug_texto(v, x + 8, y + 6, tit, C_AMARILLO);
    dlg_abs(ruta);
    /* barra de direccion + subir */
    ug_rect(v, x + 8, y + 24, w - 86, 18, T_PANEL_BORDE);
    ug_texto(v, x + 12, y + 27, ruta, T_PANEL_TEXTO);
    ug_boton(v, x + w - 70, y + 24, 62, 18, "Subir", 0);
    /* lista */
    ay = y + 48;
    ug_rect(v, x + 8, ay - 2, w - 16, filas * 16 + 4, T_PANEL_BORDE);
    if (!dlg_nn)
        ug_texto(v, x + 14, ay + 2, "(carpeta vacia)", C_GRIS_OSCURO);
    for (i = 0; i < filas && dlg_scroll + i < dlg_nn; i++) {
        int id = dlg_scroll + i, yy = ay + i * 16;
        char nm[34];
        int q = 0;
        if (id == dlg_sel)
            ug_relleno(v, x + 9, yy, w - 18, 16, T_SELECCION);
        icono(dlg_tipos[id] == 1 ? 6 : 7, x + 12, yy, v);
        while (dlg_noms[id][q] && q < 30) { nm[q] = dlg_noms[id][q]; q++; }
        nm[q] = 0;
        ug_texto(v, x + 32, yy + 1, nm,
                 id == dlg_sel ? C_BLANCO
                               : (dlg_tipos[id] == 1 ? C_AMARILLO : C_GRIS));
    }
    /* campo nombre + pista */
    ug_texto(v, x + 8, y + 150, "Nombre:", C_GRIS);
    ug_campo(v, x + 70, y + 148, w - 78, dlg_campo, dlg_pos, 1);
    ug_texto(v, x + 8, y + 172, "doble clic o Enter abre la carpeta",
             C_GRIS_OSCURO);
    ug_boton(v, x + 8, y + 196, 100, 22, "Aceptar", 0);
    ug_boton(v, x + 116, y + 196, 100, 22, "Nada", 0);
}

static void dibuja_dlg_conf(ug_win_t *v, const char *tit, const char *pre) {
    int w = v->w - 160, x = 80;
    int y = (v->h - UG_CHROME) / 2 - 44;
    if (w < 200) { w = v->w - 16; x = 8; }
    if (y < 60) y = 60;
    marco_dlg(v, tit, 88);
    ug_texto(v, x + 8, y + 26, pre, C_BLANCO);
    ug_boton(v, x + 8, y + 54, 90, 22, "Guardar", 0);
    ug_boton(v, x + 104, y + 54, 90, 22, "Descartar", 0);
    ug_boton(v, x + 200, y + 54, 90, 22, "Nada", 0);
}

static void dibuja_dlg_msg(ug_win_t *v) {
    int w = v->w - 160, x = 80;
    int y = (v->h - UG_CHROME) / 2 - 40;
    if (w < 200) { w = v->w - 16; x = 8; }
    if (y < 60) y = 60;
    marco_dlg(v, "Aviso", 80);
    ug_texto(v, x + 8, y + 26, dlg_msg, C_BLANCO);
    ug_boton(v, x + 8, y + 50, 100, 22, "Aceptar", 0);
}

static void dibuja_dlg_color(ug_win_t *v) {
    int w = 340, x = (v->w - w) / 2;
    int y = (v->h - UG_CHROME) / 2 - 80;
    int i;
    if (x < 8) x = 8;
    if (y < 60) y = 60;
    ug_panel(v, x, y, w, 160, T_PANEL_BORDE, T_PANEL);
    ug_texto(v, x + 8, y + 6, "Color de texto", C_AMARILLO);
    for (i = 0; i < 10; i++) {
        int cx = x + 8 + (i % 5) * 64, cy = y + 26 + (i / 5) * 30;
        ug_relleno(v, cx, cy, 56, 22,
                   color_rgb(i, tema_claro, dlg_cr, dlg_cg, dlg_cb));
        ug_rect(v, cx, cy, 56, 22,
                i == dlg_ccol ? C_AMARILLO : T_PANEL_BORDE);
        ug_texto(v, cx + 4, cy + 4, color_nom(i), C_BLANCO);
    }
    ug_texto(v, x + 8, y + 90, "propio R G B (0-255):", C_GRIS);
    {
        char nb[12];
        int q;
        u_itoa(dlg_cr, nb);
        ug_texto(v, x + 8, y + 106, "R:", C_GRIS);
        ug_campo(v, x + 28, y + 104, 60, nb, u_strlen(nb), 0);
        u_itoa(dlg_cg, nb);
        q = u_strlen(nb);
        (void)q;
        ug_texto(v, x + 100, y + 106, "G:", C_GRIS);
        ug_campo(v, x + 120, y + 104, 60, nb, u_strlen(nb), 0);
        u_itoa(dlg_cb, nb);
        ug_texto(v, x + 192, y + 106, "B:", C_GRIS);
        ug_campo(v, x + 212, y + 104, 60, nb, u_strlen(nb), 0);
    }
    ug_relleno(v, x + 284, y + 104, 40, 20,
               RGB24(dlg_cr, dlg_cg, dlg_cb));
    ug_rect(v, x + 284, y + 104, 40, 20, C_BLANCO);
    ug_boton(v, x + 8, y + 130, 100, 22, "Aplicar", 0);
    ug_boton(v, x + 116, y + 130, 100, 22, "Nada", 0);
}

static void dibuja_dlg_ir(ug_win_t *v) {
    int w = 260, x = (v->w - w) / 2;
    int y = (v->h - UG_CHROME) / 2 - 40;
    if (x < 8) x = 8;
    if (y < 60) y = 60;
    ug_panel(v, x, y, w, 80, T_PANEL_BORDE, T_PANEL);
    ug_texto(v, x + 8, y + 6, "Ir a la linea:", C_AMARILLO);
    ug_campo(v, x + 8, y + 26, w - 16, dlg_campo, dlg_pos, 1);
    ug_boton(v, x + 8, y + 50, 100, 22, "Ir", 0);
    ug_boton(v, x + 116, y + 50, 100, 22, "Nada", 0);
}

/* ---- cierre de documento / salida ---- */
static void cambia_pestana(int dir) {
    int k, n = 0, lista[MAXDOC], i;
    for (k = 0; k < MAXDOC; k++) {
        if (docs[k].uso) lista[n++] = k;
    }
    if (n < 2) return;
    for (i = 0; i < n; i++) {
        if (lista[i] == dact) {
            dact = lista[(i + dir + n) % n];
            bus_nh = 0;
            bus_sel = -1;
            return;
        }
    }
    dact = lista[0];
}

static void guarda_todos(void) {
    int k, ok = 0, mal = 0;
    for (k = 0; k < MAXDOC; k++) {
        if (docs[k].uso && docs[k].sucio) {
            if (!docs[k].ruta[0]) {
                mal++;
                continue;
            }
            if (doc_guarda(&docs[k], docs[k].ruta)) mal++;
            else ok++;
        }
    }
    {
        char b[48];
        int t = 0;
        copia(b, &t, "guardados: ");
        num(b, &t, ok);
        if (mal) {
            copia(b, &t, " (sin nombre: ");
            num(b, &t, mal);
            copia(b, &t, ")");
        }
        b[t] = 0;
        pon_msg(b);
    }
}

static int pide_cerrar_doc(void) {
    doc_t *d = DA();
    if (!d) return 1;
    if (!d->sucio) return 1;
    dlg = 3;
    dlg_accion = 1;   /* cerrar doc */
    return 0;
}

static void cierra_doc_ahora(void) {
    int i = dact, k, best = -1;
    if (i < 0 || !docs[i].uso) return;
    doc_cierra_mem(i);
    for (k = 0; k < MAXDOC; k++) {
        if (docs[k].uso) best = k;
    }
    dact = best;
    bus_nh = 0;
    bus_sel = -1;
}

static int todos_limpios(void) {
    int k;
    for (k = 0; k < MAXDOC; k++) {
        if (docs[k].uso && docs[k].sucio) return 0;
    }
    return 1;
}

/* ---- abrir / guardar desde dialogo ---- */
static void hace_abrir(void) {
    char ruta[NNOM];
    dlg_ruta_full(ruta);
    if (!ruta[0]) {
        /* sin nombre: si hay carpeta seleccionada se entra */
        if (dlg_sel >= 0 && dlg_sel < dlg_nn &&
            dlg_tipos[dlg_sel] == 1) {
            dlg_entra_sel();
            return;
        }
        /* sin nombre: si hay archivo seleccionado se abre */
        if (dlg_sel >= 0 && dlg_sel < dlg_nn &&
            dlg_tipos[dlg_sel] == 2) {
            char base[128];
            int l = 0, q = 0;
            dlg_abs(base);
            while (base[l]) l++;
            if (l > 1) base[l++] = '/';
            while (dlg_noms[dlg_sel][q] && l < NNOM - 1)
                base[l++] = dlg_noms[dlg_sel][q++];
            base[l] = 0;
            u_strcpy(ruta, base);
        } else {
            return;   /* nada que abrir */
        }
    }
    /* si es el doc actual sin cambios, recarga */
    if (doc_nuevo() < 0) { dlg = 0; return; }
    {
        doc_t *d = DA();
        int r = doc_carga(d, ruta);
        if (r == -1) {
            u_strcpy(dlg_msg, "no se abre (nombre o formato)");
            doc_cierra_mem(dact);
            {
                int k, best = -1;
                for (k = 0; k < MAXDOC; k++) {
                    if (docs[k].uso) best = k;
                }
                dact = best;
            }
            dlg = 5;
            return;
        }
        if (r == -2) {
            u_strcpy(dlg_msg, "f32r roto: texto recuperado");
            dlg = 5;
            return;
        }
        if (r == 1) pon_msg("abierto (truncado a 32KB)");
        else pon_msg("abierto");
        dlg = 0;
    }
}

static void hace_guardar_como(void) {
    doc_t *d = DA();
    char ruta[NNOM];
    if (!d) { dlg = 0; return; }
    dlg_ruta_full(ruta);
    if (!ruta[0]) {
        /* sin nombre: si hay carpeta seleccionada se entra */
        if (dlg_sel >= 0 && dlg_sel < dlg_nn &&
            dlg_tipos[dlg_sel] == 1) {
            dlg_entra_sel();
            return;
        }
        /* sin nombre: usa el archivo seleccionado si lo hay */
        if (dlg_sel >= 0 && dlg_sel < dlg_nn &&
            dlg_tipos[dlg_sel] == 2) {
            int q = 0;
            while (dlg_noms[dlg_sel][q] && q < NNOM - 1) {
                dlg_campo[q] = dlg_noms[dlg_sel][q];
                q++;
            }
            dlg_campo[q] = 0;
            dlg_pos = q;
            dlg_ruta_full(ruta);
        }
        if (!ruta[0]) return;
    }
    /* existe? confirma */
    {
        UFILE *f = u_fopen(ruta, "r");
        int distinto = 1;
        if (f) {
            u_fclose(f);
            if (!u_strcmp(ruta, d->ruta)) distinto = 0;
            else {
                int q = 0;
                while (ruta[q] && q < NNOM - 1) {
                    dlg_ruta[q] = ruta[q];
                    q++;
                }
                dlg_ruta[q] = 0;
                dlg = 4;
                dlg_accion = 1;
                return;
            }
            (void)distinto;
        }
    }
    if (doc_guarda(d, ruta)) {
        if (msg[0]) {
            u_strcpy(dlg_msg, msg);
            dlg = 5;
        }
        return;
    }
    dlg = 0;
    if (dlg_accion == 2) {
        /* venia de confirmar cierre: sigue cerrando */
        dlg_accion = 0;
        cierra_doc_ahora();
    }
}

/* ---- F32 Lang: indentacion automatica (solo .f32) ---- */
static int es_f32ruta(const char *r) {
    int l = u_strlen(r), i;
    if (l < 4) return 0;
    if (r[l - 4] != '.') return 0;
    if (r[l - 3] != 'f' && r[l - 3] != 'F') return 0;
    if (r[l - 2] != '3') return 0;
    if (r[l - 1] != '2') return 0;
    (void)i;
    return 1;
}

/* 1 si [ini,fin) recortado es exactamente la palabra w */
static int linea_es(doc_t *d, int ini, int fin, const char *w) {
    int a = ini, b = fin, i = 0;
    while (a < b && (d->txt[a] == ' ' || d->txt[a] == '\t')) a++;
    while (b > a && (d->txt[b - 1] == ' ' || d->txt[b - 1] == '\t')) b--;
    while (w[i]) {
        if (a + i >= b || d->txt[a + i] != w[i]) return 0;
        i++;
    }
    return a + i == b;
}

/* 1 si la linea recortada es w o empieza por w + espacio */
static int emp_o_igual(doc_t *d, int ini, int fin, const char *w) {
    int a = ini, b = fin, i = 0;
    while (a < b && (d->txt[a] == ' ' || d->txt[a] == '\t')) a++;
    while (b > a && (d->txt[b - 1] == ' ' || d->txt[b - 1] == '\t')) b--;
    while (w[i]) {
        if (a + i >= b || d->txt[a + i] != w[i]) return 0;
        i++;
    }
    return a + i == b || d->txt[a + i] == ' ' || d->txt[a + i] == '\t';
}

static int es_apertura_f32(doc_t *d, int ini, int fin) {
    return linea_es(d, ini, fin, "programa") ||
           emp_o_igual(d, ini, fin, "si") ||
           linea_es(d, ini, fin, "sinosi") ||
           linea_es(d, ini, fin, "sino") ||
           emp_o_igual(d, ini, fin, "mientras") ||
           emp_o_igual(d, ini, fin, "funcion") ||
           emp_o_igual(d, ini, fin, "para");
}

static int es_cierre_f32(doc_t *d, int ini, int fin) {
    return linea_es(d, ini, fin, "finsi") ||
           linea_es(d, ini, fin, "finmientras") ||
           linea_es(d, ini, fin, "finfuncion") ||
           linea_es(d, ini, fin, "finpara") ||
           linea_es(d, ini, fin, "fin") ||
           linea_es(d, ini, fin, "sinosi") ||
           linea_es(d, ini, fin, "sino");
}

/* indentacion (espacios iniciales) de la linea que empieza en li */
static int ind_de(doc_t *d, int li) {
    int n = 0;
    while (li + n < d->len && d->txt[li + n] == ' ') n++;
    return n;
}

/* quita n espacios en li con undo */
static void quita_ind(doc_t *d, int li, int n) {
    if (n <= 0) return;
    doc_del(d, li, n, 1);
}

/* inserta n espacios en pos (uno a uno, como Tab: mismo grano undo) */
static void mete_esp(doc_t *d, int pos) {
    int i;
    for (i = 0; i < 4; i++) {
        char sp = ' ';
        if (doc_ins(d, pos, &sp, 0, 0, 1, 1)) break;
        pos++;
        d->cur++;
    }
    d->anc = d->cur;
}

/* realinea la linea actual si es un cierre/continuacion f32 */
static void f32_alinea(doc_t *d) {
    int nl = lin_de_pos(d, d->cur);
    int li = lin_inicio(d, nl), lf = lin_fin(d, li);
    int t, ind, ps = 0, pm = 0, pf = 0;
    if (!linea_es(d, li, lf, "finsi") &&
        !linea_es(d, li, lf, "finmientras") &&
        !linea_es(d, li, lf, "finfuncion") &&
        !linea_es(d, li, lf, "fin") && !linea_es(d, li, lf, "sinosi") &&
        !linea_es(d, li, lf, "sino"))
        return;
    /* busca la apertura: cuenta cierres del mismo tipo */
    t = nl - 1;
    while (t >= 0) {
        int a = lin_inicio(d, t), b = lin_fin(d, a);
        if (linea_es(d, a, b, "finsi")) ps++;
        else if (linea_es(d, a, b, "finmientras"))
            pm++;
        else if (linea_es(d, a, b, "finfuncion"))
            pf++;
        else if (emp_o_igual(d, a, b, "si")) {
            if (ps > 0) ps--;
            else break;
        } else if (emp_o_igual(d, a, b, "mientras")) {
            if (pm > 0) pm--;
            else break;
        } else if (emp_o_igual(d, a, b, "funcion")) {
            if (pf > 0) pf--;
            else break;
        } else if (linea_es(d, a, b, "programa")) {
            if (!ps && !pm && !pf) break;
        }
        t--;
    }
    if (t < 0) return;
    ind = ind_de(d, lin_inicio(d, t));
    /* sinosi/sino/finsi/finmientras/finfuncion alinean con apertura;
       fin va al margen */
    if (linea_es(d, li, lf, "fin")) ind = 0;
    {
        int cur = ind_de(d, li), i;
        if (cur == ind) return;
        if (cur > ind) {
            quita_ind(d, li, cur - ind);
            if (d->cur > li) {
                d->cur -= cur - ind;
                if (d->cur < li + ind) d->cur = li + ind;
            }
        } else {
            for (i = 0; i < ind - cur; i++) {
                char sp = ' ';
                if (doc_ins(d, li + cur + i, &sp, 0, 0, 1, 1)) break;
                d->cur++;
            }
        }
        d->anc = d->cur;
    }
}

/* reduce un nivel la linea actual; 1 si cambio */
static int f32_sin_ind(doc_t *d) {
    int nl = lin_de_pos(d, d->cur);
    int li = lin_inicio(d, nl);
    int n = ind_de(d, li);
    int quita, nn;
    if (!n) return 0;
    nn = ((n - 1) / 4) * 4;
    quita = n - nn;
    quita_ind(d, li, quita);
    if (d->cur > li) {
        d->cur -= quita;
        if (d->cur < li) d->cur = li;
    }
    d->anc = d->cur;
    return 1;
}

/* ---- teclado ---- */
/* Decodifica tecla con modificadores del driver (sin carreras):
   devuelve el codigo base y pone sh/ct. Ctrl+letra llega como
   0xE100+idx (0xE200 con May); especiales con bits 0x200/0x400. */
static int dec_tecla(int k, int *sh, int *ct) {
    *sh = *ct = 0;
    if (k >= 0xE100 && k <= 0xE119) {
        *ct = 1;
        return 'a' + (k - 0xE100);
    }
    if (k >= 0xE200 && k <= 0xE219) {
        *ct = 1;
        *sh = 1;
        return 'a' + (k - 0xE200);
    }
    if (k >= 0x301 && k <= 0x30A) { *sh = 1; return k & 0x1FF; }
    if (k >= 0x501 && k <= 0x50A) { *ct = 1; return k & 0x1FF; }
    if (k >= 0x701 && k <= 0x70A) {
        *sh = 1;
        *ct = 1;
        return k & 0x1FF;
    }
    if (k == 0x7F) { *ct = 1; return 8; }
    return k;
}

static void tecla_doc(doc_t *d, int k, int mod) {
    int sh2 = 0, ct2 = 0;
    int shift, ctrl;
    /* Ctrl+Tab cambia de pestana (codificado en la tecla) */
    if (k == 0xE120) { cambia_pestana(1); return; }
    if (k == 0xE220) { cambia_pestana(-1); return; }
    /* May+Tab reduce indentacion (F32 y texto general) */
    if (k == TEC_MAY_TAB_U) {
        f32_sin_ind(d);
        return;
    }
    k = dec_tecla(k, &sh2, &ct2);
    shift = (sh2 || (mod & 1)) ? 1 : 0;
    ctrl = (ct2 || (mod & 2)) ? 1 : 0;
    /* atajos Ctrl */
    if (ctrl) {
        if (k == 'n' || k == 'N') { acc_nuevo(); return; }
        if (k == 'o' || k == 'O') {
            dlg = 1;
            dlg_browser_ini(d->ruta);
            return;
        }
        if (k == 's' || k == 'S') {
            if (shift) guarda_todos();
            else acc_guardar(d);
            return;
        }
        if (k == 'g' || k == 'G') {
            dlg = 2;
            dlg_accion = 0;
            dlg_browser_ini(d->ruta);
            return;
        }
        if (k == 'z' || k == 'Z') {
            if (shift) doc_rehacer(d);
            else doc_deshacer(d);
            return;
        }
        if (k == 'y' || k == 'Y') { doc_rehacer(d); return; }
        if (k == 'x' || k == 'X') { clip_corta(d); return; }
        if (k == 'c' || k == 'C') { clip_copia(d); return; }
        if (k == 'v' || k == 'V') { clip_pega(d); return; }
        if (k == 'e' || k == 'E' || k == 'a' || k == 'A') {
            /* Ctrl+E todo (Ctrl+A abre en otros; aqui tambien vale) */
            if (k == 'a' || k == 'A') {
                /* sin conflicto de Abrir: Abrir es Ctrl+O */
            }
            acc_todo(d);
            return;
        }
        if (k == 'f' || k == 'F') {
            bus_panel = 1;
            bus_pos_campo = 0;
            return;
        }
        if (k == 'r' || k == 'R') {
            bus_panel = 1;
            bus_pos_campo = 1;
            return;
        }
        if (k == 'b' || k == 'B') { acc_formato(d, EST_B); return; }
        if (k == 'i' || k == 'I') { acc_formato(d, EST_I); return; }
        if (k == 'u' || k == 'U') { acc_formato(d, EST_S); return; }
        if (k == 'k' || k == 'K') {
            dlg = 6;
            dlg_ccol = d->pen_col;
            dlg_cr = pers_r;
            dlg_cg = pers_g;
            dlg_cb = pers_b;
            return;
        }
        if (k == 't' || k == 'T') {
            tema_claro ^= 1;
            if (d) d->pen_col = tema_claro ? 0 : 1;
            return;
        }
        if (k == 'j' || k == 'J') {
            ajuste ^= 1;
            if (d) d->scol = 0;
            return;
        }
        if (k == 'w' || k == 'W') {
            if (!pide_cerrar_doc()) return;
            cierra_doc_ahora();
            return;
        }
        if (k == 'l' || k == 'L') {
            dlg = 7;
            dlg_campo[0] = 0;
            dlg_pos = 0;
            return;
        }
        return;
    }
    if (k == 0x101) {   /* arriba */
        int nl = lin_de_pos(d, d->cur), li;
        if (shift) {
            int li2;
            if (nl <= 0) return;
            li2 = lin_inicio(d, nl - 1);
            {
                int cc = celdas(d, lin_inicio(d, nl), d->cur);
                d->cur = byte_celda(d, li2, lin_fin(d, li2), cc);
            }
            return;
        }
        if (nl <= 0) { d->cur = 0; d->anc = d->cur; return; }
        li = lin_inicio(d, nl - 1);
        {
            int cc = celdas(d, lin_inicio(d, nl), d->cur);
            d->cur = byte_celda(d, li, lin_fin(d, li), cc);
        }
        d->anc = d->cur;
        return;
    }
    if (k == 0x102) {   /* abajo */
        int nl = lin_de_pos(d, d->cur), tot = nlineas(d), li;
        if (shift) {
            if (nl + 1 >= tot) return;
            li = lin_inicio(d, nl + 1);
            {
                int cc = celdas(d, lin_inicio(d, nl), d->cur);
                d->cur = byte_celda(d, li, lin_fin(d, li), cc);
            }
            return;
        }
        if (nl + 1 >= tot) {
            d->cur = d->len;
            d->anc = d->cur;
            return;
        }
        li = lin_inicio(d, nl + 1);
        {
            int cc = celdas(d, lin_inicio(d, nl), d->cur);
            d->cur = byte_celda(d, li, lin_fin(d, li), cc);
        }
        d->anc = d->cur;
        return;
    }
    if (k == 0x103) {   /* izq */
        if (ctrl) {
            int p = salto_pal(d, d->cur, -1);
            if (shift) d->cur = p;
            else d->cur = d->anc = p;
            return;
        }
        if (d->cur > 0) {
            int p = ant_c(d->txt, d->cur);
            /* salta \n de una */
            if (shift) d->cur = p;
            else d->cur = d->anc = p;
        } else if (!shift) {
            d->anc = d->cur;
        }
        return;
    }
    if (k == 0x104) {   /* der */
        if (ctrl) {
            int p = salto_pal(d, d->cur, 1);
            if (shift) d->cur = p;
            else d->cur = d->anc = p;
            return;
        }
        if (d->cur < d->len) {
            int p = sig_c(d->txt, d->cur, d->len);
            if (shift) d->cur = p;
            else d->cur = d->anc = p;
        } else if (!shift) {
            d->anc = d->cur;
        }
        return;
    }
    if (k == 0x107) {   /* inicio */
        int li = lin_inicio(d, lin_de_pos(d, d->cur));
        int p = ctrl ? 0 : li;
        if (shift) d->cur = p;
        else d->cur = d->anc = p;
        return;
    }
    if (k == 0x108) {   /* fin */
        int li = lin_inicio(d, lin_de_pos(d, d->cur));
        int lf = lin_fin(d, li);
        int p = ctrl ? d->len : lf;
        if (shift) d->cur = p;
        else d->cur = d->anc = p;
        return;
    }
    if (k == 0x109) {   /* repag */
        if (ctrl) { cambia_pestana(-1); return; }
        {
            int nl = lin_de_pos(d, d->cur);
            int nn = nl - 10;
            if (nn < 0) nn = 0;
            d->cur = lin_inicio(d, nn);
            if (!shift) d->anc = d->cur;
            d->slin -= 10;
            if (d->slin < 0) d->slin = 0;
        }
        return;
    }
    if (k == 0x10A) {   /* avpag */
        if (ctrl) { cambia_pestana(1); return; }
        {
            int nl = lin_de_pos(d, d->cur);
            int tot = nlineas(d), nn = nl + 10;
            if (nn >= tot) nn = tot - 1;
            d->cur = lin_inicio(d, nn);
            if (!shift) d->anc = d->cur;
            d->slin += 10;
        }
        return;
    }
    if (k == 8) {   /* retroceso */
        if (hay_sel(d)) {
            int a, b;
            sel_rango(d, &a, &b);
            doc_del(d, a, b - a, 1);
            d->cur = d->anc = a;
            return;
        }
        if (ctrl) { borra_palabra(d, 0); return; }
        if (d->cur > 0) {
            /* F32: al inicio de la indentacion borra un nivel */
            if (es_f32ruta(d->ruta)) {
                int nl = lin_de_pos(d, d->cur);
                int li = lin_inicio(d, nl), p = li, ok = 1;
                while (p < d->cur) {
                    if (d->txt[p] != ' ') { ok = 0; break; }
                    p++;
                }
                if (ok && d->cur > li) {
                    int cc = d->cur - li;
                    int nn = ((cc - 1) / 4) * 4, q = cc - nn;
                    doc_del(d, d->cur - q, q, 1);
                    d->cur = d->anc = d->cur - q;
                    return;
                }
            }
            {
                int a = ant_c(d->txt, d->cur);
                doc_del(d, a, d->cur - a, 1);
                d->cur = d->anc = a;
            }
        }
        return;
    }
    if (k == 0x106) {   /* supr */
        if (hay_sel(d)) {
            int a, b;
            sel_rango(d, &a, &b);
            doc_del(d, a, b - a, 1);
            d->cur = d->anc = a;
            return;
        }
        if (ctrl) { borra_palabra(d, 1); return; }
        if (d->cur < d->len) {
            int s = sig_c(d->txt, d->cur, d->len);
            doc_del(d, d->cur, s - d->cur, 1);
        }
        return;
    }
    if (k == 13) {   /* enter */
        char nl = '\n';
        if (hay_sel(d)) {
            int a, b;
            sel_rango(d, &a, &b);
            doc_del(d, a, b - a, 1);
            d->cur = d->anc = a;
        }
        doc_ins(d, d->cur, &nl, 0, 0, 1, 1);
        d->cur++;
        d->anc = d->cur;
        /* F32: auto-indent (copia nivel + 4 tras apertura) */
        if (es_f32ruta(d->ruta)) {
            int pl = lin_de_pos(d, d->cur - 1);
            int li = lin_inicio(d, pl), lf = lin_fin(d, li);
            int ind = ind_de(d, li), t, q;
            if (es_apertura_f32(d, li, lf)) ind += 4;
            q = ind;
            /* inserta de 4 en 4 como Tab (mismo grano undo) */
            while (q >= 4) {
                int pp = d->cur;
                mete_esp(d, pp);
                if (d->cur == pp) break;
                q -= 4;
            }
            for (t = 0; t < q; t++) {
                char sp = ' ';
                int pp = d->cur;
                if (doc_ins(d, pp, &sp, 0, 0, 1, 1)) break;
                d->cur++;
            }
            d->anc = d->cur;
        }
        return;
    }
    if (k == 9) {   /* tab: 4 espacios */
        int i;
        if (hay_sel(d)) {
            int a, b;
            sel_rango(d, &a, &b);
            doc_del(d, a, b - a, 1);
            d->cur = d->anc = a;
        }
        for (i = 0; i < 4; i++) {
            char sp = ' ';
            if (doc_ins(d, d->cur, &sp, 0, 0, 1, 1)) break;
            d->cur++;
        }
        d->anc = d->cur;
        return;
    }
    if (k >= 32 && k != 0x7F && !(k >= 0x101 && k <= 0x10A) &&
        !(k >= 0xE000 && k <= 0xE004) &&
        !(k >= 0xE100 && k <= 0xE219) &&
        !(k >= 0x300 && k <= 0x7FF)) {
        char tmp[5];
        int w = ut_enc(tmp, 5, (unsigned)k);
        if (!w) return;
        if (hay_sel(d)) {
            int a, b;
            sel_rango(d, &a, &b);
            doc_del(d, a, b - a, 1);
            d->cur = d->anc = a;
        }
        if (doc_ins(d, d->cur, tmp, 0, 0, w, 1)) return;
        d->cur += w;
        d->anc = d->cur;
        /* F32: al completar un cierre se realinea */
        if (es_f32ruta(d->ruta)) f32_alinea(d);
        return;
    }
}

/* ---- raton ---- */
static int clic_toolbar(int lx, int ly, int w) {
    int b;
    int f = tb_filas(w);
    for (b = 0; b < 17; b++) {
        int x, y;
        btn_geo(b, w, &x, &y);
        if (x + 30 > w - 2) continue;
        if (lx >= x && lx < x + 30 && ly >= y && ly < y + 30) return b;
    }
    (void)f;
    return -1;
}

static void pulsa_toolbar(int b) {
    doc_t *d = DA();
    if (b == 0) acc_nuevo();
    else if (b == 1) {
        dlg = 1;
        dlg_browser_ini(d ? d->ruta : "");
    } else if (b == 2) {
        if (d) acc_guardar(d);
    } else if (b == 3) {
        if (!d) return;
        dlg = 2;
        dlg_accion = 0;
        dlg_browser_ini(d->ruta);
    } else if (b == 4) {
        if (d) doc_deshacer(d);
    } else if (b == 5) {
        if (d) doc_rehacer(d);
    } else if (b == 6) {
        if (d) clip_corta(d);
    } else if (b == 7) {
        if (d) clip_copia(d);
    } else if (b == 8) {
        if (d) clip_pega(d);
    } else if (b == 9) {
        if (d) acc_todo(d);
    } else if (b == 10 || b == 11) {
        bus_panel = 1;
        bus_pos_campo = b == 10 ? 0 : 1;
    } else if (b == 12) {
        if (d) acc_formato(d, EST_B);
    } else if (b == 13) {
        if (d) acc_formato(d, EST_I);
    } else if (b == 14) {
        if (d) acc_formato(d, EST_S);
    } else if (b == 15) {
        dlg = 6;
        if (d) dlg_ccol = d->pen_col;
        dlg_cr = pers_r;
        dlg_cg = pers_g;
        dlg_cb = pers_b;
    } else if (b == 16) {
        if (!pide_cerrar_doc()) return;
        cierra_doc_ahora();
    }
}

/* convierte clic en area a byte pos */
static int area_a_pos(doc_t *d, ug_win_t *v, int lx, int ly) {
    int y0 = area_y0(v);
    int vf = d->slin + (ly - y0 - 1) / UG_FH;
    int ini, fin, vc = vis_cols(v);
    int tot = total_vis(d, v);
    if (vf < 0) vf = 0;
    if (vf >= tot) vf = tot - 1;
    vis_fila(d, v, vf, &ini, &fin);
    {
        int cells = (lx - 2 - num_w() - 8) / UG_FW + d->scol;
        if (!ajuste && cells < 0) cells = 0;
        if (ajuste) {
            int li0 = lin_inicio(d, lin_de_pos(d, ini));
            int rel = cells;
            if (rel < 0) rel = 0;
            /* fila visual: offset dentro de la logica */
            {
                int base = celdas(d, li0, ini);
                return byte_celda(d, ini, fin, base + cells - base);
            }
            (void)li0;
            (void)rel;
        }
        if (cells < 0) cells = 0;
        {
            int cc = cuenta_car(d->txt, ini, fin);
            if (!ajuste && cells > cc + 4) cells = cc;
            if (cells > vc + d->scol) cells = vc + d->scol;
            return byte_celda(d, ini, fin, cells);
        }
    }
}

static void clic_area(doc_t *d, ug_win_t *v, int lx, int ly, int arrastre) {
    int p = area_a_pos(d, v, lx, ly);
    if (arrastre && selec_arrastra) {
        d->cur = p;
        /* autodesplazamiento vertical */
        {
            int y0 = area_y0(v), y1 = estado_y0(v);
            if (ly < y0 + 16 && d->slin > 0) d->slin--;
            if (ly > y1 - 16) d->slin++;
        }
    } else {
        d->anc = d->cur = p;
        selec_arrastra = 1;
    }
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0, bot_antes = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    while (*a >= '0' && *a <= '9') a++;
    while (*a == ' ') a++;
    mi_srv = srv;
    msg[0] = 0;
    bus_q[0] = bus_r[0] = 0;
    if (ug_abre(&v, srv, 700, 500, "editor")) return 98;
    ug_edit(&v, 1);
    /* documento inicial */
    doc_nuevo();
    if (*a) {
        char ruta[NNOM];
        int i = 0;
        while (*a && i < NNOM - 1) ruta[i++] = *a++;
        ruta[i] = 0;
        {
            doc_t *d = DA();
            int r = doc_carga(d, ruta);
            if (r == -1) {
                /* nuevo con ese nombre */
                int q = 0;
                while (ruta[q] && q < NNOM - 1) {
                    d->ruta[q] = ruta[q];
                    q++;
                }
                d->ruta[q] = 0;
                pon_msg("nuevo archivo");
            } else if (r == -2) {
                u_strcpy(dlg_msg, "f32r roto: texto recuperado");
                dlg = 5;
            } else if (r == 1) {
                pon_msg("abierto (truncado a 32KB)");
            }
        }
    }
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        doc_t *d = DA();
        hover_btn = -1;
        if (t == UG_EV_SALIR) {
            if (!todos_limpios() && !confirma_sal) {
                confirma_sal = 1;
                u_strcpy(msg, "con cambios: otra vez Salir");
                continue;
            }
            return 0;
        }
        confirma_sal = 0;
        if (t == UG_EV_TECLA) {
            int k = ev.tecla, mod = u_teclado_mod();
            if (dlg == 1 || dlg == 2 || dlg == 7) {
                if (k == 13) {
                    if (dlg == 1) hace_abrir();
                    else if (dlg == 2)
                        hace_guardar_como();
                    else {
                        /* ir a linea */
                        int n = u_atoi(dlg_campo);
                        if (d && n >= 1) {
                            int tot = nlineas(d);
                            if (n > tot) n = tot;
                            d->cur = d->anc =
                                lin_inicio(d, n - 1);
                        }
                        dlg = 0;
                    }
                } else if (k == 27) {
                    dlg = 0;
                } else if ((dlg == 1 || dlg == 2) &&
                           (k == 0x101 || k == 0x301 || k == 0x102 ||
                            k == 0x302)) {
                    /* flechas: mueve seleccion (carpetas y archivos) */
                    int kb = k & 0x1FF, n2 = dlg_sel;
                    if (kb == 0x101) n2--;
                    else n2++;
                    if (n2 < 0) n2 = 0;
                    if (n2 >= dlg_nn) n2 = dlg_nn - 1;
                    if (n2 < 0) n2 = 0;
                    dlg_sel = n2;
                    if (dlg_sel < dlg_scroll) dlg_scroll = dlg_sel;
                    if (dlg_sel >= dlg_scroll + 6)
                        dlg_scroll = dlg_sel - 5;
                    /* refleja archivos en el campo */
                    if (dlg_sel >= 0 && dlg_sel < dlg_nn &&
                        dlg_tipos[dlg_sel] == 2) {
                        int q = 0;
                        while (dlg_noms[dlg_sel][q] && q < NNOM - 1) {
                            dlg_campo[q] = dlg_noms[dlg_sel][q];
                            q++;
                        }
                        dlg_campo[q] = 0;
                        dlg_pos = q;
                    }
                } else if ((dlg == 1 || dlg == 2) && k == 8 &&
                           dlg_pos == 0) {
                    dlg_subir();   /* retroceso con campo vacio = subir */
                } else {
                    ug_campo_tecla(dlg_campo, NNOM, &dlg_pos, k);
                }
            } else if (dlg == 3) {
                /* Guardar / Descartar / Nada: teclas g/d/esc */
                if (k == 'g' || k == 'G' || k == 13) {
                    if (dlg_accion == 1) {
                        doc_t *dd = DA();
                        if (dd && dd->sucio) {
                            if (!dd->ruta[0]) {
                                dlg = 2;
                                dlg_accion = 2;
                                dlg_browser_ini(dd->ruta);
                            } else {
                                doc_guarda(dd, dd->ruta);
                                cierra_doc_ahora();
                                dlg = 0;
                            }
                        } else {
                            cierra_doc_ahora();
                            dlg = 0;
                        }
                    }
                } else if (k == 'd' || k == 'D') {
                    cierra_doc_ahora();
                    dlg = 0;
                } else if (k == 27) {
                    dlg = 0;
                }
            } else if (dlg == 4) {
                if (k == 's' || k == 'S' || k == 13) {
                    doc_t *dd = DA();
                    if (dd) doc_guarda(dd, dlg_ruta);
                    dlg = 0;
                } else if (k == 27 || k == 'n' || k == 'N') {
                    dlg = 0;
                }
            } else if (dlg == 5) {
                if (k == 13 || k == 27 || k == ' ') dlg = 0;
            } else if (dlg == 6) {
                if (k == 27) dlg = 0;
                else if (k >= '0' && k <= '9') {
                    dlg_ccol = k - '0';
                } else if (k == 13) {
                    doc_t *dd = DA();
                    pers_r = dlg_cr;
                    pers_g = dlg_cg;
                    pers_b = dlg_cb;
                    if (dd) acc_color(dd, dlg_ccol);
                    dlg = 0;
                }
            } else if (bus_panel &&
                       (bus_pos_campo == 0 || bus_pos_campo == 1)) {
                /* may+flecha navega igual (sin seleccion aqui) */
                if ((k >= 0x301 && k <= 0x30A) ||
                    (k >= 0x501 && k <= 0x50A) ||
                    (k >= 0x701 && k <= 0x70A))
                    k &= 0x1FF;
                /* el panel captura texto si el foco esta en campos:
                   TAB alterna, ESC cierra, ENTER busca (en "por:"
                   cambia la actual; Ctrl+ENTER cambia todas) */
                if (k == 27) {
                    bus_panel = 0;
                    bus_nh = 0;
                } else if (k == 9) {
                    bus_pos_campo ^= 1;
                } else if (k == 13) {
                    if ((mod & 2) && bus_pos_campo == 1) {
                        if (d) bus_todas(d);
                    } else if (bus_pos_campo == 0) {
                        if (d) {
                            busca_todo(d);
                            if (bus_nh > 0) ir_a_hit(d, bus_sel);
                            else pon_msg("sin coincidencias");
                        }
                    } else {
                        if (d) bus_cambia(d);
                    }
                } else if (k == 0x101 || k == 0x102) {
                    if (d && bus_nh > 0) {
                        int n2 = bus_sel +
                                 (k == 0x102 ? 1 : -1);
                        if (n2 < 0) n2 = bus_nh - 1;
                        if (n2 >= bus_nh) n2 = 0;
                        ir_a_hit(d, n2);
                    }
                } else if (bus_pos_campo == 0) {
                    if (ug_campo_tecla(bus_q, 64, &bus_cpos, k)) {
                        bus_nh = 0;
                    }
                } else {
                    ug_campo_tecla(bus_r, 64, &bus_rpos, k);
                }
            } else if (d) {
                tecla_doc(d, k, mod);
            } else {
                /* sin documento: solo Nuevo y Abrir (con tecla ya
                   decodificada: Ctrl+letra llega como 0xE1xx) */
                int sh2 = 0, ct2 = 0, kk = dec_tecla(k, &sh2, &ct2);
                int cctrl = (ct2 || (mod & 2)) ? 1 : 0;
                if (cctrl && (kk == 'n' || kk == 'N')) acc_nuevo();
                else if (cctrl && (kk == 'o' || kk == 'O')) {
                    dlg = 1;
                    dlg_browser_ini("");
                }
            }
        } else if (t == UG_EV_RATON) {
            int lx = ev.rx - v.x, ly = ev.ry - (v.y + UG_CHROME);
            rat_lx = lx;
            rat_ly = ly;
            /* tooltip: cuenta tiempo sobre boton (simple: muestra) */
            {
                int b = clic_toolbar(lx, ly, v.w);
                if (b >= 0) hover_btn = b;
            }
            if (dlg == 1 || dlg == 2) {
                int w = v.w - 80, x = 40;
                int H = 236, y = (v.h - UG_CHROME) / 2 - H / 2;
                int n, ay;
                if (w < 240) { w = v.w - 16; x = 8; }
                if (w > 560) { w = 560; x = (v.w - w) / 2; }
                if (y < 56) y = 56;
                ay = y + 48;
                /* subir */
                if (ug_clic(bot_antes, ev.rbot) &&
                    ug_dentro(lx, ly, x + w - 70, y + 24, 62, 18)) {
                    dlg_subir();
                } else {
                    /* lista: clic selecciona, doble clic entra/abre */
                    for (n = 0; n < 6; n++) {
                        int id = dlg_scroll + n;
                        if (id < 0 || id >= dlg_nn) continue;
                        if (ug_dentro(lx, ly, x + 8, ay + n * 16,
                                      w - 16, 16)) {
                            if (ug_clic(bot_antes, ev.rbot)) {
                                unsigned int ahora =
                                    (unsigned int)u_ticks();
                                if (id == dlg_ult_id &&
                                    ahora - dlg_ult_clic < 50) {
                                    /* doble clic */
                                    if (dlg_tipos[id] == 1) {
                                        dlg_sel = id;
                                        dlg_entra_sel();
                                    } else {
                                        int q = 0;
                                        while (dlg_noms[id][q] &&
                                               q < NNOM - 1) {
                                            dlg_campo[q] =
                                                dlg_noms[id][q];
                                            q++;
                                        }
                                        dlg_campo[q] = 0;
                                        dlg_pos = q;
                                        if (dlg == 1) hace_abrir();
                                        else hace_guardar_como();
                                    }
                                    dlg_ult_id = -1;
                                } else {
                                    dlg_sel = id;
                                    dlg_ult_id = id;
                                    dlg_ult_clic = ahora;
                                    if (dlg_tipos[id] == 2) {
                                        int q = 0;
                                        while (dlg_noms[id][q] &&
                                               q < NNOM - 1) {
                                            dlg_campo[q] =
                                                dlg_noms[id][q];
                                            q++;
                                        }
                                        dlg_campo[q] = 0;
                                        dlg_pos = q;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                if (ug_clic(bot_antes, ev.rbot)) {
                    if (ug_dentro(lx, ly, x + 8, y + 196, 100, 22)) {
                        if (dlg == 1) hace_abrir();
                        else hace_guardar_como();
                    } else if (ug_dentro(lx, ly, x + 116, y + 196, 100,
                                         22)) {
                        dlg = 0;
                    }
                }
                /* rueda: desplaza la lista */
                if (ev.rueda) {
                    if (ev.rueda > 0) dlg_scroll -= 3;
                    else dlg_scroll += 3;
                    if (dlg_scroll < 0) dlg_scroll = 0;
                    if (dlg_scroll > dlg_sel) dlg_scroll = dlg_sel;
                }
            } else if (dlg == 3) {
                if (ug_clic(bot_antes, ev.rbot)) {
                    int w = v.w - 160, x = 80;
                    int y = (v.h - UG_CHROME) / 2 - 44;
                    if (w < 200) { w = v.w - 16; x = 8; }
                    if (ug_dentro(lx, ly, x + 8, y + 54, 90, 22)) {
                        doc_t *dd = DA();
                        if (dd && dd->sucio) {
                            if (!dd->ruta[0]) {
                                dlg = 2;
                                dlg_accion = 2;
                                dlg_browser_ini(dd->ruta);
                            } else {
                                doc_guarda(dd, dd->ruta);
                                cierra_doc_ahora();
                                dlg = 0;
                            }
                        } else {
                            cierra_doc_ahora();
                            dlg = 0;
                        }
                    } else if (ug_dentro(lx, ly, x + 104, y + 54, 90,
                                         22)) {
                        cierra_doc_ahora();
                        dlg = 0;
                    } else if (ug_dentro(lx, ly, x + 200, y + 54, 90,
                                         22)) {
                        dlg = 0;
                    }
                }
            } else if (dlg == 4) {
                if (ug_clic(bot_antes, ev.rbot)) {
                    int w = v.w - 160, x = 80;
                    int y = (v.h - UG_CHROME) / 2 - 44;
                    if (w < 200) { w = v.w - 16; x = 8; }
                    if (ug_dentro(lx, ly, x + 8, y + 54, 90, 22)) {
                        doc_t *dd = DA();
                        if (dd) doc_guarda(dd, dlg_ruta);
                        dlg = 0;
                    } else if (ug_dentro(lx, ly, x + 104, y + 54, 90,
                                         22)) {
                        dlg = 0;
                    }
                }
            } else if (dlg == 5) {
                if (ug_clic(bot_antes, ev.rbot)) {
                    int w = v.w - 160, x = 80;
                    int y = (v.h - UG_CHROME) / 2 - 40;
                    if (w < 200) { w = v.w - 16; x = 8; }
                    if (ug_dentro(lx, ly, x + 8, y + 50, 100, 22))
                        dlg = 0;
                }
            } else if (dlg == 6) {
                int w = 340, x = (v.w - w) / 2;
                int y = (v.h - UG_CHROME) / 2 - 80;
                int i;
                if (x < 8) x = 8;
                if (ug_clic(bot_antes, ev.rbot)) {
                    for (i = 0; i < 10; i++) {
                        int cx = x + 8 + (i % 5) * 64,
                            cy = y + 26 + (i / 5) * 30;
                        if (ug_dentro(lx, ly, cx, cy, 56, 22)) {
                            dlg_ccol = i;
                            if (i == 9) {
                                dlg_cr = pers_r;
                                dlg_cg = pers_g;
                                dlg_cb = pers_b;
                            }
                            break;
                        }
                    }
                    if (ug_dentro(lx, ly, x + 8, y + 130, 100, 22)) {
                        doc_t *dd = DA();
                        pers_r = dlg_cr;
                        pers_g = dlg_cg;
                        pers_b = dlg_cb;
                        if (dd) acc_color(dd, dlg_ccol);
                        dlg = 0;
                    } else if (ug_dentro(lx, ly, x + 116, y + 130, 100,
                                         22)) {
                        dlg = 0;
                    }
                }
            } else if (dlg == 7) {
                if (ug_clic(bot_antes, ev.rbot)) {
                    int w = 260, x = (v.w - w) / 2;
                    int y = (v.h - UG_CHROME) / 2 - 40;
                    if (x < 8) x = 8;
                    if (ug_dentro(lx, ly, x + 8, y + 50, 100, 22)) {
                        int n = u_atoi(dlg_campo);
                        if (d && n >= 1) {
                            int tot = nlineas(d);
                            if (n > tot) n = tot;
                            d->cur = d->anc = lin_inicio(d, n - 1);
                        }
                        dlg = 0;
                    } else if (ug_dentro(lx, ly, x + 116, y + 50, 100,
                                         22)) {
                        dlg = 0;
                    }
                }
            } else {
                /* pestañas */
                if (ly >= 2 && ly < 22 && ug_clic(bot_antes, ev.rbot)) {
                    int x = 2, k;
                    for (k = 0; k < MAXDOC; k++) {
                        char nm[20], et[24];
                        int t2 = 0, q = 0, tw;
                        if (!docs[k].uso) continue;
                        nom_corto(docs[k].ruta, nm, 14);
                        while (nm[q] && t2 < 20)
                            et[t2++] = nm[q++];
                        if (docs[k].sucio && t2 < 22)
                            et[t2++] = '*';
                        et[t2] = 0;
                        tw = u_strlen(et) * UG_FW + 12;
                        if (lx >= x && lx < x + tw) {
                            if (docs[k].sucio &&
                                (ev.rbot & 2)) {
                                /* boton derecho: nada */
                            }
                            dact = k;
                            bus_nh = 0;
                            bus_sel = -1;
                            break;
                        }
                        x += tw + 2;
                    }
                }
                /* toolbar */
                {
                    int b = clic_toolbar(lx, ly, v.w);
                    if (b >= 0 && ug_clic(bot_antes, ev.rbot)) {
                        pulsa_toolbar(b);
                    }
                }
                /* estado: tema / ajuste */
                if (d && ug_clic(bot_antes, ev.rbot)) {
                    int sy = estado_y0(&v);
                    if (ly >= sy + 19 && ly < sy + 36) {
                        if (lx >= 150 && lx < 230) {
                            tema_claro ^= 1;
                            if (d) d->pen_col = tema_claro ? 0 : 1;
                        } else if (lx >= 235 && lx < 330) {
                            ajuste ^= 1;
                            d->scol = 0;
                        }
                    }
                }
                /* panel buscar */
                if (bus_panel && ug_clic(bot_antes, ev.rbot)) {
                    int yb = estado_y0(&v) - 62;
                    if (yb < 120) yb = 120;
                    if (ug_dentro(lx, ly, 470, yb + 2, 70, 20)) {
                        if (d && bus_nh > 0) {
                            int n2 = bus_sel - 1;
                            if (n2 < 0) n2 = bus_nh - 1;
                            ir_a_hit(d, n2);
                        } else if (d) {
                            busca_todo(d);
                            if (bus_nh > 0) ir_a_hit(d, bus_sel);
                        }
                    } else if (ug_dentro(lx, ly, 544, yb + 2, 70, 20)) {
                        if (d && bus_nh > 0) {
                            int n2 = bus_sel + 1;
                            if (n2 >= bus_nh) n2 = 0;
                            ir_a_hit(d, n2);
                        } else if (d) {
                            busca_todo(d);
                            if (bus_nh > 0) ir_a_hit(d, bus_sel);
                        }
                    } else if (ug_dentro(lx, ly, 250, yb + 24, 90, 20)) {
                        /* cambiar actual */
                        if (d) bus_cambia(d);
                    } else if (ug_dentro(lx, ly, 344, yb + 24, 90, 20)) {
                        /* todas */
                        if (d) bus_todas(d);
                    } else if (ug_dentro(lx, ly, 438, yb + 24, 90, 20)) {
                        bus_may ^= 1;
                        if (!bus_may) {
                            /* segunda pulsacion alterna palabra */
                            /* (simple: May->Pal->May+Pal->ninguna) */
                        }
                        busca_todo(d);
                    } else if (ug_dentro(lx, ly, 532, yb + 24, 70, 20)) {
                        bus_panel = 0;
                        bus_nh = 0;
                    } else if (ug_dentro(lx, ly, 60, yb + 24, 80, 16)) {
                        bus_may ^= 1;
                        busca_todo(d);
                    } else if (ug_dentro(lx, ly, 150, yb + 24, 80, 16)) {
                        bus_pal ^= 1;
                        busca_todo(d);
                    } else if (ug_dentro(lx, ly, 80, yb + 2, 180, 20)) {
                        bus_pos_campo = 0;
                    } else if (ug_dentro(lx, ly, 310, yb + 2, 150, 20)) {
                        bus_pos_campo = 1;
                    }
                }
                /* area de edicion */
                if (d && dlg == 0) {
                    int y0 = area_y0(&v), y1 = estado_y0(&v);
                    int ax = 2, aw = v.w - 4;
                    if (ly >= y0 && ly < y1 && lx >= ax + num_w() &&
                        lx < ax + aw - 12) {
                        if (ug_clic(bot_antes, ev.rbot)) {
                            clic_area(d, &v, lx, ly, 0);
                        } else if ((ev.rbot & 1) && selec_arrastra) {
                            clic_area(d, &v, lx, ly, 1);
                        }
                    }
                    /* scrollbar */
                    {
                        int sx = ax + aw - 12;
                        int y0b = y0, ah = y1 - y0;
                        if (ug_clic(bot_antes, ev.rbot) && lx >= sx &&
                            lx < sx + 12 && ly >= y0b && ly < y0b + ah) {
                            int tot = total_vis(d, &v);
                            int filas =
                                (y1 - y0) / UG_FH;
                            if (filas < 1) filas = 1;
                            if (ly < y0b + ah / 2) {
                                d->slin -= filas;
                                if (d->slin < 0) d->slin = 0;
                            } else {
                                d->slin += filas;
                                if (d->slin > tot - filas)
                                    d->slin = tot - filas;
                                if (d->slin < 0) d->slin = 0;
                            }
                        }
                    }
                    if (!(ev.rbot & 1)) selec_arrastra = 0;
                    if (ev.rueda) {
                        if (ev.rueda > 0) d->slin -= 3;
                        else d->slin += 3;
                        if (d->slin < 0) d->slin = 0;
                    }
                }
            }
            bot_antes = ev.rbot;
        } else if (t == UG_EV_DRAW) {
            dibuja(&v);
        } else if (t == UG_EV_FOCO) {
            if (!ev.foco) selec_arrastra = 0;
        }
    }
}

