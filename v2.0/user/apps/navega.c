void __main(void) {
}

#include "ulib.h"
#include "nsfetch.h"
#include "hubbub/hubbub.h"
#include "hubbub/parser.h"
#include "hubbub/errors.h"
#include "hubbub/functypes.h"
#include "hubbub/types.h"
#include <dom/dom.h>
#include <libcss/libcss.h>
#include <libcss/fpmath.h>
#include <libnsbmp.h>
#include <nsgif.h>
#include "ugfx.h"
#include "utext.h"

/* navega (Hito 3 NetSurf): visor minimo web/local.
 * fetch (nsfetch) o fichero -> hubbub -> libdom -> select CSS
 * (color/fondo) -> layout de texto -> ventana ugfx con scroll.
 * Sin JS, sin imagenes, sin formularios, sin atributos style
 * en linea (documentado). -p = resumen a consola sin ventana. */

#define NV_MAXTEXTO (64 * 1024)
#define NV_MAXFILAS 2048
#define NV_MAXPILA 64
#define NV_MAXTITULO 128
#define NV_MAXCSS (8 * 1024)
#define NV_MAXURL 512
#define NV_W 640
#define NV_H 460

static unsigned char NV_txt[NV_MAXTEXTO];
static int NV_ntxt = 0;

typedef struct {
    int ini, len;
    int fg;                    /* 0xRRGGBB o -1 = defecto */
    int bg;                    /* 0xRRGGBB o -1 = ninguno */
    int estilo;                /* 0 normal, 1 vacia, 2 HR, 3 IMG */
    int img;                   /* indice en NV_img o -1 */
} nv_fila_t;
static nv_fila_t NV_filas[NV_MAXFILAS];
static int NV_nfilas = 0;
static int NV_nestilos = 0;

/* ---- etiquetas ---- */
enum {
    T_OTRO = 0, T_P, T_DIV, T_H1, T_H2, T_H3, T_H4, T_H5, T_H6,
    T_UL, T_OL, T_LI, T_PRE, T_BR, T_HR, T_BLOQUE, T_A,
    T_SCRIPT, T_STYLE, T_TITLE, T_HEAD, T_HTML, T_BODY, T_META,
    T_LINK, T_IMG, T_INPUT, T_FORM, T_TABLE, T_TR, T_TD, T_TH
};

static int nv_tag_id(const char *s, int n) {
    char b[16];
    int i;
    if (n <= 0 || n > 15) return T_OTRO;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        b[i] = c;
    }
    b[n] = 0;
    if (!u_strcmp(b, "p")) return T_P;
    if (!u_strcmp(b, "div")) return T_DIV;
    if (!u_strcmp(b, "h1")) return T_H1;
    if (!u_strcmp(b, "h2")) return T_H2;
    if (!u_strcmp(b, "h3")) return T_H3;
    if (!u_strcmp(b, "h4")) return T_H4;
    if (!u_strcmp(b, "h5")) return T_H5;
    if (!u_strcmp(b, "h6")) return T_H6;
    if (!u_strcmp(b, "ul")) return T_UL;
    if (!u_strcmp(b, "ol")) return T_OL;
    if (!u_strcmp(b, "li")) return T_LI;
    if (!u_strcmp(b, "pre")) return T_PRE;
    if (!u_strcmp(b, "br")) return T_BR;
    if (!u_strcmp(b, "hr")) return T_HR;
    if (!u_strcmp(b, "blockquote")) return T_BLOQUE;
    if (!u_strcmp(b, "section") || !u_strcmp(b, "article") ||
        !u_strcmp(b, "header") || !u_strcmp(b, "footer") ||
        !u_strcmp(b, "nav") || !u_strcmp(b, "main") ||
        !u_strcmp(b, "aside") || !u_strcmp(b, "figure") ||
        !u_strcmp(b, "figcaption")) return T_DIV;
    if (!u_strcmp(b, "a")) return T_A;
    if (!u_strcmp(b, "script")) return T_SCRIPT;
    if (!u_strcmp(b, "style")) return T_STYLE;
    if (!u_strcmp(b, "title")) return T_TITLE;
    if (!u_strcmp(b, "head")) return T_HEAD;
    if (!u_strcmp(b, "html")) return T_HTML;
    if (!u_strcmp(b, "body")) return T_BODY;
    if (!u_strcmp(b, "meta")) return T_META;
    if (!u_strcmp(b, "link")) return T_LINK;
    if (!u_strcmp(b, "img")) return T_IMG;
    if (!u_strcmp(b, "input")) return T_INPUT;
    if (!u_strcmp(b, "form")) return T_FORM;
    if (!u_strcmp(b, "table")) return T_TABLE;
    if (!u_strcmp(b, "tr")) return T_TR;
    if (!u_strcmp(b, "td")) return T_TD;
    if (!u_strcmp(b, "th")) return T_TH;
    return T_OTRO;
}

static int nv_es_vacio(int id) {
    return id == T_BR || id == T_HR || id == T_IMG ||
           id == T_META || id == T_LINK || id == T_INPUT;
}

static int nv_es_bloque(int id) {
    return id == T_P || id == T_DIV || id == T_H1 || id == T_H2 ||
           id == T_H3 || id == T_H4 || id == T_H5 || id == T_H6 ||
           id == T_UL || id == T_OL || id == T_LI || id == T_PRE ||
           id == T_BLOQUE || id == T_HR || id == T_TABLE ||
           id == T_TR || id == T_TD || id == T_TH;
}

/* ---- puente hubbub -> libdom ---- */
typedef struct {
    hubbub_parser *ps;
    dom_document *doc;
    dom_node *pila[NV_MAXPILA];
    int tags[NV_MAXPILA];
    int npila;
    int salta;                 /* T_SCRIPT/T_STYLE/T_TITLE o -1 */
    char estilo[NV_MAXCSS];
    int nestilo;
    char titulo[NV_MAXTITULO];
    int ntitulo;
} nv_t;

static void nv_empuja(nv_t *e, dom_node *n, int tag) {
    if (e->npila < NV_MAXPILA) {
        e->pila[e->npila] = n;
        e->tags[e->npila] = tag;
        e->npila++;
    }
}

static dom_node *nv_cima(nv_t *e) {
    if (!e->npila) return 0;
    return e->pila[e->npila - 1];
}

static void nv_cierra_hasta(nv_t *e, int tag) {
    int k;
    for (k = e->npila - 1; k >= 0; k--) {
        if (e->tags[k] == tag) {
            e->npila = k;
            return;
        }
    }
}

static void nv_txt_nodo(nv_t *e, const unsigned char *p, int n) {
    dom_string *s = 0;
    dom_text *tx = 0;
    dom_node *hijo = 0, *padre;
    if (n <= 0) return;
    padre = nv_cima(e);
    if (!padre) return;
    if (dom_string_create(p, (unsigned)n, &s) != DOM_NO_ERR || !s)
        return;
    if (dom_document_create_text_node(e->doc, s,
            (struct dom_text **)&tx) == DOM_NO_ERR && tx) {
        if (dom_node_append_child(padre, (dom_node *)tx, &hijo) ==
            DOM_NO_ERR && hijo)
            dom_node_unref(hijo);
        dom_node_unref((dom_node *)tx);
    }
    dom_string_unref(s);
}

static hubbub_error nv_tok(const hubbub_token *tk, void *pw) {
    nv_t *e = (nv_t *)pw;
    hubbub_parser_optparams prm;
    char nb[16];
    int i, nn, id;
    if (!tk) return HUBBUB_OK;
    if (tk->type == HUBBUB_TOKEN_DOCTYPE ||
        tk->type == HUBBUB_TOKEN_COMMENT ||
        tk->type == HUBBUB_TOKEN_EOF)
        return HUBBUB_OK;
    if (tk->type == HUBBUB_TOKEN_CHARACTER) {
        const unsigned char *p = tk->data.character.ptr;
        int n = (int)tk->data.character.len;
        if (e->salta == T_SCRIPT) return HUBBUB_OK;
        if (e->salta == T_STYLE) {
            int k;
            for (k = 0; k < n && e->nestilo < NV_MAXCSS - 1; k++)
                e->estilo[e->nestilo++] = (char)p[k];
            e->estilo[e->nestilo] = 0;
            return HUBBUB_OK;
        }
        if (e->salta == T_TITLE) {
            int k;
            for (k = 0; k < n && e->ntitulo < NV_MAXTITULO - 1; k++)
                e->titulo[e->ntitulo++] = (char)p[k];
            e->titulo[e->ntitulo] = 0;
            return HUBBUB_OK;
        }
        nv_txt_nodo(e, p, n);
        return HUBBUB_OK;
    }
    nn = (int)tk->data.tag.name.len;
    if (nn > 15) nn = 15;
    for (i = 0; i < nn; i++) nb[i] = (char)tk->data.tag.name.ptr[i];
    nb[nn] = 0;
    id = nv_tag_id(nb, (int)tk->data.tag.name.len);
    if (tk->type == HUBBUB_TOKEN_END_TAG) {
        if (e->salta >= 0) {
            if (id == e->salta) {
                e->salta = -1;
                prm.content_model.model =
                    HUBBUB_CONTENT_MODEL_PCDATA;
                hubbub_parser_setopt(e->ps,
                    HUBBUB_PARSER_CONTENT_MODEL, &prm);
            }
            return HUBBUB_OK;
        }
        nv_cierra_hasta(e, id);
        return HUBBUB_OK;
    }
    if (e->salta >= 0) return HUBBUB_OK;
    if (id == T_SCRIPT || id == T_STYLE || id == T_TITLE) {
        e->salta = id;
        prm.content_model.model = id == T_TITLE ?
            HUBBUB_CONTENT_MODEL_RCDATA : HUBBUB_CONTENT_MODEL_CDATA;
        hubbub_parser_setopt(e->ps, HUBBUB_PARSER_CONTENT_MODEL, &prm);
        return HUBBUB_OK;
    }
    if (nv_es_vacio(id)) {
        /* BR/HR dejan marca en el DOM; IMG tambien (Hito 5:
           el layout lee src/alt); el resto se ignora */
        if (id != T_BR && id != T_HR && id != T_IMG)
            return HUBBUB_OK;
    }
    {
        dom_string *tag = 0;
        dom_element *el = 0;
        dom_node *hijo = 0, *padre = nv_cima(e);
        int tl = 0;
        if (!padre) return HUBBUB_OK;
        while (nb[tl]) tl++;
        if (dom_string_create((const unsigned char *)nb,
                              (unsigned)tl, &tag) != DOM_NO_ERR ||
            !tag)
            return HUBBUB_OK;
        if (dom_document_create_element(e->doc, tag,
                (struct dom_element **)&el) != DOM_NO_ERR || !el) {
            dom_string_unref(tag);
            return HUBBUB_OK;
        }
        dom_string_unref(tag);
        if (dom_node_append_child(padre, (dom_node *)el, &hijo) ==
            DOM_NO_ERR) {
            if (hijo) dom_node_unref(hijo);
            /* Hito 4/5: href en <a>, src/alt en <img>,
               style en linea en cualquier elemento */
            {
                uint32_t ai;
                for (ai = 0; ai < tk->data.tag.n_attributes; ai++) {
                    const hubbub_attribute *at =
                        &tk->data.tag.attributes[ai];
                    const char *want = 0;
                    size_t cap = 255;
                    if (at->name.len == 5 &&
                        (at->name.ptr[0] == 's' ||
                         at->name.ptr[0] == 'S') &&
                        (at->name.ptr[1] == 't' ||
                         at->name.ptr[1] == 'T') &&
                        (at->name.ptr[2] == 'y' ||
                         at->name.ptr[2] == 'Y') &&
                        (at->name.ptr[3] == 'l' ||
                         at->name.ptr[3] == 'L') &&
                        (at->name.ptr[4] == 'e' ||
                         at->name.ptr[4] == 'E')) {
                        want = "style";
                        cap = 511;
                    } else if (at->name.len == 4 &&
                        (at->name.ptr[0] == 'h' ||
                         at->name.ptr[0] == 'H') &&
                        (at->name.ptr[1] == 'r' ||
                         at->name.ptr[1] == 'R') &&
                        (at->name.ptr[2] == 'e' ||
                         at->name.ptr[2] == 'E') &&
                        (at->name.ptr[3] == 'f' ||
                         at->name.ptr[3] == 'F')) {
                        if (id != T_A) continue;
                        want = "href";
                    } else if (at->name.len == 3 &&
                               (at->name.ptr[0] == 's' ||
                                at->name.ptr[0] == 'S') &&
                               (at->name.ptr[1] == 'r' ||
                                at->name.ptr[1] == 'R') &&
                               (at->name.ptr[2] == 'c' ||
                                at->name.ptr[2] == 'C')) {
                        if (id != T_IMG) continue;
                        want = "src";
                    } else if (at->name.len == 3 &&
                               (at->name.ptr[0] == 'a' ||
                                at->name.ptr[0] == 'A') &&
                               (at->name.ptr[1] == 'l' ||
                                at->name.ptr[1] == 'L') &&
                               (at->name.ptr[2] == 't' ||
                                at->name.ptr[2] == 'T')) {
                        if (id != T_IMG) continue;
                        want = "alt";
                        cap = 127;
                    } else {
                        continue;
                    }
                    {
                        dom_string *nm = 0, *v = 0;
                        size_t vl = at->value.len;
                        int wl = 0;
                        if (vl > cap) vl = cap;
                        while (want[wl]) wl++;
                        if (dom_string_create(
                                (const unsigned char *)want,
                                (unsigned)wl,
                                &nm) == DOM_NO_ERR && nm &&
                            dom_string_create(at->value.ptr, vl,
                                              &v) == DOM_NO_ERR &&
                            v) {
                            dom_element_set_attribute(el, nm, v);
                            dom_string_unref(v);
                        }
                        if (nm) dom_string_unref(nm);
                    }
                }
            }
            if (!nv_es_vacio(id) &&
                !tk->data.tag.self_closing)
                nv_empuja(e, (dom_node *)el, id);
        }
        dom_node_unref((dom_node *)el);
    }
    return HUBBUB_OK;
}

/* ---- select CSS propio sobre libdom (sin codigo GPL) ---- */
typedef struct {
    void *nodo;
    void *dato;
} nv_nd_t;
static nv_nd_t NV_tabla[512];
static int NV_ntab = 0;

static css_error nv_set_dato(void *pw, void *nodo, void *dato) {
    int i;
    (void)pw;
    for (i = 0; i < NV_ntab; i++) {
        if (NV_tabla[i].nodo == nodo) {
            NV_tabla[i].dato = dato;
            return CSS_OK;
        }
    }
    if (NV_ntab >= 512) return CSS_NOMEM;
    NV_tabla[NV_ntab].nodo = nodo;
    NV_tabla[NV_ntab].dato = dato;
    NV_ntab++;
    return CSS_OK;
}

static css_error nv_get_dato(void *pw, void *nodo, void **dato) {
    int i;
    (void)pw;
    for (i = 0; i < NV_ntab; i++) {
        if (NV_tabla[i].nodo == nodo) {
            *dato = NV_tabla[i].dato;
            return CSS_OK;
        }
    }
    *dato = 0;
    return CSS_OK;
}

/* nombre del elemento en minusculas (hasta 15 + NUL), -1 si no */
static int nv_nom_el(void *nodo, char *sal) {
    dom_node *n = (dom_node *)nodo;
    dom_string *nm = 0;
    const uint8_t *p;
    uint32_t m, i;
    dom_node_type t;
    if (!n || !sal) return -1;
    if (dom_node_get_node_type(n, &t) != DOM_NO_ERR) return -1;
    if (t != DOM_ELEMENT_NODE) return -1;
    if (dom_node_get_node_name(n, &nm) != DOM_NO_ERR || !nm)
        return -1;
    p = dom_string_data(nm);
    m = dom_string_length(nm);
    if (m > 15) m = 15;
    for (i = 0; i < m; i++) {
        unsigned char c = p[i];
        sal[i] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    }
    sal[m] = 0;
    dom_string_unref(nm);
    return 0;
}

static int nv_atributo(dom_node *n, const char *at, char *sal,
                       int max)
{
    dom_string *nm = 0, *v = 0;
    const uint8_t *p;
    uint32_t m, i;
    int k = 0;
    char nb[64], tmp[16];
    if (!n || !at || !sal || max < 1) return -1;
    /* dom_element_get_attribute solo vale en elementos */
    if (nv_nom_el(n, tmp)) return -1;
    while (at[k] && k < 63) {
        nb[k] = at[k];
        k++;
    }
    nb[k] = 0;
    if (dom_string_create((const unsigned char *)nb, (unsigned)k,
                          &nm) != DOM_NO_ERR || !nm)
        return -1;
    if (dom_element_get_attribute((dom_element *)n, nm,
                                  (dom_string **)&v) != DOM_NO_ERR ||
        !v) {
        dom_string_unref(nm);
        return -1;
    }
    dom_string_unref(nm);
    p = dom_string_data(v);
    m = dom_string_length(v);
    if ((int)m > max - 1) m = (uint32_t)(max - 1);
    for (i = 0; i < m; i++) sal[i] = (char)p[i];
    sal[m] = 0;
    dom_string_unref(v);
    return 0;
}

static css_error nv_qname(void *pw, void *nodo, css_qname *qn) {
    char nb[16];
    (void)pw;
    if (!qn) return CSS_BADPARM;
    qn->ns = 0;
    qn->name = 0;
    if (!nodo || nv_nom_el(nodo, nb)) return CSS_OK;
    if (lwc_intern_string(nb, u_strlen(nb), &qn->name) != lwc_error_ok)
        return CSS_NOMEM;
    return CSS_OK;
}

static int nv_mismo_nom(void *a, const char *b) {
    char nb[16];
    if (nv_nom_el(a, nb)) return 0;
    return !u_strcmp(nb, b);
}

/* antecesor con ese nombre (o 0) */
static css_error nv_ante(void *pw, void *nodo, const css_qname *qn,
                         void **res) {
    dom_node *h = 0;
    char nb[16];
    const uint8_t *p;
    uint32_t m, i;
    (void)pw;
    *res = 0;
    if (!nodo || !qn || !qn->name) return CSS_OK;
    p = lwc_string_data(qn->name);
    m = lwc_string_length(qn->name);
    if (m > 15 || !m) return CSS_OK;
    for (i = 0; i < m; i++) nb[i] = (char)p[i];
    nb[m] = 0;
    if (dom_node_get_parent_node((dom_node *)nodo, &h) != DOM_NO_ERR ||
        !h)
        return CSS_OK;
    for (;;) {
        if (nv_mismo_nom(h, nb)) {
            *res = h;
            return CSS_OK;
        }
        {
            dom_node *s = 0;
            if (dom_node_get_parent_node(h, &s) != DOM_NO_ERR ||
                !s) {
                dom_node_unref(h);
                return CSS_OK;
            }
            dom_node_unref(h);
            h = s;
        }
    }
}

static css_error nv_padre(void *pw, void *nodo, const css_qname *qn,
                          void **res) {
    dom_node *h = 0, *p = 0;
    char nb[16];
    const uint8_t *q;
    uint32_t m, i;
    (void)pw;
    *res = 0;
    if (!nodo || !qn || !qn->name) return CSS_OK;
    q = lwc_string_data(qn->name);
    m = lwc_string_length(qn->name);
    if (m > 15 || !m) return CSS_OK;
    for (i = 0; i < m; i++) nb[i] = (char)q[i];
    nb[m] = 0;
    if (dom_node_get_parent_node((dom_node *)nodo, &p) != DOM_NO_ERR ||
        !p)
        return CSS_OK;
    if (nv_mismo_nom(p, nb))
        h = p;
    else
        dom_node_unref(p);
    *res = h;
    return CSS_OK;
}

static css_error nv_hermano(void *pw, void *nodo, const css_qname *qn,
                            void **res) {
    dom_node *h = 0, *ant = 0;
    char nb[16];
    const uint8_t *p;
    uint32_t m, i;
    (void)pw;
    *res = 0;
    if (!nodo || !qn || !qn->name) return CSS_OK;
    p = lwc_string_data(qn->name);
    m = lwc_string_length(qn->name);
    if (m > 15 || !m) return CSS_OK;
    for (i = 0; i < m; i++) nb[i] = (char)p[i];
    nb[m] = 0;
    if (dom_node_get_previous_sibling((dom_node *)nodo, &ant) !=
        DOM_NO_ERR || !ant)
        return CSS_OK;
    if (nv_mismo_nom(ant, nb))
        h = ant;
    else
        dom_node_unref(ant);
    *res = h;
    return CSS_OK;
}

static css_error nv_hermano_gen(void *pw, void *nodo,
                               const css_qname *qn, void **res) {
    dom_node *h = 0;
    char nb[16];
    const uint8_t *p;
    uint32_t m, i;
    (void)pw;
    *res = 0;
    if (!nodo || !qn || !qn->name) return CSS_OK;
    p = lwc_string_data(qn->name);
    m = lwc_string_length(qn->name);
    if (m > 15 || !m) return CSS_OK;
    for (i = 0; i < m; i++) nb[i] = (char)p[i];
    nb[m] = 0;
    if (dom_node_get_previous_sibling((dom_node *)nodo, &h) !=
        DOM_NO_ERR || !h)
        return CSS_OK;
    for (;;) {
        if (nv_mismo_nom(h, nb)) {
            *res = h;
            return CSS_OK;
        }
        {
            dom_node *s = 0;
            if (dom_node_get_previous_sibling(h, &s) != DOM_NO_ERR ||
                !s) {
                dom_node_unref(h);
                return CSS_OK;
            }
            dom_node_unref(h);
            h = s;
        }
    }
}

static css_error nv_padre_cual(void *pw, void *nodo, void **res) {
    dom_node *h = 0;
    (void)pw;
    *res = 0;
    if (!nodo) return CSS_OK;
    if (dom_node_get_parent_node((dom_node *)nodo, &h) != DOM_NO_ERR ||
        !h)
        return CSS_OK;
    *res = h;
    return CSS_OK;
}

static css_error nv_hermano_cual(void *pw, void *nodo, void **res) {
    dom_node *h = 0;
    (void)pw;
    *res = 0;
    if (!nodo) return CSS_OK;
    if (dom_node_get_previous_sibling((dom_node *)nodo, &h) !=
        DOM_NO_ERR || !h)
        return CSS_OK;
    *res = h;
    return CSS_OK;
}

static css_error nv_tiene_nom(void *pw, void *nodo,
                              const css_qname *qn, bool *ok) {
    char me[16], el[16];
    const uint8_t *p;
    uint32_t m, i;
    (void)pw;
    *ok = false;
    if (!nodo || !qn || !qn->name) return CSS_OK;
    if (nv_nom_el(nodo, me)) return CSS_OK;
    p = lwc_string_data(qn->name);
    m = lwc_string_length(qn->name);
    if (m > 15) return CSS_OK;
    for (i = 0; i < m; i++) el[i] = (char)p[i];
    el[m] = 0;
    *ok = !u_strcmp(me, el);
    return CSS_OK;
}

static css_error nv_tiene_clase(void *pw, void *nodo,
                                lwc_string *nom, bool *ok) {
    char cls[256], want[64];
    const uint8_t *p;
    uint32_t m, i;
    int j;
    (void)pw;
    *ok = false;
    if (!nodo || !nom) return CSS_OK;
    if (nv_atributo((dom_node *)nodo, "class", cls,
                    (int)sizeof(cls)))
        return CSS_OK;
    p = lwc_string_data(nom);
    m = lwc_string_length(nom);
    if (m > 63 || !m) return CSS_OK;
    for (i = 0; i < m; i++) want[i] = (char)p[i];
    want[m] = 0;
    j = 0;
    while (cls[j]) {
        char tok[64];
        int t = 0;
        while (cls[j] == ' ' || cls[j] == '\t') j++;
        if (!cls[j]) break;
        while (cls[j] && cls[j] != ' ' && cls[j] != '\t' && t < 63)
            tok[t++] = cls[j++];
        tok[t] = 0;
        if (t && !u_strcmp(tok, want)) {
            *ok = true;
            return CSS_OK;
        }
    }
    return CSS_OK;
}

static css_error nv_tiene_id(void *pw, void *nodo, lwc_string *nom,
                             bool *ok) {
    char buf[128], want[128];
    const uint8_t *p;
    uint32_t m, i;
    (void)pw;
    *ok = false;
    if (!nodo || !nom) return CSS_OK;
    if (nv_atributo((dom_node *)nodo, "id", buf,
                    (int)sizeof(buf)))
        return CSS_OK;
    p = lwc_string_data(nom);
    m = lwc_string_length(nom);
    if (m > 127) return CSS_OK;
    for (i = 0; i < m; i++) want[i] = (char)p[i];
    want[m] = 0;
    *ok = !u_strcmp(buf, want);
    return CSS_OK;
}

static int nv_compara_atr(dom_node *n, const css_qname *qn,
                          const char *op, lwc_string *val) {
    char buf[256], want[256];
    const uint8_t *p;
    uint32_t m, i, qm;
    const uint8_t *q;
    char nm[16];
    if (!n || !qn || !qn->name || !val || !op) return 0;
    q = lwc_string_data(qn->name);
    qm = lwc_string_length(qn->name);
    if (qm > 15 || !qm) return 0;
    for (i = 0; i < qm; i++) nm[i] = (char)q[i];
    nm[qm] = 0;
    p = lwc_string_data(val);
    m = lwc_string_length(val);
    if (m > 255) return 0;
    for (i = 0; i < m; i++) want[i] = (char)p[i];
    want[m] = 0;
    if (nv_atributo(n, nm, buf, (int)sizeof(buf))) return 0;
    if (!u_strcmp(op, "=")) return !u_strcmp(buf, want);
    if (!u_strcmp(op, "~=")) {
        int j = 0;
        while (buf[j]) {
            char tok[256];
            int t = 0;
            while (buf[j] == ' ' || buf[j] == '\t') j++;
            if (!buf[j]) break;
            while (buf[j] && buf[j] != ' ' && buf[j] != '\t' &&
                   t < 255)
                tok[t++] = buf[j++];
            tok[t] = 0;
            if (t && !u_strcmp(tok, want)) return 1;
        }
        return 0;
    }
    if (!u_strcmp(op, "|=")) {
        int j = 0;
        while (want[j] && buf[j] && want[j] == buf[j]) j++;
        return want[j] == 0 && (buf[j] == 0 || buf[j] == '-');
    }
    if (!u_strcmp(op, "^=")) {
        int j = 0;
        while (want[j] && buf[j] && want[j] == buf[j]) j++;
        return want[j] == 0;
    }
    if (!u_strcmp(op, "$=")) {
        int lb = 0, lw = 0, j;
        while (buf[lb]) lb++;
        while (want[lw]) lw++;
        if (!lw || lw > lb) return 0;
        for (j = 0; j < lw; j++) {
            if (buf[lb - lw + j] != want[j]) return 0;
        }
        return 1;
    }
    if (!u_strcmp(op, "*=")) {
        int lb = 0, lw = 0, j;
        while (buf[lb]) lb++;
        while (want[lw]) lw++;
        if (!lw || lw > lb) return 0;
        for (j = 0; j + lw <= lb; j++) {
            if (!u_memcmp(buf + j, want, (int)lw)) return 1;
        }
        return 0;
    }
    return 0;
}

static css_error nv_atr(void *pw, void *nodo, const css_qname *qn,
                        bool *ok) {
    const uint8_t *p;
    uint32_t m, i;
    char nm[16], probe[2];
    (void)pw;
    *ok = false;
    if (!nodo || !qn || !qn->name) return CSS_OK;
    p = lwc_string_data(qn->name);
    m = lwc_string_length(qn->name);
    if (m > 15 || !m) return CSS_OK;
    for (i = 0; i < m; i++) nm[i] = (char)p[i];
    nm[m] = 0;
    probe[0] = 0;
    *ok = !nv_atributo((dom_node *)nodo, nm, probe, 2);
    return CSS_OK;
}

#define NV_ATR_OP(nom, ops)                                            \
static css_error nv_atr_##nom(void *pw, void *nodo,                    \
                              const css_qname *qn,                    \
                              lwc_string *val, bool *ok) {            \
    (void)pw;                                                         \
    *ok = nv_compara_atr((dom_node *)nodo, qn, ops, val) ?            \
        true : false;                                                 \
    return CSS_OK;                                                    \
}
NV_ATR_OP(eq, "=")
NV_ATR_OP(incl, "~=")
NV_ATR_OP(dash, "|=")
NV_ATR_OP(pref, "^=")
NV_ATR_OP(suf, "$=")
NV_ATR_OP(sub, "*=")

static css_error nv_es_raiz(void *pw, void *nodo, bool *ok) {
    dom_node *h = 0;
    (void)pw;
    *ok = false;
    if (!nodo) return CSS_OK;
    if (dom_node_get_parent_node((dom_node *)nodo, &h) != DOM_NO_ERR)
        return CSS_OK;
    *ok = h == 0 ? true : false;
    if (h) dom_node_unref(h);
    return CSS_OK;
}

static css_error nv_n_hermanos(void *pw, void *nodo, bool mismo,
                               bool despues, int32_t *n) {
    dom_node *padre = 0, *k = 0;
    char me[16];
    int c = 0, pasado = 0;
    (void)pw;
    *n = 0;
    if (!nodo) return CSS_OK;
    if (mismo && nv_nom_el(nodo, me)) return CSS_OK;
    if (dom_node_get_parent_node((dom_node *)nodo, &padre) !=
        DOM_NO_ERR || !padre)
        return CSS_OK;
    if (dom_node_get_first_child(padre, &k) != DOM_NO_ERR || !k) {
        dom_node_unref(padre);
        return CSS_OK;
    }
    for (;;) {
        if (k == (dom_node *)nodo) {
            pasado = 1;
        } else {
            int cuenta = despues ? pasado : !pasado;
            if (cuenta) {
                dom_node_type t;
                if (dom_node_get_node_type(k, &t) == DOM_NO_ERR &&
                    t == DOM_ELEMENT_NODE &&
                    (!mismo || nv_mismo_nom(k, me)))
                    c++;
            }
        }
        {
            dom_node *s = 0;
            if (dom_node_get_next_sibling(k, &s) != DOM_NO_ERR ||
                !s) {
                dom_node_unref(k);
                break;
            }
            dom_node_unref(k);
            k = s;
        }
    }
    dom_node_unref(padre);
    *n = (int32_t)c;
    return CSS_OK;
}

static css_error nv_es_vacio_css(void *pw, void *nodo, bool *ok) {
    dom_node *h = 0;
    (void)pw;
    *ok = false;
    if (!nodo) return CSS_OK;
    if (dom_node_get_first_child((dom_node *)nodo, &h) != DOM_NO_ERR)
        return CSS_OK;
    *ok = h == 0 ? true : false;
    if (h) dom_node_unref(h);
    return CSS_OK;
}

static css_error nv_es_enlace(void *pw, void *nodo, bool *ok) {
    char buf[2];
    (void)pw;
    *ok = false;
    if (!nodo || !nv_mismo_nom(nodo, "a")) return CSS_OK;
    *ok = !nv_atributo((dom_node *)nodo, "href", buf, 2);
    return CSS_OK;
}

static css_error nv_falso(void *pw, void *nodo, bool *ok) {
    (void)pw;
    (void)nodo;
    *ok = false;
    return CSS_OK;
}

static css_error nv_lang(void *pw, void *nodo, lwc_string *lang,
                         bool *ok) {
    (void)pw;
    (void)nodo;
    (void)lang;
    *ok = false;
    return CSS_OK;
}

static css_error nv_sin_hints(void *pw, void *nodo, uint32_t *n,
                              css_hint **h) {
    (void)pw;
    (void)nodo;
    *n = 0;
    *h = 0;
    return CSS_OK;
}

static css_error nv_sin_defecto(void *pw, uint32_t prop,
                                css_hint *hint) {
    (void)pw;
    (void)prop;
    (void)hint;
    return CSS_INVALID;
}

static lwc_string *NV_cls_tab[16];

static css_error nv_clases(void *pw, void *nodo,
                           lwc_string ***clases, uint32_t *n) {
    char buf[256];
    int j = 0;
    uint32_t c = 0;
    (void)pw;
    *clases = 0;
    *n = 0;
    if (!nodo) return CSS_OK;
    /* solo elementos (nv_atributo ya lo exige, pero el scratch
       buf se reutiliza: filtra antes) */
    if (nv_nom_el(nodo, buf)) return CSS_OK;
    if (nv_atributo((dom_node *)nodo, "class", buf,
                    (int)sizeof(buf)))
        return CSS_OK;
    j = 0;
    while (buf[j] && c < 16) {
        char tok[64];
        int t = 0;
        while (buf[j] == ' ' || buf[j] == '\t') j++;
        if (!buf[j]) break;
        while (buf[j] && buf[j] != ' ' && buf[j] != '\t' &&
               t < 63)
            tok[t++] = buf[j++];
        tok[t] = 0;
        if (t) {
            lwc_string *s = 0;
            if (lwc_intern_string(tok, (size_t)t, &s) !=
                lwc_error_ok || !s)
                return CSS_NOMEM;
            NV_cls_tab[c++] = s;
        }
    }
    if (c) {
        *clases = NV_cls_tab;
        *n = c;
    }
    return CSS_OK;
}

static css_error nv_id(void *pw, void *nodo, lwc_string **id) {
    char buf[128];
    int i = 0;
    (void)pw;
    *id = 0;
    if (!nodo) return CSS_OK;
    if (nv_atributo((dom_node *)nodo, "id", buf,
                    (int)sizeof(buf)))
        return CSS_OK;
    while (buf[i]) i++;
    if (!i) return CSS_OK;
    if (lwc_intern_string(buf, (size_t)i, id) != lwc_error_ok)
        return CSS_NOMEM;
    return CSS_OK;
}

static css_select_handler NV_sel;
static int NV_sel_ok = 0;

static void nv_sel_init(void) {
    if (NV_sel_ok) return;
    NV_sel.handler_version = CSS_SELECT_HANDLER_VERSION_1;
    NV_sel.node_name = nv_qname;
    NV_sel.node_classes = nv_clases;
    NV_sel.node_id = nv_id;
    NV_sel.named_ancestor_node = nv_ante;
    NV_sel.named_parent_node = nv_padre;
    NV_sel.named_sibling_node = nv_hermano;
    NV_sel.named_generic_sibling_node = nv_hermano_gen;
    NV_sel.parent_node = nv_padre_cual;
    NV_sel.sibling_node = nv_hermano_cual;
    NV_sel.node_has_name = nv_tiene_nom;
    NV_sel.node_has_class = nv_tiene_clase;
    NV_sel.node_has_id = nv_tiene_id;
    NV_sel.node_has_attribute = nv_atr;
    NV_sel.node_has_attribute_equal = nv_atr_eq;
    NV_sel.node_has_attribute_dashmatch = nv_atr_dash;
    NV_sel.node_has_attribute_includes = nv_atr_incl;
    NV_sel.node_has_attribute_prefix = nv_atr_pref;
    NV_sel.node_has_attribute_suffix = nv_atr_suf;
    NV_sel.node_has_attribute_substring = nv_atr_sub;
    NV_sel.node_is_root = nv_es_raiz;
    NV_sel.node_count_siblings = nv_n_hermanos;
    NV_sel.node_is_empty = nv_es_vacio_css;
    NV_sel.node_is_link = nv_es_enlace;
    NV_sel.node_is_visited = nv_falso;
    NV_sel.node_is_hover = nv_falso;
    NV_sel.node_is_active = nv_falso;
    NV_sel.node_is_focus = nv_falso;
    NV_sel.node_is_enabled = nv_falso;
    NV_sel.node_is_disabled = nv_falso;
    NV_sel.node_is_checked = nv_falso;
    NV_sel.node_is_target = nv_falso;
    NV_sel.node_is_lang = nv_lang;
    NV_sel.node_presentational_hint = nv_sin_hints;
    NV_sel.ua_default_for_property = nv_sin_defecto;
    NV_sel.set_libcss_node_data = nv_set_dato;
    NV_sel.get_libcss_node_data = nv_get_dato;
    NV_sel_ok = 1;
}

/* ---- contexto CSS global ---- */
static css_select_ctx *NV_ctx = 0;
static css_stylesheet *NV_hoja = 0;
static css_unit_ctx NV_uctx;
static css_media NV_media;
static int NV_css_ok = 0;

static css_error nv_css_res(void *pw, const char *base,
                            lwc_string *rel, lwc_string **abs) {
    (void)pw;
    (void)base;
    *abs = lwc_string_ref(rel);
    return CSS_OK;
}

static void nv_css_fin(void) {
    if (NV_ctx) {
        css_select_ctx_destroy(NV_ctx);
        NV_ctx = 0;
    }
    if (NV_hoja) {
        css_stylesheet_destroy(NV_hoja);
        NV_hoja = 0;
    }
    NV_css_ok = 0;
}

/* crea ctx + hoja desde el <style> capturado (puede estar vacio) */
static int nv_css_ini(const char *css, int n) {
    css_stylesheet_params p;
    css_error e;
    nv_css_fin();
    if (css_select_ctx_create(&NV_ctx) != CSS_OK || !NV_ctx)
        return -1;
    p.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
    p.level = CSS_LEVEL_21;
    p.charset = "UTF-8";
    p.url = "http://navega/";
    p.title = 0;
    p.allow_quirks = false;
    p.inline_style = false;
    p.resolve = nv_css_res;
    p.resolve_pw = 0;
    p.import = 0;
    p.import_pw = 0;
    p.color = 0;
    p.color_pw = 0;
    p.font = 0;
    p.font_pw = 0;
    if (css_stylesheet_create(&p, &NV_hoja) != CSS_OK || !NV_hoja) {
        nv_css_fin();
        return -1;
    }
    if (n > 0 && css) {
        e = css_stylesheet_append_data(NV_hoja,
                                       (const uint8_t *)css,
                                       (size_t)n);
        if (e != CSS_OK && e != CSS_NEEDDATA) {
            nv_css_fin();
            return -1;
        }
    }
    if (css_stylesheet_data_done(NV_hoja) != CSS_OK) {
        nv_css_fin();
        return -1;
    }
    if (css_select_ctx_append_sheet(NV_ctx, NV_hoja,
                                    CSS_ORIGIN_AUTHOR,
                                    "screen") != CSS_OK) {
        nv_css_fin();
        return -1;
    }
    {
        int i;
        uint8_t *b = (uint8_t *)&NV_uctx;
        for (i = 0; i < (int)sizeof(NV_uctx); i++) b[i] = 0;
        b = (uint8_t *)&NV_media;
        for (i = 0; i < (int)sizeof(NV_media); i++) b[i] = 0;
    }
    NV_uctx.viewport_width = INTTOFIX(640);
    NV_uctx.viewport_height = INTTOFIX(460);
    NV_uctx.font_size_default = INTTOFIX(16);
    NV_uctx.font_size_minimum = 0;
    NV_uctx.device_dpi = INTTOFIX(96);
    NV_uctx.root_style = 0;
    NV_uctx.pw = 0;
    /* measure es const: ya quedo a 0 con el bucle de arriba */
    NV_media.type = CSS_MEDIA_SCREEN;
    NV_media.width = INTTOFIX(640);
    NV_media.height = INTTOFIX(460);
    NV_css_ok = 1;
    return 0;
}

/* lee color/fondo del estilo calculado; -1 si no fijado */
static void nv_colores(css_computed_style *st, int *fg, int *bg) {
    css_color c;
    *fg = -1;
    *bg = -1;
    if (!st) return;
    if (css_computed_color(st, &c) == CSS_COLOR_COLOR)
        *fg = (int)(c & 0xFFFFFF);
    if (css_computed_background_color(st, &c) ==
        CSS_BACKGROUND_COLOR_COLOR)
        *bg = (int)(c & 0xFFFFFF);
}

/* ---- layout de texto ---- */
static char NV_titulo[NV_MAXTITULO];
static int NV_fg = -1, NV_bg = -1;
static int NV_cols = 76;
static int NV_pend_esp = 0;

/* enlaces (Hito 4): filas [f0,f1] -> URL navegable */
#define NV_MAXENL 64
typedef struct {
    int f0, f1;
    char url[256];
} nv_enl_t;
static nv_enl_t NV_enl[NV_MAXENL];
static int NV_nenl = 0, NV_foco_enl = -1;

/* historial (Hito 4): pila de URLs con scroll por entrada */
#define NV_MAXHIST 16
static char NV_hist[NV_MAXHIST][NV_MAXURL + 1];
static int NV_hscroll[NV_MAXHIST];
static int NV_nhist = 0, NV_ihist = -1;

/* base para resolver relativas + ultimo resultado de red */
static char NV_base[NV_MAXURL + 1];
static char NV_tmpurl[NV_MAXURL + 1];
static char NV_red[64];
static int NV_ult_cod = 0, NV_ult_tam = 0;

/* ---- imagenes (Hito 5): <img> BMP/GIF, pixeles en heap ---- */
#define NV_MAXIMG 8
#define NV_IMG_MAXPX 150000    /* tope de decodificacion */
typedef struct {
    int w, h;
    unsigned char *px;         /* RGB, w*h*3 (u_malloc) o 0 */
    int fila;                  /* primera fila de texto que ocupa */
} nv_img_t;
static nv_img_t NV_img[NV_MAXIMG];
static int NV_nimg = 0;
static int NV_ninline = 0;     /* elementos con style= aplicado */

static void nv_linea_nueva(void) {
    if (NV_nfilas >= NV_MAXFILAS) return;
    NV_filas[NV_nfilas].ini = NV_ntxt;
    NV_filas[NV_nfilas].len = 0;
    NV_filas[NV_nfilas].fg = NV_fg;
    NV_filas[NV_nfilas].bg = NV_bg;
    NV_filas[NV_nfilas].estilo = 0;
    NV_filas[NV_nfilas].img = -1;
}

static void nv_salto(void) {
    if (NV_nfilas >= NV_MAXFILAS) return;
    NV_pend_esp = 0;
    NV_filas[NV_nfilas].len = NV_ntxt - NV_filas[NV_nfilas].ini;
    NV_nfilas++;
    nv_linea_nueva();
}

/* bloque: si la linea actual tiene texto, la cierra */
static void nv_bloque(void) {
    if (NV_nfilas >= NV_MAXFILAS) return;
    NV_pend_esp = 0;
    if (NV_ntxt > NV_filas[NV_nfilas].ini)
        nv_salto();
    else {
        NV_filas[NV_nfilas].fg = NV_fg;
        NV_filas[NV_nfilas].bg = NV_bg;
    }
}

static int nv_ancho_linea(void) {
    return NV_ntxt - NV_filas[NV_nfilas].ini;
}

/* vuelca un byte al buffer (con wrap); devuelve 0 si no cabe */
static void nv_pon_byte(unsigned char c) {
    if (NV_nfilas >= NV_MAXFILAS) return;
    if (c == '\n') {
        nv_salto();
        return;
    }
    if (NV_ntxt >= NV_MAXTEXTO - 1) return;
    if (nv_ancho_linea() >= NV_cols)
        nv_salto();
    if (NV_nfilas >= NV_MAXFILAS) return;
    NV_txt[NV_ntxt++] = c;
}

/* texto normal: colapsa blancos, respeta wrap.
   El espacio pendiente es global: sobrevive a los limites entre
   nodos en linea ("de "<b>"prueba" -> "de prueba"). */
static void nv_pon_texto(const unsigned char *p, int n, int pre) {
    int i;
    for (i = 0; i < n; i++) {
        unsigned char c = p[i];
        if (c == '\r') continue;
        if (pre) {
            if (c == '\t') c = ' ';
            NV_pend_esp = 0;
            nv_pon_byte(c == '\n' ? (unsigned char)'\n' : c);
            continue;
        }
        if (c == '\n' || c == '\t' || c == ' ') {
            NV_pend_esp = 1;
            continue;
        }
        if (NV_pend_esp) {
            NV_pend_esp = 0;
            if (nv_ancho_linea() > 0)
                nv_pon_byte(' ');
        }
        nv_pon_byte(c);
    }
}

/* id de etiqueta desde nombre en minusculas */
static int nv_id_nom(const char *s) {
    char b[16];
    int i = 0;
    while (s[i] && i < 15) {
        char c = s[i];
        b[i] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
        i++;
    }
    b[i] = 0;
    return nv_tag_id(b, i);
}

/* Hito 4: resuelve href contra NV_base en sal[512]. 0 ok, -1 no. */
static int nv_resuelve(const char *ref, char *sal) {
    int i;
    if (!ref || !sal || !ref[0]) return -1;
    /* https absoluta: el resolver de ulib solo acepta http */
    if ((ref[0] == 'h' || ref[0] == 'H') &&
        (ref[1] == 't' || ref[1] == 'T') &&
        (ref[2] == 't' || ref[2] == 'T') &&
        (ref[3] == 'p' || ref[3] == 'P') &&
        (ref[4] == 's' || ref[4] == 'S') &&
        ref[5] == ':' && ref[6] == '/' && ref[7] == '/') {
        i = 0;
        while (ref[i] && ref[i] != '#' && i < 511) {
            sal[i] = ref[i];
            i++;
        }
        sal[i] = 0;
        if (i <= 8) return -1;
        {
            int j = 8;
            while (sal[j] == '/') j++;
            if (!sal[j]) return -1;
        }
        return 0;
    }
    if (!NV_base[0]) return -1;
    if (u_html_resuelve_url(NV_base, ref, sal)) return -1;
    return 0;
}

/* Hito 4: indice del enlace que cubre la fila f, o -1.
   Del ultimo al primero: el mas interno gana. */
static int nv_enlace_en(int f) {
    int i;
    for (i = NV_nenl - 1; i >= 0; i--) {
        if (f >= NV_enl[i].f0 && f <= NV_enl[i].f1)
            return i;
    }
    return -1;
}

/* nv_es_url vive abajo (carga); avance para nv_imagen */
static int nv_es_url(const char *s);

/* ---- imagenes (Hito 5): decodificacion BMP/GIF a heap ---- */
static void nv_img_libera(void) {
    int i;
    for (i = 0; i < NV_nimg; i++) {
        if (NV_img[i].px) u_free(NV_img[i].px);
        NV_img[i].px = 0;
    }
    NV_nimg = 0;
}

static void *nv_bmp_nuevo(int w, int h, unsigned int estado) {
    (void)estado;
    if (w <= 0 || h <= 0 ||
        (unsigned)w * (unsigned)h > (unsigned)NV_IMG_MAXPX)
        return 0;
    return u_malloc((unsigned)(w * h * 4));
}

static void nv_bmp_borra(void *b) {
    u_free(b);
}

static unsigned char *nv_bmp_buf(void *b) {
    return (unsigned char *)b;
}

static void *nv_gif_nuevo(int w, int h) {
    if (w <= 0 || h <= 0 ||
        (unsigned)w * (unsigned)h > (unsigned)NV_IMG_MAXPX)
        return 0;
    return u_malloc((unsigned)(w * h * 4));
}

static void nv_gif_borra(nsgif_bitmap_t *b) {
    u_free(b);
}

static uint8_t *nv_gif_buf(nsgif_bitmap_t *b) {
    return (uint8_t *)b;
}

/* BMP ("BM") o GIF ("GIF8") a RGB en heap. 0 ok. */
static int nv_img_dec(const unsigned char *d, int n,
                      int *w, int *h, unsigned char **rgb) {
    int i;
    *w = 0;
    *h = 0;
    *rgb = 0;
    if (!d || n < 6) return -1;
    if (d[0] == 'B' && d[1] == 'M') {
        bmp_image img;
        bmp_bitmap_callback_vt vt;
        uint32_t *src;
        unsigned char *dst;
        for (i = 0; i < (int)sizeof(vt); i++)
            ((char *)&vt)[i] = 0;
        vt.bitmap_create = nv_bmp_nuevo;
        vt.bitmap_destroy = nv_bmp_borra;
        vt.bitmap_get_buffer = nv_bmp_buf;
        if (bmp_create(&img, &vt) != BMP_OK) return -1;
        if (bmp_analyse(&img, (size_t)n, (uint8_t *)d) !=
            BMP_OK) {
            bmp_finalise(&img);
            return -1;
        }
        if (img.width < 1 || img.height < 1 ||
            img.width > 2000 || img.height > 2000 ||
            (unsigned)img.width * (unsigned)img.height >
            (unsigned)NV_IMG_MAXPX) {
            bmp_finalise(&img);
            return -1;
        }
        if (bmp_decode(&img) != BMP_OK || !img.decoded ||
            !img.bitmap) {
            bmp_finalise(&img);
            return -1;
        }
        dst = (unsigned char *)u_malloc(
            (unsigned)(img.width * img.height * 3));
        if (!dst) {
            bmp_finalise(&img);
            return -1;
        }
        src = (uint32_t *)img.bitmap;
        for (i = 0; i < (int)(img.width * img.height); i++) {
            uint32_t p = src[i];
            dst[i * 3] = (unsigned char)(p & 0xFF);
            dst[i * 3 + 1] = (unsigned char)((p >> 8) & 0xFF);
            dst[i * 3 + 2] = (unsigned char)((p >> 16) & 0xFF);
        }
        *w = (int)img.width;
        *h = (int)img.height;
        *rgb = dst;
        bmp_finalise(&img);
        return 0;
    }
    if (d[0] == 'G' && d[1] == 'I' && d[2] == 'F') {
        nsgif_t *g = 0;
        nsgif_bitmap_cb_vt vt;
        const nsgif_info_t *inf;
        nsgif_bitmap_t *bm = 0;
        uint8_t *src;
        unsigned char *dst;
        for (i = 0; i < (int)sizeof(vt); i++)
            ((char *)&vt)[i] = 0;
        vt.create = nv_gif_nuevo;
        vt.destroy = nv_gif_borra;
        vt.get_buffer = nv_gif_buf;
        vt.set_opaque = 0;
        vt.test_opaque = 0;
        vt.modified = 0;
        if (nsgif_create(&vt, NSGIF_BITMAP_FMT_R8G8B8A8,
                         &g) != NSGIF_OK || !g)
            return -1;
        if (nsgif_data_scan(g, (uint32_t)n, d) != NSGIF_OK) {
            nsgif_destroy(g);
            return -1;
        }
        nsgif_data_complete(g);
        inf = nsgif_get_info(g);
        if (!inf || inf->width < 1 || inf->height < 1 ||
            inf->frame_count < 1 ||
            (unsigned)inf->width * (unsigned)inf->height >
            (unsigned)NV_IMG_MAXPX) {
            nsgif_destroy(g);
            return -1;
        }
        if (nsgif_frame_decode(g, 0, &bm) != NSGIF_OK || !bm) {
            nsgif_destroy(g);
            return -1;
        }
        dst = (unsigned char *)u_malloc(
            (unsigned)(inf->width * inf->height * 3));
        if (!dst) {
            nsgif_destroy(g);
            return -1;
        }
        src = nv_gif_buf(bm);
        for (i = 0; i < (int)(inf->width * inf->height); i++) {
            dst[i * 3] = src[i * 4];
            dst[i * 3 + 1] = src[i * 4 + 1];
            dst[i * 3 + 2] = src[i * 4 + 2];
        }
        *w = (int)inf->width;
        *h = (int)inf->height;
        *rgb = dst;
        nsgif_destroy(g);
        return 0;
    }
    /* PNG/JPEG/otros: sin decodificador (documentado) */
    return -1;
}

/* vuelca <img>: filas de pixeles o texto alt */
static void nv_imagen(dom_node *h) {
    char src[256], alt[128];
    int i, dw, dh, R;
    nv_bloque();
    if (nv_atributo(h, "src", src, (int)sizeof(src))) {
        if (!nv_atributo(h, "alt", alt, (int)sizeof(alt)) &&
            alt[0])
            nv_pon_texto((const unsigned char *)alt,
                         u_strlen(alt), 0);
        return;
    }
    if (!nv_resuelve(src, NV_tmpurl) && nv_es_url(NV_tmpurl)) {
        nsfetch_res_t r;
        if (nsfetch_get(NV_tmpurl, &r) == NSFETCH_OK &&
            r.codigo == 200 && r.tam > 0) {
            int w = 0, hh = 0;
            unsigned char *rgb = 0;
            if (!nv_img_dec((const unsigned char *)r.cuerpo,
                            r.tam, &w, &hh, &rgb) &&
                NV_nimg < NV_MAXIMG) {
                int slot = NV_nimg++;
                NV_img[slot].w = w;
                NV_img[slot].h = hh;
                NV_img[slot].px = rgb;
                NV_img[slot].fila = NV_nfilas;
                dw = w <= 600 ? w : 600;
                dh = (hh * dw) / w;
                if (dh < 1) dh = 1;
                R = (dh + 15) / 16;
                for (i = 0; i < R; i++) {
                    if (NV_nfilas >= NV_MAXFILAS) break;
                    NV_filas[NV_nfilas].estilo = 3;
                    NV_filas[NV_nfilas].img = slot;
                    nv_salto();
                }
                return;
            }
        }
    }
    if (!nv_atributo(h, "alt", alt, (int)sizeof(alt)) && alt[0])
        nv_pon_texto((const unsigned char *)alt,
                     u_strlen(alt), 0);
}

/* aplica el estilo CSS del nodo; actualiza NV_fg/NV_bg */
static void nv_estilo_nodo(dom_node *n) {
    css_select_results *res = 0;
    if (!NV_css_ok || !n) return;
    if (css_select_style(NV_ctx, n, &NV_uctx, &NV_media, 0,
                         &NV_sel, 0, &res) != CSS_OK || !res)
        return;
    if (res->styles[CSS_PSEUDO_ELEMENT_NONE]) {
        int fg = -1, bg = -1;
        nv_colores(res->styles[CSS_PSEUDO_ELEMENT_NONE],
                   &fg, &bg);
        if (fg >= 0) {
            NV_fg = fg;
            NV_nestilos++;
        }
        if (bg >= 0) {
            NV_bg = bg;
            NV_nestilos++;
        }
    }
    css_select_results_destroy(res);
    /* Hito 5: atributo style= como hoja inline (pesa mas que la
       hoja <style>; mismo subconjunto: color/fondo) */
    {
        char st[512];
        if (!nv_atributo(n, "style", st, (int)sizeof(st)) &&
            st[0]) {
            css_stylesheet_params p;
            css_stylesheet *hoja = 0;
            p.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
            p.level = CSS_LEVEL_21;
            p.charset = "UTF-8";
            p.url = "http://navega/";
            p.title = 0;
            p.allow_quirks = false;
            p.inline_style = true;
            p.resolve = nv_css_res;
            p.resolve_pw = 0;
            p.import = 0;
            p.import_pw = 0;
            p.color = 0;
            p.color_pw = 0;
            p.font = 0;
            p.font_pw = 0;
            if (css_stylesheet_create(&p, &hoja) == CSS_OK &&
                hoja) {
                css_error e = css_stylesheet_append_data(
                    hoja, (const uint8_t *)st,
                    (size_t)u_strlen(st));
                if ((e == CSS_OK || e == CSS_NEEDDATA) &&
                    css_stylesheet_data_done(hoja) == CSS_OK) {
                    css_select_results *ir = 0;
                    if (css_select_style(NV_ctx, n, &NV_uctx,
                            &NV_media, hoja, &NV_sel, 0,
                            &ir) == CSS_OK && ir) {
                        if (ir->styles[
                                CSS_PSEUDO_ELEMENT_NONE]) {
                            int fg = -1, bg = -1;
                            nv_colores(
                                ir->styles[
                                    CSS_PSEUDO_ELEMENT_NONE],
                                &fg, &bg);
                            if (fg >= 0) {
                                NV_fg = fg;
                                NV_ninline++;
                            }
                            if (bg >= 0) {
                                NV_bg = bg;
                                NV_ninline++;
                            }
                        }
                        css_select_results_destroy(ir);
                    }
                }
                css_stylesheet_destroy(hoja);
            }
        }
    }
}

static void nv_recorre(dom_node *n, int prof) {
    dom_node *h = 0;
    if (!n || prof > 32) return;
    if (dom_node_get_first_child(n, &h) != DOM_NO_ERR || !h)
        return;
    for (;;) {
        dom_node_type t = (dom_node_type)0;
        if (dom_node_get_node_type(h, &t) == DOM_NO_ERR) {
            if (t == DOM_ELEMENT_NODE) {
                char nb[16];
                int id;
                if (!nv_nom_el(h, nb)) {
                    id = nv_id_nom(nb);
                    if (id == T_SCRIPT || id == T_STYLE ||
                        id == T_TITLE || id == T_HEAD ||
                        id == T_META || id == T_LINK) {
                        /* sin contenido visible */
                    } else if (nv_es_vacio(id)) {
                        if (id == T_BR)
                            nv_salto();
                        else if (id == T_HR) {
                            nv_bloque();
                            if (NV_nfilas < NV_MAXFILAS) {
                                NV_filas[NV_nfilas].estilo = 2;
                                nv_salto();
                            }
                        } else if (id == T_IMG) {
                            nv_imagen(h);
                        }
                    } else {
                        int fg0 = NV_fg, bg0 = NV_bg;
                        int es_enl = 0, ienl = -1, ntxt0 = 0;
                        nv_estilo_nodo(h);
                        /* Hito 4: registra el enlace (filas+f0..f1) */
                        if (id == T_A) {
                            char href[256];
                            if (!nv_atributo(h, "href", href,
                                             (int)sizeof(href)) &&
                                !nv_resuelve(href, NV_tmpurl) &&
                                NV_nenl < NV_MAXENL) {
                                int q = 0;
                                ienl = NV_nenl++;
                                NV_enl[ienl].f0 = NV_nfilas;
                                NV_enl[ienl].f1 = NV_nfilas;
                                while (NV_tmpurl[q] && q < 255) {
                                    NV_enl[ienl].url[q] =
                                        NV_tmpurl[q];
                                    q++;
                                }
                                NV_enl[ienl].url[q] = 0;
                                es_enl = 1;
                                ntxt0 = NV_ntxt;
                            }
                        }
                        if (id == T_LI) {
                            nv_bloque();
                            nv_pon_texto(
                                (const unsigned char *)"  * ",
                                4, 1);
                        } else if (nv_es_bloque(id)) {
                            nv_bloque();
                        }
                        {
                            int fg1 = NV_fg, bg1 = NV_bg;
                            nv_recorre(h, prof + 1);
                            NV_fg = fg1;
                            NV_bg = bg1;
                        }
                        if (nv_es_bloque(id))
                            nv_salto();
                        if (es_enl && ienl >= 0) {
                            int f1 = NV_nfilas;
                            if (NV_nfilas >= NV_MAXFILAS)
                                f1 = NV_MAXFILAS - 1;
                            if (NV_ntxt ==
                                NV_filas[f1].ini &&
                                f1 > NV_enl[ienl].f0)
                                f1--;
                            if (NV_ntxt == ntxt0) {
                                /* vacio: anula el registro */
                                NV_nenl--;
                            } else {
                                NV_enl[ienl].f1 = f1;
                            }
                        }
                        NV_fg = fg0;
                        NV_bg = bg0;
                        {
                            /* la linea en curso hereda el estilo
                               exterior restaurado */
                            if (NV_nfilas < NV_MAXFILAS) {
                                NV_filas[NV_nfilas].fg = NV_fg;
                                NV_filas[NV_nfilas].bg = NV_bg;
                            }
                        }
                    }
                }
            } else if (t == DOM_TEXT_NODE) {
                dom_string *ds = 0;
                if (dom_characterdata_get_data(
                        (dom_characterdata *)h,
                        &ds) == DOM_NO_ERR && ds) {
                    const uint8_t *p = dom_string_data(ds);
                    uint32_t m = dom_string_length(ds);
                    if (p && m) {
                        int pre = 0;
                        /* PRE conserva saltos */
                        {
                            dom_node *pa = 0;
                            if (dom_node_get_parent_node(
                                    h, &pa) == DOM_NO_ERR &&
                                pa) {
                                char pb[16];
                                if (!nv_nom_el(pa, pb) &&
                                    !u_strcmp(pb, "pre"))
                                    pre = 1;
                                dom_node_unref(pa);
                            }
                        }
                        nv_pon_texto(p, (int)m, pre);
                    }
                    dom_string_unref(ds);
                }
            }
        }
        {
            dom_node *s = 0;
            if (dom_node_get_next_sibling(h, &s) != DOM_NO_ERR ||
                !s) {
                dom_node_unref(h);
                break;
            }
            dom_node_unref(h);
            h = s;
        }
    }
}

static void nv_hubbub_err(uint32_t lin, uint32_t col,
                          const char *msg, void *pw) {
    (void)lin;
    (void)col;
    (void)msg;
    (void)pw;
}

/* puente hubbub->dom global (evita 8K en pila) */
static nv_t NV_pu;

/* procesa un documento completo; devuelve 0 ok */
static int nv_procesa(const unsigned char *pg, int n) {
    dom_document *doc = 0;
    hubbub_parser *ps = 0;
    hubbub_parser_optparams prm;
    hubbub_error r;
    NV_ntxt = 0;
    NV_nfilas = 0;
    NV_nestilos = 0;
    NV_titulo[0] = 0;
    NV_ntab = 0;
    NV_fg = -1;
    NV_bg = -1;
    NV_cols = 76;
    NV_pend_esp = 0;
    NV_nenl = 0;
    NV_ninline = 0;
    nv_img_libera();
    NV_css_ok = 0;
    nv_linea_nueva();
    if (!pg || n < 0) return -1;
    if (dom_implementation_create_document(
            DOM_IMPLEMENTATION_HTML, NULL, NULL, NULL, NULL, NULL,
            &doc) != DOM_NO_ERR || !doc)
        return -2;
    {
        int i;
        uint8_t *b = (uint8_t *)&NV_pu;
        for (i = 0; i < (int)sizeof(NV_pu); i++) b[i] = 0;
    }
    NV_pu.doc = doc;
    NV_pu.npila = 0;
    NV_pu.salta = -1;
    NV_pu.estilo[0] = 0;
    NV_pu.nestilo = 0;
    NV_pu.titulo[0] = 0;
    NV_pu.ntitulo = 0;
    nv_empuja(&NV_pu, (dom_node *)doc, T_OTRO);
    r = hubbub_parser_create(0, false, &ps);
    if (r != HUBBUB_OK || !ps) {
        dom_node_unref(doc);
        return -2;
    }
    NV_pu.ps = ps;
    prm.token_handler.handler = nv_tok;
    prm.token_handler.pw = &NV_pu;
    r = hubbub_parser_setopt(ps, HUBBUB_PARSER_TOKEN_HANDLER,
                             &prm);
    if (r == HUBBUB_OK) {
        prm.error_handler.handler = nv_hubbub_err;
        prm.error_handler.pw = &NV_pu;
        r = hubbub_parser_setopt(ps, HUBBUB_PARSER_ERROR_HANDLER,
                                 &prm);
    }
    if (r == HUBBUB_OK && n > 0)
        r = hubbub_parser_parse_chunk(ps, pg, (size_t)n);
    if (r == HUBBUB_OK)
        r = hubbub_parser_completed(ps);
    hubbub_parser_destroy(ps);
    NV_pu.ps = 0;
    /* titulo */
    {
        int i = 0;
        while (NV_pu.titulo[i] && i < NV_MAXTITULO - 1) {
            NV_titulo[i] = NV_pu.titulo[i];
            i++;
        }
        NV_titulo[i] = 0;
    }
    /* CSS del <style> (si falla, sigue sin estilos) */
    nv_sel_init();
    nv_css_ini(NV_pu.nestilo ? NV_pu.estilo : 0,
               NV_pu.nestilo);
    /* vuelca el arbol a filas de texto */
    nv_recorre((dom_node *)doc, 0);
    /* cierra la linea en curso */
    if (NV_nfilas < NV_MAXFILAS) {
        NV_filas[NV_nfilas].len =
            NV_ntxt - NV_filas[NV_nfilas].ini;
        if (NV_filas[NV_nfilas].len == 0 &&
            NV_filas[NV_nfilas].estilo == 0)
            NV_filas[NV_nfilas].estilo = 1;
        NV_nfilas++;
    }
    dom_node_unref(doc);
    return r == HUBBUB_OK ? 0 : -3;
}

/* ---- carga: URL (nsfetch) o fichero local ---- */
static int nv_es_url(const char *s) {
    if (!s) return 0;
    if (s[0] == 'h' && s[1] == 't' && s[2] == 't' &&
        s[3] == 'p') {
        if (s[4] == ':' && s[5] == '/' && s[6] == '/')
            return 1;
        if (s[4] == 's' && s[5] == ':' && s[6] == '/' &&
            s[7] == '/')
            return 1;
    }
    return 0;
}

/* -p: vuelca una linea de texto a consola (hasta 200 bytes) */
static void nv_linea_consola(int f) {
    int i, n;
    char b[208];
    if (f < 0 || f >= NV_nfilas) return;
    n = NV_filas[f].len;
    if (n > 200) n = 200;
    for (i = 0; i < n; i++)
        b[i] = (char)NV_txt[NV_filas[f].ini + i];
    b[n] = '\n';
    b[n + 1] = 0;
    u_print(b);
}

static void nv_num(int v) {
    char b[16];
    int i = 0, j;
    if (v == 0) {
        u_print("0");
        return;
    }
    if (v < 0) {
        u_print("-");
        v = -v;
    }
    while (v > 0 && i < 14) {
        b[i++] = (char)('0' + v % 10);
        v /= 10;
    }
    for (j = i - 1; j >= 0; j--) {
        char c[2];
        c[0] = b[j];
        c[1] = 0;
        u_print(c);
    }
}

/* carga fuente y procesa; deja filas listas. rc: 0 ok */
static int nv_cargar(const char *f) {
    const unsigned char *pg = 0;
    int n = 0, rc;
    if (!f || !f[0]) return -1;
    if (nv_es_url(f)) {
        nsfetch_res_t r;
        int e = nsfetch_get(f, &r);
        if (e != NSFETCH_OK) return -10 - e;
        NV_ult_cod = r.codigo;
        NV_ult_tam = r.tam;
        if (r.codigo != 200) return -20;
        pg = (const unsigned char *)r.cuerpo;
        n = r.tam;
        rc = nv_procesa(pg, n);
        nv_css_fin();
        return rc;
    }
    /* fichero local: reusa NV_txt como bufer de entrada (64K) */
    {
        UFILE *fp = u_fopen(f, "r");
        int tam, leido;
        if (!fp) return -30;
        tam = u_fsize(fp);
        if (tam < 0) {
            u_fclose(fp);
            return -30;
        }
        if (tam > NV_MAXTEXTO) {
            u_fclose(fp);
            return -31;
        }
        leido = u_fread(NV_txt, tam, fp);
        u_fclose(fp);
        if (leido != tam) return -32;
        NV_ult_cod = 0;
        NV_ult_tam = tam;
        rc = nv_procesa(NV_txt, tam);
        nv_css_fin();
        return rc;
    }
}

/* ---- autotests -p (consola, sin ventana) ---- */
static int NV_pruebas = 0, NV_fallos = 0;

static void nv_ok(int cond, const char *nom) {
    NV_pruebas++;
    if (cond) {
        u_print("ok - ");
        u_print(nom);
        u_print("\n");
    } else {
        NV_fallos++;
        u_print("FALLO - ");
        u_print(nom);
        u_print("\n");
    }
}

static int nv_contiene(const char *aguja) {
    int m = 0, i;
    while (aguja[m]) m++;
    if (!m || m > NV_ntxt) return 0;
    for (i = 0; i + m <= NV_ntxt; i++) {
        if (!u_memcmp((const char *)NV_txt + i, aguja, m))
            return 1;
    }
    return 0;
}

static int nv_pagina(const char *url) {
    int rc = nv_cargar(url);
    if (rc) return rc;
    return 0;
}

/* navegar e historial viven abajo (tras el layout); avance para -p */
static void nv_navegar(const char *f, int empuja);
static void nv_hist_ir(int d);
static int NV_tiene;

/* -p [url]: sin url = bateria contra stubs; con url = resumen */
static int nv_modo_p(const char *resto) {
    while (*resto == ' ') resto++;
    if (*resto == 0) {
        /* bateria: necesita http_stub en 18080 (ftest lo arranca) */
        u_print("navega: bateria Hito 5...\n");
        nv_ok(nv_pagina("http://10.0.2.2:18080/pagina") == 0,
              "pagina-carga");
        nv_ok(!u_strcmp(NV_titulo, "Inicio Facil"),
              "pagina-titulo");
        nv_ok(NV_nfilas >= 4, "pagina-filas");
        nv_ok(nv_contiene("FacilBrowser"), "pagina-texto");
        nv_ok(nv_contiene("Segunda"), "pagina-entidad");
        nv_ok(nv_pagina("http://10.0.2.2:18080/css") == 0,
              "css-carga");
        nv_ok(!u_strcmp(NV_titulo, "CSS"), "css-titulo");
        nv_ok(NV_nestilos >= 2, "css-estilos");
        nv_ok(nv_contiene("Verde por clase"), "css-texto");
        /* Hito 4: enlaces (misma pagina que el stub /enlaces) */
        nv_navegar("http://10.0.2.2:18080/enlaces", 1);
        nv_ok(NV_tiene, "enl-carga");
        nv_ok(NV_nenl >= 5, "enl-num");
        nv_ok(!u_strcmp(NV_enl[0].url,
                        "http://10.0.2.2:18080/pagina"), "enl-abs");
        nv_ok(!u_strcmp(NV_enl[1].url,
                        "http://10.0.2.2:18080/pagina"), "enl-raiz");
        nv_ok(!u_strcmp(NV_enl[2].url,
                        "http://10.0.2.2:18080/pagina"), "enl-rel");
        nv_ok(!u_strcmp(NV_enl[5].url, "https://example.com/"),
              "enl-https");
        /* Hito 4: historial atras/adelante */
        nv_navegar("http://10.0.2.2:18080/css", 1);
        nv_ok(NV_tiene, "hist-carga");
        nv_hist_ir(-1);
        nv_ok(!u_strcmp(NV_titulo, "Enlaces"), "hist-atras");
        nv_hist_ir(1);
        nv_ok(!u_strcmp(NV_titulo, "CSS"), "hist-adelante");
        /* Hito 5: imagenes BMP + GIF (pagina /img-bmp) */
        nv_navegar("http://10.0.2.2:18080/img-bmp", 1);
        nv_ok(NV_tiene, "img-carga");
        nv_ok(NV_nimg >= 2, "img-num");
        nv_ok(NV_nimg >= 1 && NV_img[0].w == 48 &&
              NV_img[0].h == 32, "img-bmp");
        nv_ok(NV_nimg >= 2 && NV_img[1].w == 48 &&
              NV_img[1].h == 32, "img-gif");
        nv_ok(NV_nimg >= 2 && NV_img[0].px != 0 &&
              NV_img[1].px != 0, "img-px");
        /* Hito 5: style= en linea (pagina /css) */
        nv_navegar("http://10.0.2.2:18080/css", 1);
        nv_ok(NV_ninline >= 1, "inl-num");
        {
            int f, k, ok = 0;
            for (f = 0; f < NV_nfilas; f++) {
                int m = NV_filas[f].len;
                for (k = 0; k + 8 <= m; k++) {
                    if (!u_memcmp((const char *)NV_txt +
                                  NV_filas[f].ini + k,
                                  "Amarillo", 8)) {
                        if (NV_filas[f].fg == 0xFFFF00)
                            ok = 1;
                    }
                }
            }
            nv_ok(ok, "inl-color");
        }
        {
            int f;
            u_print("navega: titulo=");
            u_print(NV_titulo);
            u_print(" filas=");
            nv_num(NV_nfilas);
            u_print(" estilos=");
            nv_num(NV_nestilos);
            u_print("\n");
            for (f = 0; f < NV_nfilas && f < 8; f++)
                nv_linea_consola(f);
        }
        u_print("navega: ");
        nv_num(NV_pruebas);
        u_print(" pruebas ");
        nv_num(NV_fallos);
        u_print(" fallos\n");
        return NV_fallos ? 1 : 0;
    }
    /* resumen de una URL o fichero */
    {
        int rc = nv_cargar(resto);
        int f;
        if (rc) {
            u_print("navega: no se pudo cargar (");
            nv_num(rc);
            u_print(")\n");
            return 1;
        }
        u_print("titulo: ");
        u_print(NV_titulo[0] ? NV_titulo : "(sin titulo)");
        u_print("\nfilas: ");
        nv_num(NV_nfilas);
        u_print(" estilos: ");
        nv_num(NV_nestilos);
        u_print("\n");
        for (f = 0; f < NV_nfilas; f++)
            nv_linea_consola(f);
        return 0;
    }
}

/* ---- visor grafico ---- */
#define NV_BAR_H 44
#define NV_EST_H 24

static ug_win_t NV_v;
static char NV_bar[NV_MAXURL + 1];
static char NV_url[NV_MAXURL + 1];
static char NV_estado[160];
static int NV_desplaza, NV_vis, NV_bpos;
static int NV_tiene, NV_foco_barra, NV_b_antes;
/* 1 = URL inicial pendiente: se carga en el primer DRAW (con la
   geometria ya conocida y un repintado explicito, porque el primer
   pintado a veces no se compone). */
static int NV_ini = 0;

static void nv_pon_estado(const char *s) {
    int i = 0;
    while (s[i] && i < 158) {
        NV_estado[i] = s[i];
        i++;
    }
    NV_estado[i] = 0;
}

/* Hito 4: historial con scroll por entrada */
static void nv_hist_empuja(const char *f) {
    int i;
    if (!f || !f[0]) return;
    if (NV_nhist > 0 && NV_ihist >= 0 && NV_ihist < NV_nhist &&
        !u_strcmp(NV_hist[NV_ihist], f))
        return;
    if (NV_ihist + 1 < NV_nhist)
        NV_nhist = NV_ihist + 1;    /* recorta el futuro */
    if (NV_nhist >= NV_MAXHIST) {
        for (i = 1; i < NV_MAXHIST; i++) {
            int k = 0;
            while (NV_hist[i][k]) {
                NV_hist[i - 1][k] = NV_hist[i][k];
                k++;
            }
            NV_hist[i - 1][k] = 0;
            NV_hscroll[i - 1] = NV_hscroll[i];
        }
        NV_nhist = NV_MAXHIST - 1;
    }
    {
        int k = 0;
        while (f[k] && k < NV_MAXURL) {
            NV_hist[NV_nhist][k] = f[k];
            k++;
        }
        NV_hist[NV_nhist][k] = 0;
    }
    NV_hscroll[NV_nhist] = 0;
    NV_ihist = NV_nhist;
    NV_nhist++;
}

static void nv_hist_ir(int d);

static void nv_navegar(const char *f, int empuja) {
    int rc, t0;
    /* Hito 4: f puede aliasar NV_enl[i].url, que nv_cargar
       reconstruye; se copia a slot estable antes de cargar. */
    static char dest[NV_MAXURL + 1];
    {
        int i = 0;
        if (!f || !f[0]) return;
        while (f[i] && i < NV_MAXURL) {
            dest[i] = f[i];
            i++;
        }
        dest[i] = 0;
    }
    f = dest;
    /* base para resolver relativas (solo http/https) */
    if (nv_es_url(f)) {
        int i = 0;
        while (f[i] && f[i] != '#' && i < NV_MAXURL) {
            NV_base[i] = f[i];
            i++;
        }
        NV_base[i] = 0;
    } else {
        NV_base[0] = 0;
    }
    NV_ult_cod = 0;
    NV_ult_tam = 0;
    t0 = u_ticks();
    rc = nv_cargar(f);
    {
        int ms = (u_ticks() - t0) * 10;
        int p = 0, q;
        if (ms < 0) ms = 0;
        /* NV_red: "200 110B 30ms" | "fich 1234B 5ms" | "ERR ..." */
        if (rc == 0 && NV_ult_cod != 0) {
            u_itoa(NV_ult_cod, NV_red);
            while (NV_red[p]) p++;
            NV_red[p++] = ' ';
            u_itoa(NV_ult_tam, NV_red + p);
            while (NV_red[p]) p++;
            NV_red[p++] = 'B';
            NV_red[p++] = ' ';
            u_itoa(ms, NV_red + p);
            while (NV_red[p]) p++;
            NV_red[p++] = 'm';
            NV_red[p++] = 's';
            NV_red[p] = 0;
        } else if (rc == 0) {
            q = 0;
            {
                const char *t = "fich ";
                while (t[q]) {
                    NV_red[q] = t[q];
                    q++;
                }
            }
            p = q;
            u_itoa(NV_ult_tam, NV_red + p);
            while (NV_red[p]) p++;
            NV_red[p++] = 'B';
            NV_red[p++] = ' ';
            u_itoa(ms, NV_red + p);
            while (NV_red[p]) p++;
            NV_red[p++] = 'm';
            NV_red[p++] = 's';
            NV_red[p] = 0;
        } else if (rc == -20) {
            const char *t = "ERR 404";
            int k = 0;
            while (t[k] && k < 60) {
                NV_red[k] = t[k];
                k++;
            }
            NV_red[k] = 0;
        } else {
            const char *t = "ERR red";
            int k = 0;
            while (t[k] && k < 60) {
                NV_red[k] = t[k];
                k++;
            }
            NV_red[k] = 0;
        }
    }
    if (rc == 0) {
        int i = 0;
        NV_tiene = 1;
        NV_foco_enl = -1;
        while (f[i] && i < NV_MAXURL) {
            NV_url[i] = f[i];
            NV_bar[i] = f[i];
            i++;
        }
        NV_url[i] = 0;
        NV_bar[i] = 0;
        NV_bpos = i;
        NV_desplaza = 0;
        if (NV_titulo[0])
            nv_pon_estado(NV_titulo);
        else
            nv_pon_estado("sin titulo");
        if (empuja)
            nv_hist_empuja(f);
    } else {
        NV_tiene = 0;
        NV_foco_enl = -1;
        if (rc <= -10 && rc >= -19)
            nv_pon_estado("error de red");
        else if (rc == -20)
            nv_pon_estado("pagina no encontrada");
        else
            nv_pon_estado("no se pudo cargar");
    }
}

static void nv_hist_ir(int d) {
    int j = NV_ihist + d;
    if (j < 0 || j >= NV_nhist) return;
    NV_hscroll[NV_ihist] = NV_desplaza;
    NV_ihist = j;
    nv_navegar(NV_hist[j], 0);
    NV_desplaza = NV_hscroll[j];
    if (NV_desplaza < 0) NV_desplaza = 0;
}

static void nv_dibuja(void) {
    int w = NV_v.w, h = NV_v.h, i;
    int cw, ch, cols;
    ug_comienza(&NV_v);
    ug_limpia(&NV_v, T_VENT_FONDO);
    ug_relleno(&NV_v, 0, 0, w, NV_BAR_H, T_PANEL);
    ug_linea(&NV_v, 0, NV_BAR_H - 1, w - 1, NV_BAR_H - 1,
             T_PANEL_BORDE);
    ug_boton(&NV_v, 8, 8, 44, 28, "<",
             (NV_nhist > 0 && NV_ihist > 0) ? UG_BOTON_NORMAL :
             UG_BOTON_OFF);
    ug_boton(&NV_v, 56, 8, 44, 28, ">",
             (NV_ihist < NV_nhist - 1) ? UG_BOTON_NORMAL :
             UG_BOTON_OFF);
    ug_boton(&NV_v, 104, 8, 44, 28, "R", UG_BOTON_NORMAL);
    ug_boton(&NV_v, 152, 8, 44, 28, "X", UG_BOTON_NORMAL);
    ug_campo(&NV_v, 200, 12, w - 200 - 60, NV_bar, NV_bpos,
             NV_foco_barra);
    ug_boton(&NV_v, w - 52, 8, 44, 28, "Ir", UG_BOTON_NORMAL);
    cw = w - 14;
    if (cw < 80) cw = 80;
    cols = cw / UG_FW;
    ch = h - NV_BAR_H - NV_EST_H;
    if (ch < UG_FH) ch = UG_FH;
    NV_vis = ch / UG_FH;
    if (NV_vis < 1) NV_vis = 1;
    if (!NV_tiene) {
        ug_texto(&NV_v, 24, NV_BAR_H + 40, "navega",
                 C_AMARILLO);
        ug_texto(&NV_v, 24, NV_BAR_H + 64,
                 NV_estado[0] ? NV_estado :
                 "Escribe una direccion y pulsa Intro.",
                 T_PANEL_TEXTO);
        ug_texto(&NV_v, 24, NV_BAR_H + 88,
                 "Ej: http://10.0.2.2:18080/pagina", C_GRIS);
        ug_texto(&NV_v, 24, NV_BAR_H + 112,
                 "Intro=ir r=recargar Retroceso=barra Tab=enlace",
                 C_GRIS);
        ug_texto(&NV_v, 24, NV_BAR_H + 136,
                 "Sin JS, sin imagenes, sin style= (ver NOTAS)",
                 C_GRIS);
    } else {
        int maxd = NV_nfilas - NV_vis;
        char tmp[160];
        if (maxd < 0) maxd = 0;
        if (NV_desplaza > maxd) NV_desplaza = maxd;
        if (NV_desplaza < 0) NV_desplaza = 0;
        for (i = 0; i < NV_vis; i++) {
            int f = NV_desplaza + i;
            int y = NV_BAR_H + i * UG_FH, q, k;
            int x = 4, color = T_PANEL_TEXTO;
            if (f >= NV_nfilas) break;
            if (NV_filas[f].estilo == 1) continue;
            if (NV_filas[f].estilo == 2) {
                ug_linea(&NV_v, 4, y + 8, cw - 4, y + 8,
                         C_GRIS);
                continue;
            }
            if (NV_filas[f].estilo == 3) {
                /* Hito 5: banda de imagen (ajuste a 600 px) */
                int gi = NV_filas[f].img;
                if (gi >= 0 && gi < NV_nimg &&
                    NV_img[gi].px && NV_img[gi].w > 0 &&
                    NV_img[gi].h > 0) {
                    int band = f - NV_img[gi].fila;
                    int dw = NV_img[gi].w <= 600 ?
                        NV_img[gi].w : 600;
                    int dh = (NV_img[gi].h * dw) /
                        NV_img[gi].w;
                    int yy, xx;
                    if (dh < 1) dh = 1;
                    for (yy = 0; yy < UG_FH &&
                         band * UG_FH + yy < dh; yy++) {
                        int sy = (band * UG_FH + yy) *
                            NV_img[gi].h / dh;
                        for (xx = 0; xx < dw &&
                             x + xx < cw - 4; xx++) {
                            int sx = xx * NV_img[gi].w / dw;
                            unsigned char *pp =
                                NV_img[gi].px +
                                (sy * NV_img[gi].w + sx) * 3;
                            ug_pixel(&NV_v, x + xx, y + yy,
                                     ((int)pp[0] << 16) |
                                     ((int)pp[1] << 8) |
                                     (int)pp[2]);
                        }
                    }
                }
                continue;
            }
            q = NV_filas[f].len;
            if (q > (int)sizeof(tmp) - 2) q = (int)sizeof(tmp) - 2;
            if (q > cols) q = cols;
            for (k = 0; k < q; k++)
                tmp[k] = (char)NV_txt[NV_filas[f].ini + k];
            tmp[q] = 0;
            if (NV_filas[f].fg >= 0)
                color = 0x01000000 |
                    (NV_filas[f].fg & 0xFFFFFF);
            if (NV_filas[f].bg >= 0) {
                int wpx = q * UG_FW;
                ug_relleno(&NV_v, x, y, wpx, UG_FH,
                           0x01000000 |
                           (NV_filas[f].bg & 0xFFFFFF));
            }
            /* Hito 4: enlace con foco -> banda de seleccion */
            if (NV_tiene && NV_foco_enl >= 0 &&
                NV_foco_enl < NV_nenl &&
                f >= NV_enl[NV_foco_enl].f0 &&
                f <= NV_enl[NV_foco_enl].f1) {
                int wpx = q * UG_FW;
                if (wpx < 8) wpx = 8;
                ug_relleno(&NV_v, x, y, wpx, UG_FH,
                           T_SELECCION);
                color = C_BLANCO;
            }
            ug_texto(&NV_v, x, y, tmp, color);
        }
        /* scrollbar */
        if (NV_nfilas > NV_vis) {
            int th = NV_vis * UG_FH;
            int tam = th * NV_vis / NV_nfilas;
            int pos = th * NV_desplaza / NV_nfilas;
            if (tam < 8) tam = 8;
            ug_relleno(&NV_v, w - 8, NV_BAR_H + pos, 6, tam,
                       C_GRIS);
        }
    }
    ug_relleno(&NV_v, 0, h - NV_EST_H, w, NV_EST_H, T_PANEL);
    ug_texto(&NV_v, 8, h - NV_EST_H + 4,
             NV_estado[0] ? NV_estado : "navega Hito 4",
             T_PANEL_TEXTO);
    /* Hito 4: resultado de red a la derecha (codigo, bytes, ms) */
    if (NV_red[0]) {
        int lr = 0, xr;
        while (NV_red[lr]) lr++;
        xr = w - 8 - lr * UG_FW;
        if (xr < w / 2) xr = w / 2;
        ug_texto(&NV_v, xr, h - NV_EST_H + 4, NV_red, C_GRIS);
    }
    ug_fin(&NV_v);
}

/* Hito 4: lleva el enlace con foco a la vista */
static void nv_asegura_enlace(int idx) {
    int f;
    if (idx < 0 || idx >= NV_nenl) return;
    f = NV_enl[idx].f0;
    if (f < NV_desplaza)
        NV_desplaza = f;
    else if (f >= NV_desplaza + NV_vis)
        NV_desplaza = f - NV_vis + 1;
    if (NV_desplaza < 0) NV_desplaza = 0;
}

static void nv_ev_tecla(int tecla) {
    if (NV_foco_barra) {
        if (tecla == 13) {
            nv_navegar(NV_bar, 1);
            NV_foco_barra = 0;
        } else if (tecla == 27) {
            int i = 0;
            while (NV_url[i]) {
                NV_bar[i] = NV_url[i];
                i++;
            }
            NV_bar[i] = 0;
            NV_bpos = i;
            NV_foco_barra = 0;
        } else if (tecla == 9) {
            /* Tab en barra: primer enlace */
            NV_foco_barra = 0;
            NV_foco_enl = (NV_tiene && NV_nenl > 0) ? 0 : -1;
            nv_asegura_enlace(NV_foco_enl);
        } else {
            ug_campo_tecla(NV_bar, (int)sizeof(NV_bar),
                           &NV_bpos, tecla);
        }
        return;
    }
    if (tecla == 0x101) {
        NV_desplaza--;
        if (NV_desplaza < 0) NV_desplaza = 0;
    } else if (tecla == 0x102) {
        NV_desplaza++;
    } else if (tecla == 0x109) {
        NV_desplaza -= NV_vis;
        if (NV_desplaza < 0) NV_desplaza = 0;
    } else if (tecla == 0x10A) {
        NV_desplaza += NV_vis;
    } else if (tecla == 0x107) {
        NV_desplaza = 0;
    } else if (tecla == 0x108) {
        NV_desplaza = NV_nfilas - NV_vis;
        if (NV_desplaza < 0) NV_desplaza = 0;
    } else if (tecla == 8) {
        nv_hist_ir(-1);
    } else if (tecla == 'r' || tecla == 'R') {
        if (NV_url[0])
            nv_navegar(NV_url, 0);
    } else if (tecla == 9) {
        /* Tab: siguiente enlace (al final, vuelve a la barra) */
        if (NV_tiene && NV_nenl > 0 && NV_foco_enl + 1 < NV_nenl) {
            NV_foco_enl++;
            nv_asegura_enlace(NV_foco_enl);
        } else {
            NV_foco_enl = -1;
            NV_foco_barra = 1;
        }
    } else if (tecla == 13) {
        /* Intro: sigue el enlace con foco */
        if (NV_tiene && NV_foco_enl >= 0 &&
            NV_foco_enl < NV_nenl)
            nv_navegar(NV_enl[NV_foco_enl].url, 1);
        else if (!NV_tiene)
            NV_foco_barra = 1;
    } else if (tecla == 27) {
        NV_foco_enl = -1;
        NV_foco_barra = 1;
    }
}

static void nv_ev_raton(int lx, int ly, int bot, int rueda) {
    int clic = ug_clic(NV_b_antes, bot);
    NV_b_antes = bot;
    if (rueda) {
        NV_desplaza -= rueda * 2;
        if (NV_desplaza < 0) NV_desplaza = 0;
        return;
    }
    if (!clic) return;
    if (ly >= 8 && ly < 36) {
        if (lx >= 8 && lx < 52) {
            nv_hist_ir(-1);
            return;
        }
        if (lx >= 56 && lx < 100) {
            nv_hist_ir(1);
            return;
        }
        if (lx >= 104 && lx < 148) {
            if (NV_url[0])
                nv_navegar(NV_url, 0);
            return;
        }
        if (lx >= 152 && lx < 196) {
            NV_tiene = 0;
            NV_foco_enl = -1;
            NV_url[0] = 0;
            NV_bar[0] = 0;
            NV_bpos = 0;
            NV_foco_barra = 1;
            nv_pon_estado("");
            NV_red[0] = 0;
            return;
        }
        if (lx >= NV_v.w - 52 && lx < NV_v.w - 8) {
            nv_navegar(NV_bar, 1);
            NV_foco_barra = 0;
            return;
        }
        if (lx >= 200 && lx < NV_v.w - 60) {
            NV_foco_barra = 1;
            return;
        }
        NV_foco_barra = 0;
        return;
    }
    if (ly >= NV_BAR_H && ly < NV_v.h - NV_EST_H) {
        if (lx >= NV_v.w - 12 && NV_nfilas > NV_vis) {
            int th = NV_vis * UG_FH;
            int rel = ly - NV_BAR_H;
            int tam = th * NV_vis / NV_nfilas;
            int pos = th * NV_desplaza / NV_nfilas;
            if (rel < pos)
                NV_desplaza -= NV_vis;
            else if (rel > pos + tam)
                NV_desplaza += NV_vis;
            if (NV_desplaza < 0) NV_desplaza = 0;
        } else if (lx < NV_v.w - 12) {
            /* Hito 4: clic en enlace */
            int f = NV_desplaza +
                (ly - NV_BAR_H) / UG_FH, idx;
            if (f >= 0 && f < NV_nfilas) {
                idx = nv_enlace_en(f);
                if (idx >= 0)
                    nv_navegar(NV_enl[idx].url, 1);
            }
        }
    }
}

int main(void) {
    const char *a = u_args();
    int srv = 0;
    ug_ev_t ev;
    while (*a == ' ') a++;
    if (a[0] == '-' && a[1] == 'p' &&
        (a[2] == 0 || a[2] == ' ')) {
        return nv_modo_p(a + 2);
    }
    srv = u_atoi(a);
    while (*a >= '0' && *a <= '9') a++;
    while (*a == ' ') a++;
    if (srv <= 0) {
        u_print("Uso: navega <srv> [url] | navega -p [url]\n");
        return 99;
    }
    NV_ntxt = 0;
    NV_nfilas = 0;
    NV_nestilos = 0;
    NV_titulo[0] = 0;
    NV_bar[0] = 0;
    NV_bpos = 0;
    NV_url[0] = 0;
    NV_estado[0] = 0;
    NV_desplaza = 0;
    NV_vis = 20;
    NV_tiene = 0;
    NV_foco_barra = 1;
    NV_b_antes = 0;
    if (ug_abre(&NV_v, srv, NV_W, NV_H, "navega"))
        return 98;
    ug_edit(&NV_v, 1);
    if (*a) {
        int i = 0;
        while (a[i] && i < NV_MAXURL) {
            NV_bar[i] = a[i];
            i++;
        }
        NV_bar[i] = 0;
        NV_bpos = i;
        NV_foco_barra = 0;
        /* La carga se difiere al primer DRAW: geometria conocida
           y repintado explicito tras la descarga. */
        NV_ini = 1;
        nv_pon_estado("cargando...");
    }
    for (;;) {
        int t = ug_evento(&NV_v, &ev, 1);
        if (t == UG_EV_SALIR) break;
        if (t == UG_EV_TECLA) {
            nv_ev_tecla(ev.tecla);
        } else if (t == UG_EV_RATON) {
            nv_ev_raton(ev.rx - NV_v.x,
                        ev.ry - (NV_v.y + UG_CHROME), ev.rbot,
                        ev.rueda);
        } else if (t == UG_EV_DRAW) {
            nv_dibuja();
            if (NV_ini) {
                NV_ini = 0;
                nv_navegar(NV_bar, 1);
                nv_dibuja();
            }
        }
    }
    nv_css_fin();
    ug_cierra(&NV_v);
    return 0;
}