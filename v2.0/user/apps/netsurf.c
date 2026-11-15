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
#include "imgdec.h"
#include "fuente/ttf.h"

/* netsurf (Hito 3 NetSurf): visor minimo web/local.
 * fetch (nsfetch) o fichero -> hubbub -> libdom -> select CSS
 * (color/fondo) -> layout de texto -> ventana ugfx con scroll.
 * Imagenes BMP/GIF (libnsbmp/nsgif) + PNG/JPEG propios (imgdec).
 * Sin JS, sin formularios (documentado). -p = resumen a consola. */

#define NV_MAXTEXTO (64 * 1024)
#define NV_MAXFILAS 2048
#define NV_MAXPILA 64
#define NV_MAXTITULO 128
#define NV_MAXCSS (24 * 1024)
#define NV_MAXURL 512
#define NV_W 640
#define NV_H 460

/* Tema claro profesional (fondo blanco, tinta oscura, acento azul).
   RGB24 de tema.h (bit24 = color crudo para el nucleo). */
#define NS_BLANCO RGB24(255, 255, 255)
#define NS_TINTA RGB24(24, 32, 48)
#define NS_TITULO RGB24(0, 102, 204)
#define NS_ENLACE RGB24(0, 102, 204)
#define NS_PANEL RGB24(214, 226, 245)
#define NS_BORDE RGB24(170, 180, 195)
#define NS_EST RGB24(230, 233, 238)
#define NS_GRIS RGB24(100, 110, 125)
#define NS_CAMPO_FOCO RGB24(255, 255, 255)

/* Pagina de bienvenida (sin red): se muestra al abrir sin URL.
   Etiquetas del motor (h1/p/a/ul/li/br/hr) + CSS basica. */
#define NV_BIENV_URL "netsurf:bienvenida"
static const char NV_BIENV_HTML[] =
    "<html><head><title>Bienvenido a NetSurf</title></head>"
    "<body>"
    "<h1>Welcome to NetSurf</h1>"
    "<p>NetSurf para FacilOS: navegador rapido con motor NetSurf "
    "(hubbub/dom/css). Escribe una direccion o una busqueda en la "
    "barra superior y pulsa Enter.</p>"
    "<p><a href=\"https://old-search.marginalia.nu/search?query=FacilOS\">"
    "Buscar con Marginalia</a></p>"
    "<p><a href=\"https://marginalia-search.com/search?query=FacilOS\">"
    "Marginalia (vista nueva, exige JS)</a></p>"
    "<ul>"
    "<li><a href=\"http://info.cern.ch/\">CERN (primera web)</a></li>"
    "<li><a href=\"http://example.com/\">Example</a></li>"
    "<li><a href=\"https://www.wikipedia.org/\">Wikipedia</a></li>"
    "<li><a href=\"https://old-search.marginalia.nu/\">Marginalia clasica</a></li>"
    "</ul>"
    "</body></html>";

static unsigned char NV_txt[NV_MAXTEXTO];
static int NV_ntxt = 0;

typedef struct {
    int ini, len;
    int fg;                    /* 0xRRGGBB o -1 = defecto */
    int bg;                    /* 0xRRGGBB o -1 = ninguno */
    int estilo;                /* 0 normal, 1 vacia, 2 HR, 3 IMG */
    int img;                   /* indice en NV_img o -1 */
    int sub;                   /* 1 = subrayado (enlaces) */
    int full;                  /* 1 = fondo a ancho completo */
    int al;                    /* 0 izq, 1 centro, 2 der */
    int bd;                    /* 1 = negrita (doble pasada) */
} nv_fila_t;
static nv_fila_t NV_filas[NV_MAXFILAS];
static int NV_nfilas = 0;
static int NV_nestilos = 0;

/* ---- etiquetas ---- */
enum {
    T_OTRO = 0, T_P, T_DIV, T_H1, T_H2, T_H3, T_H4, T_H5, T_H6,
    T_UL, T_OL, T_LI, T_PRE, T_BR, T_HR, T_BLOQUE, T_A,
    T_SCRIPT, T_STYLE, T_TITLE, T_HEAD, T_HTML, T_BODY, T_META,
    T_LINK, T_IMG, T_INPUT, T_FORM, T_TABLE, T_TR, T_TD, T_TH,
    T_TEXTAREA, T_SELECT, T_OPTION, T_BUTTON
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
    if (!u_strcmp(b, "textarea")) return T_TEXTAREA;
    if (!u_strcmp(b, "select")) return T_SELECT;
    if (!u_strcmp(b, "option")) return T_OPTION;
    if (!u_strcmp(b, "button")) return T_BUTTON;
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
           el layout lee src/alt); LINK tambien (v2.5: el
           layout lee rel/href para hojas externas); INPUT
           tambien (v2.7: el layout lee type/name/value para
           formularios); el resto se ignora */
        if (id != T_BR && id != T_HR && id != T_IMG &&
            id != T_LINK && id != T_INPUT)
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
                        if (id != T_A && id != T_LINK) continue;
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
                    } else if (at->name.len >= 1 &&
                               at->name.len <= 31) {
                        /* Hito 6: resto (class, id, title...):
                           se guardan tal cual para el select */
                        static char gen[32];
                        uint32_t gl;
                        for (gl = 0; gl < at->name.len; gl++) {
                            unsigned char cc =
                                at->name.ptr[gl];
                            gen[gl] = (char)(cc >= 'A' &&
                                             cc <= 'Z' ?
                                             cc + ('a' - 'A') :
                                             cc);
                        }
                        gen[gl] = 0;
                        want = gen;
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

/* Hito 6: libcss libera (unref) cada cadena que le entregamos, y
   lwc_interna las mismas entre llamadas. Guardamos NUESTRA ref en
   un registro por documento para que nunca se liberen antes de
   tiempo (se sueltan al empezar el siguiente documento). */
#define NV_MAXREF 2048
static lwc_string *NV_refs[NV_MAXREF];
static int NV_nref = 0;

static void nv_ref_guarda(lwc_string *s) {
    if (!s) return;
    if (NV_nref < NV_MAXREF)
        NV_refs[NV_nref++] = s;
    else
        lwc_string_unref(s);
}

/* tablas de clases (una por llamada: libcss compara la del nodo
   con la del candidato en la misma seleccion; un estatico comun
   se aliasa y todo comparte estilo). Se liberan con el documento. */
#define NV_MAXTAB 256
static lwc_string **NV_tabs[NV_MAXTAB];
static int NV_ntab2 = 0;

static void nv_ref_libera(void) {
    int i;
    for (i = 0; i < NV_nref; i++) {
        if (NV_refs[i]) lwc_string_unref(NV_refs[i]);
        NV_refs[i] = 0;
    }
    NV_nref = 0;
    for (i = 0; i < NV_ntab2; i++) {
        if (NV_tabs[i]) u_free(NV_tabs[i]);
        NV_tabs[i] = 0;
    }
    NV_ntab2 = 0;
}

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

static css_error nv_clases(void *pw, void *nodo,
                           lwc_string ***clases, uint32_t *n) {
    char buf[256];
    int j = 0;
    uint32_t c = 0;
    lwc_string **tab;
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
    tab = (lwc_string **)u_malloc(16 * (int)sizeof(lwc_string *));
    if (!tab) return CSS_NOMEM;
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
                lwc_error_ok || !s) {
                u_free(tab);
                return CSS_NOMEM;
            }
            /* ref propia en el registro (libcss libera la suya) */
            nv_ref_guarda(lwc_string_ref(s));
            tab[c++] = s;
        }
    }
    if (c) {
        *clases = tab;
        *n = c;
        if (NV_ntab2 < NV_MAXTAB)
            NV_tabs[NV_ntab2++] = tab;
        /* si el registro esta lleno, la tabla se pierde (fuga
           acotada) pero libcss sigue viendo memoria valida */
    } else {
        u_free(tab);
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
    /* ref propia en el registro (libcss libera la suya) */
    nv_ref_guarda(lwc_string_ref(*id));
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

/* Hito 6: reglas de agente de usuario (enlaces azules subrayados,
   negrita de titulares, como NetSurf). Se fusionan DELANTE de la
   hoja <style> en una sola stylesheet: el autor gana por orden y
   por especificidad; la cascada multi-hoja queda fuera. */
static const char NV_UA_CSS[] =
    "a:link{color:#0000ee;text-decoration:underline}"
    "b,strong,h1,h2,h3,h4,h5,h6{font-weight:bold}";

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

/* crea ctx + hoja unica (UA delante + <style> capturado).
   Hito 6: se probo ctx separado para la UA (ORIGIN_UA) pero la
   cascada multi-hoja no aplicaba bien el inline; con una sola
   hoja todo casa. Se documenta como limite. */
static int nv_css_ini(const char *css, int n) {
    css_stylesheet_params p;
    css_error e;
    nv_css_fin();
    if (css_select_ctx_create(&NV_ctx) != CSS_OK || !NV_ctx)
        return -1;
    p.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
    p.level = CSS_LEVEL_21;
    p.charset = "UTF-8";
    p.url = "http://netsurf/";
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
    e = css_stylesheet_append_data(NV_hoja,
                                   (const uint8_t *)NV_UA_CSS,
                                   (size_t)(sizeof(NV_UA_CSS) - 1));
    if (e != CSS_OK && e != CSS_NEEDDATA) {
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

/* lee color/fondo del estilo calculado; -1 si no fijado.
   Hito 6: un fondo con alfa 0 es transparente -> se hereda (-1);
   asi el color inicial no pinta bandas negras. */
static void nv_colores(css_computed_style *st, int *fg, int *bg) {
    css_color c;
    *fg = -1;
    *bg = -1;
    if (!st) return;
    if (css_computed_color(st, &c) == CSS_COLOR_COLOR)
        *fg = (int)(c & 0xFFFFFF);
    if (css_computed_background_color(st, &c) ==
        CSS_BACKGROUND_COLOR_COLOR &&
        (c & 0xFF000000u) != 0)
        *bg = (int)(c & 0xFFFFFF);
}

/* ---- layout de texto ---- */
static char NV_titulo[NV_MAXTITULO];
static int NV_fg = -1, NV_bg = -1;
extern char fuente_datos[];
extern char fuente_fin[];

/* v2.8: texto proporcional propio. El contenido se envuelve por
   ancho medido (NV_MAXW px) y las filas miden NV_FH px (metrica de
   la fuente a NV_PX). Los campos siguen en celdas de 8 px para su
   valor en mapa de bits; solo su posicion X se mide. */
#define NV_PX 16
#define NV_MAXW 600
static int NV_FH = 16;
static int NV_asc = 12;
static int NV_CELDA = 8;   /* avance medio para estimar cajas */

/* Abre la fuente enlazada; fija fila y ascenso. Sin video tambien
   vale (solo tablas en memoria): el KAT -p envuelve con lo mismo. */
static void nv_fuente_ini(void) {
    int tam = (int)(fuente_fin - fuente_datos);
    NV_FH = 16;
    NV_asc = 12;
    if (tam > 100000 &&
        !ttf_abre((const unsigned char *)fuente_datos, tam)) {
        int fh = ttf_alto_linea(NV_PX);
        int asc = ttf_ascenso(NV_PX);
        if (fh >= 8 && fh <= 64) NV_FH = fh;
        if (asc >= 6 && asc <= NV_FH) NV_asc = asc;
        {
            int cm = ttf_ancho("abcdefghijklmnopqrstuvwxyz",
                               26, NV_PX);
            if (cm > 26) NV_CELDA = cm / 26;
        }
    }
}
static int NV_pend_esp = 0;
/* Hito 6: subrayado (contador: <a> anidados), fondo de bloque
   explicito (contador) y alineacion (0 izq, 1 centro, 2 der) */
static int NV_sub = 0, NV_bgexp = 0, NV_al = 0, NV_b = 0;
/* ultimo estilo calculado: decoracion, bg explicito, alineacion */
static int NV_last_dec = 0, NV_last_bgexp = 0, NV_last_al = -1;
static int NV_last_bold = 0, NV_last_fgset = 0;

/* enlaces (Hito 4): filas [f0,f1] -> URL navegable.
   Hito 6: columnas [c0,c1] del subrayado (celdas de texto). */
#define NV_MAXENL 64
typedef struct {
    int f0, f1, c0, c1;
    char url[256];
} nv_enl_t;
static nv_enl_t NV_enl[NV_MAXENL];
static int NV_nenl = 0, NV_foco_enl = -1;

/* formularios (v2.7): GET/POST sin JS. Los campos se pintan en linea
   como cajas ([____], [Enviar], [X]) y comparten el anillo de foco
   con los enlaces. textarea/select se reconocen pero aun no se
   editan (fase 2). */
#define NV_MAXFORM 8
#define NV_MAXCAMP 48
#define NV_CAMPO_TAM 120
#define NV_CAMPO_ANCHO 24
typedef struct {
    char accion[NV_MAXURL + 1];
    int metodo;                 /* 0 get, 1 post */
} nv_form_t;
typedef struct {
    int form;                   /* indice en NV_forms o -1 */
    char nombre[64];
    int tipo;                   /* 0 texto, 1 clave, 2 oculto,
                                   3 enviar, 4 caja, 5 area,
                                   6 lista, 7 radio, 8 reponer,
                                   9 nada (boton inerte) */
    char valor[NV_CAMPO_TAM + 1];
    int pos;                    /* cursor en valor (o seleccion) */
    int marca;                  /* caja marcada */
    int fila, col;              /* caja en el texto */
    int ancho;                  /* ancho caja en celdas */
    int alto;                   /* filas que ocupa (area) */
    char ini[NV_CAMPO_TAM + 1]; /* valor inicial (reset) */
    int mini;                   /* marca inicial */
    int pini;                   /* pos/seleccion inicial */
    char ops[6][32];            /* opciones de la lista */
    int nopc;                   /* opciones */
} nv_campo_t;
static nv_form_t NV_forms[NV_MAXFORM];
static int NV_nforms = 0;
static nv_campo_t NV_campos[NV_MAXCAMP];
static int NV_ncampos = 0, NV_foco_campo = -1;
/* pila de formularios abiertos durante el recorrido */
static int NV_pila_form[4];
static int NV_npila_form = 0;

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
    NV_filas[NV_nfilas].sub = 0;
    NV_filas[NV_nfilas].full = NV_bgexp > 0;
    NV_filas[NV_nfilas].al = NV_al;
    NV_filas[NV_nfilas].bd = NV_b > 0;
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
        /* Hito 6: la fila vacia adopta el contexto del bloque */
        NV_filas[NV_nfilas].full = NV_bgexp > 0;
        NV_filas[NV_nfilas].al = NV_al;
    }
}

static int nv_ancho_linea(void) {
    return NV_ntxt - NV_filas[NV_nfilas].ini;
}

/* v2.8: ancho medido de la fila actual (bytes UTF-8). */
static int nv_ancho_px(void) {
    int ini = NV_filas[NV_nfilas].ini;
    return ttf_ancho((const char *)NV_txt + ini, NV_ntxt - ini,
                     NV_PX);
}

/* Ancho de los primeros n bytes de la fila f (n recortado). */
static int nv_ancho_hasta(int f, int n) {
    int len;
    if (f < 0 || f >= NV_nfilas) return 0;
    len = NV_filas[f].len;
    if (n > len) n = len;
    if (n < 0) n = 0;
    return ttf_ancho((const char *)NV_txt + NV_filas[f].ini, n,
                     NV_PX);
}

/* Bytes de la fila f que caben en maxw sin partir UTF-8 (max 159). */
static int nv_ajusta(int f, int maxw) {
    int len, ini, lo, hi, mid, w;
    if (f < 0 || f >= NV_nfilas) return 0;
    if (maxw < 0) maxw = 0;
    len = NV_filas[f].len;
    if (len > 159) len = 159;
    ini = NV_filas[f].ini;
    lo = 0;
    hi = len;
    while (lo < hi) {
        mid = (lo + hi + 1) / 2;
        w = ttf_ancho((const char *)NV_txt + ini, mid, NV_PX);
        if (w <= maxw)
            lo = mid;
        else
            hi = mid - 1;
    }
    while (lo > 0 &&
           ((NV_txt[ini + lo] & 0xC0) == 0x80))
        lo--;
    return lo;
}

/* 1 si la cola de la fila actual es secuencia UTF-8 incompleta. */
static int nv_cola_incompleta(void) {
    int ini = NV_filas[NV_nfilas].ini;
    int len = NV_ntxt - ini, cont = 0;
    unsigned char c;
    if (len <= 0) return 0;
    while (len - 1 - cont >= 0 &&
           ((NV_txt[ini + len - 1 - cont] & 0xC0) == 0x80)) {
        cont++;
        if (cont >= 3) break;
    }
    if (ini + len - 1 - cont < ini) return 0;
    c = NV_txt[ini + len - 1 - cont];
    if (c < 0x80) return 0;
    if (c >= 0xC2 && c <= 0xDF) return cont < 1;
    if (c >= 0xE0 && c <= 0xEF) return cont < 2;
    if (c >= 0xF0 && c <= 0xF4) return cont < 3;
    return 0;
}

/* Bytes del ultimo caracter de la fila actual (1..4). */
static int nv_ultimo_car(void) {
    int ini = NV_filas[NV_nfilas].ini;
    int len = NV_ntxt - ini, j = 1;
    if (len <= 0) return 0;
    while (j < len && j < 4 &&
           ((NV_txt[ini + len - j] & 0xC0) == 0x80))
        j++;
    return j;
}

/* vuelca un byte al buffer (con wrap medido); devuelve 0 si no cabe */
static void nv_pon_byte(unsigned char c) {
    if (NV_nfilas >= NV_MAXFILAS) return;
    if (c == '\n') {
        nv_salto();
        return;
    }
    if (NV_ntxt >= NV_MAXTEXTO - 1) return;
    /* v2.8: el wrap es por ancho medido, no por celdas. No parte
       UTF-8: si la cola queda incompleta y el exceso es poco, el
       caracter entero pasa a esta fila. */
    NV_txt[NV_ntxt++] = c;
    {
        int ini = NV_filas[NV_nfilas].ini;
        int w = ttf_ancho((const char *)NV_txt + ini,
                          NV_ntxt - ini, NV_PX);
        if (w > NV_MAXW &&
            (!nv_cola_incompleta() || w > NV_MAXW + 24)) {
            int j = nv_ultimo_car();
            NV_ntxt -= j;
            nv_salto();
            if (NV_nfilas >= NV_MAXFILAS) return;
            NV_ntxt += j;
            NV_filas[NV_nfilas].fg = NV_fg;
            NV_filas[NV_nfilas].bg = NV_bg;
            if (NV_sub > 0)
                NV_filas[NV_nfilas].sub = 1;
            if (NV_b > 0)
                NV_filas[NV_nfilas].bd = 1;
            return;
        }
    }
    /* Hito 6: la fila adopta el estilo vigente (los elementos en
       linea cambian fg/bg despues de crearla) */
    NV_filas[NV_nfilas].fg = NV_fg;
    NV_filas[NV_nfilas].bg = NV_bg;
    if (NV_sub > 0)
        NV_filas[NV_nfilas].sub = 1;
    if (NV_b > 0)
        NV_filas[NV_nfilas].bd = 1;
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
static int nv_campo_xy(int ci, int f, int *xo, int *yo);
static int nv_campo_en_px(int f, int lx);
static int nv_enlace_en(int f) {
    int i;
    for (i = NV_nenl - 1; i >= 0; i--) {
        if (f >= NV_enl[i].f0 && f <= NV_enl[i].f1)
            return i;
    }
    return -1;
}

/* v2.7: indice del campo cuya caja cubre el pixel (f, lx), o -1.
   Del ultimo al primero: el mas interno gana. */
static int nv_campo_en_px(int f, int lx) {
    int i;
    for (i = NV_ncampos - 1; i >= 0; i--) {
        if (NV_campos[i].tipo != 2 &&
            f >= NV_campos[i].fila &&
            f < NV_campos[i].fila + NV_campos[i].alto) {
            int bx, by;
            if (!nv_campo_xy(i, f, &bx, &by)) {
                int bw = NV_campos[i].ancho * UG_FW;
                if (lx >= bx && lx < bx + bw) return i;
            }
        }
    }
    return -1;
}

/* nv_es_url vive abajo (carga); avance para nv_imagen */
static int nv_es_url(const char *s);
static void nv_num(int v);

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
    /* v2.5: PNG (firma 89 50 4E 47) y JPEG (FF D8) propios (imgdec);
       BMP/GIF siguen por libnsbmp/nsgif. Otros: sin decodificador. */
    if (n >= 8 && d[0] == 137 && d[1] == 'P' && d[2] == 'N' &&
        d[3] == 'G') {
        unsigned char *sal = 0;
        int w0 = 0, h0 = 0;
        if (imgdec_png(d, n, NV_IMG_MAXPX, &w0, &h0, &sal))
            return -1;
        if (w0 < 1 || h0 < 1 ||
            (unsigned)w0 * (unsigned)h0 > (unsigned)NV_IMG_MAXPX ||
            !sal) {
            if (sal) u_free(sal);
            return -1;
        }
        *w = w0;
        *h = h0;
        *rgb = sal;
        return 0;
    }
    if (n >= 4 && d[0] == 0xFF && d[1] == 0xD8) {
        unsigned char *sal = 0;
        int w0 = 0, h0 = 0;
        if (imgdec_jpg(d, n, NV_IMG_MAXPX, &w0, &h0, &sal))
            return -1;
        if (w0 < 1 || h0 < 1 ||
            (unsigned)w0 * (unsigned)h0 > (unsigned)NV_IMG_MAXPX ||
            !sal) {
            if (sal) u_free(sal);
            return -1;
        }
        *w = w0;
        *h = h0;
        *rgb = sal;
        return 0;
    }
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
        if (nsfetch_get_ck(NV_tmpurl, &r) == NSFETCH_OK &&
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
    NV_last_dec = 0;
    NV_last_bgexp = 0;
    NV_last_al = -1;
    NV_last_bold = 0;
    NV_last_fgset = 0;
    if (!NV_css_ok || !n) return;
    if (css_select_style(NV_ctx, n, &NV_uctx, &NV_media, 0,
                         &NV_sel, 0, &res) != CSS_OK || !res)
        return;
    if (res->styles[CSS_PSEUDO_ELEMENT_NONE]) {
        css_computed_style *st =
            res->styles[CSS_PSEUDO_ELEMENT_NONE];
        int fg = -1, bg = -1;
        uint8_t ta, fw;
        nv_colores(st, &fg, &bg);
        if (fg >= 0) {
            NV_fg = fg;
            NV_last_fgset = 1;
            NV_nestilos++;
        }
        if (bg >= 0) {
            NV_bg = bg;
            NV_nestilos++;
            NV_last_bgexp = 1;
        }
        /* Hito 6: subrayado, alineacion y negrita del nodo */
        if (css_computed_text_decoration(st) &
            CSS_TEXT_DECORATION_UNDERLINE)
            NV_last_dec = CSS_TEXT_DECORATION_UNDERLINE;
        ta = css_computed_text_align(st);
        if (ta == CSS_TEXT_ALIGN_CENTER ||
            ta == CSS_TEXT_ALIGN_LIBCSS_CENTER)
            NV_last_al = 1;
        else if (ta == CSS_TEXT_ALIGN_RIGHT ||
                 ta == CSS_TEXT_ALIGN_LIBCSS_RIGHT)
            NV_last_al = 2;
        else if (ta == CSS_TEXT_ALIGN_LEFT ||
                 ta == CSS_TEXT_ALIGN_LIBCSS_LEFT ||
                 ta == CSS_TEXT_ALIGN_JUSTIFY)
            NV_last_al = 0;
        fw = css_computed_font_weight(st);
        if (fw == CSS_FONT_WEIGHT_BOLD ||
            fw == CSS_FONT_WEIGHT_BOLDER ||
            (fw >= CSS_FONT_WEIGHT_600 &&
             fw <= CSS_FONT_WEIGHT_900))
            NV_last_bold = 1;
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
            p.url = "http://netsurf/";
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
                                NV_last_fgset = 1;
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

/* v2.7-fase2: junta el texto de los hijos (textarea, option).
   pre=1 respeta saltos (textarea); pre=0 colapsa blancos. */
static void nv_texto_hijo_n(dom_node *n, char *sal, int max, int pre,
                            int prof) {
    dom_node *h = 0;
    int t = 0, pend = 0, prim = 1;
    if (!n || !sal || max < 1 || prof > 4) return;
    sal[0] = 0;
    if (dom_node_get_first_child(n, &h) != DOM_NO_ERR || !h)
        return;
    for (;;) {
        dom_node_type tt = (dom_node_type)0;
        if (dom_node_get_node_type(h, &tt) == DOM_NO_ERR) {
            if (tt == DOM_TEXT_NODE) {
                dom_string *ds = 0;
                if (dom_characterdata_get_data(
                        (dom_characterdata *)h,
                        &ds) == DOM_NO_ERR && ds) {
                    const uint8_t *p = dom_string_data(ds);
                    uint32_t m = dom_string_length(ds);
                    uint32_t i;
                    for (i = 0; i < m && t + 1 < max; i++) {
                        unsigned char c = p[i];
                        if (c == '\r') continue;
                        if (pre) {
                            if (c == '\t') c = ' ';
                            sal[t++] = (char)c;
                            continue;
                        }
                        if (c == '\n' || c == '\t' || c == ' ')
                            pend = 1;
                        else {
                            if (pend && !prim) {
                                if (t + 1 >= max) break;
                                sal[t++] = ' ';
                            }
                            pend = 0;
                            prim = 0;
                            sal[t++] = (char)c;
                        }
                    }
                    dom_string_unref(ds);
                }
            } else if (tt == DOM_ELEMENT_NODE && pre) {
                /* textarea con hijos con marca: se aplana */
                char sub[NV_CAMPO_TAM + 1];
                nv_texto_hijo_n(h, sub, (int)sizeof(sub), 1,
                                prof + 1);
                {
                    int q = 0;
                    while (sub[q] && t + 1 < max)
                        sal[t++] = sub[q++];
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
    sal[t] = 0;
}

static void nv_texto_hijo(dom_node *n, char *sal, int max, int pre) {
    nv_texto_hijo_n(n, sal, max, pre, 0);
}

/* formularios (v2.7): registra un <input> y pinta su caja en
   linea ([____], [Enviar], [X]). Tipos: texto, password, hidden,
   submit, checkbox (+button/image como texto/enviar). Sin nombre no
   viaja (salvo el boton que lo activa, si lo tiene). */
/* Registra el campo base (form, nombre, valor, marca) y devuelve
   su indice o -1 si no cabe. Guarda el valor inicial (reset). */
static int nv_campo_nuevo(int tp, const char *nombre,
                          const char *valor, int marca) {
    int ci, q;
    if (NV_ncampos >= NV_MAXCAMP) return -1;
    ci = NV_ncampos++;
    NV_campos[ci].form = NV_npila_form > 0 ?
        NV_pila_form[NV_npila_form - 1] : -1;
    NV_campos[ci].tipo = tp;
    q = 0;
    while (nombre[q] && q < 63) {
        NV_campos[ci].nombre[q] = nombre[q];
        q++;
    }
    NV_campos[ci].nombre[q] = 0;
    q = 0;
    while (valor[q] && q < NV_CAMPO_TAM) {
        NV_campos[ci].valor[q] = valor[q];
        NV_campos[ci].ini[q] = valor[q];
        q++;
    }
    NV_campos[ci].valor[q] = 0;
    NV_campos[ci].ini[q] = 0;
    NV_campos[ci].pos = q;
    NV_campos[ci].pini = q;
    NV_campos[ci].marca = marca ? 1 : 0;
    NV_campos[ci].mini = marca ? 1 : 0;
    NV_campos[ci].fila = -1;
    NV_campos[ci].col = 0;
    NV_campos[ci].ancho = 0;
    NV_campos[ci].alto = 1;
    NV_campos[ci].nopc = 0;
    return ci;
}

static void nv_campo(dom_node *h) {
    char tipo[16], nombre[64], valor[NV_CAMPO_TAM + 1], aux[16];
    int tp = 0, i, ci, col, est;
    if (nv_atributo(h, "type", tipo, (int)sizeof(tipo)))
        tipo[0] = 0;
    for (i = 0; tipo[i]; i++)
        if (tipo[i] >= 'A' && tipo[i] <= 'Z')
            tipo[i] += (char)('a' - 'A');
    if (!u_strcmp(tipo, "password")) tp = 1;
    else if (!u_strcmp(tipo, "hidden")) tp = 2;
    else if (!u_strcmp(tipo, "submit")) tp = 3;
    else if (!u_strcmp(tipo, "checkbox")) tp = 4;
    else if (!u_strcmp(tipo, "radio")) tp = 7;
    else if (!u_strcmp(tipo, "reset")) tp = 8;
    else if (!u_strcmp(tipo, "button")) tp = 9;
    else if (!u_strcmp(tipo, "image")) tp = 3;
    if (nv_atributo(h, "name", nombre, (int)sizeof(nombre)))
        nombre[0] = 0;
    if (nv_atributo(h, "value", valor, (int)sizeof(valor)))
        valor[0] = 0;
    ci = nv_campo_nuevo(tp, nombre, valor,
                        !nv_atributo(h, "checked", aux,
                                    (int)sizeof(aux)));
    if (ci < 0) return;
    if (tp == 7 && NV_campos[ci].marca) {
        /* radio marcado: desmarca al resto del grupo */
        int k;
        for (k = 0; k < NV_ncampos - 1; k++) {
            if (NV_campos[k].tipo == 7 &&
                NV_campos[k].form == NV_campos[ci].form &&
                !u_strcmp(NV_campos[k].nombre,
                          NV_campos[ci].nombre))
                NV_campos[k].marca = 0;
        }
    }
    if (tp == 2) return;   /* oculto: viaja pero no se pinta */
    if (tp == 4)
        est = 3;
    else if (tp == 7)
        est = 3;
    else if (tp == 3 || tp == 8 || tp == 9) {
        int q = 0;
        while (NV_campos[ci].valor[q]) q++;
        est = 4 + (NV_campos[ci].valor[0] ? q : 6);
    }
    else
        est = NV_CAMPO_ANCHO + 2;
    /* v2.8: la caja no se parte: si no cabe, salto antes (medido;
       est es su longitud exacta en bytes, NV_CELDA la estima) */
    if (NV_MAXW - nv_ancho_px() < (est + 1) * NV_CELDA)
        nv_salto();
    col = NV_ntxt - NV_filas[NV_nfilas].ini;
    NV_campos[ci].fila = NV_nfilas;
    NV_campos[ci].col = col;
    if (tp == 4) {
        nv_pon_texto((const unsigned char *)
                     (NV_campos[ci].marca ? "[X]" : "[ ]"), 3, 1);
        NV_campos[ci].ancho = 3;
        return;
    }
    if (tp == 7) {
        nv_pon_texto((const unsigned char *)
                     (NV_campos[ci].marca ? "(o)" : "( )"), 3, 1);
        NV_campos[ci].ancho = 3;
        return;
    }
    if (tp == 3 || tp == 8 || tp == 9) {
        const char *et = NV_campos[ci].valor[0] ?
            NV_campos[ci].valor :
            (tp == 8 ? "Restablecer" : "Enviar");
        char b[64];
        int j = 0, k = 0;
        b[j++] = '[';
        b[j++] = ' ';
        while (et[k] && j < 60) b[j++] = et[k++];
        b[j++] = ' ';
        b[j++] = ']';
        nv_pon_texto((const unsigned char *)b, j, 1);
        NV_campos[ci].ancho = j;
        return;
    }
    {
        char b[NV_CAMPO_ANCHO + 3];
        int j = 0, k = 0;
        b[j++] = '[';
        while (k < NV_CAMPO_ANCHO) {
            char c = NV_campos[ci].valor[k];
            if (!c) break;
            b[j++] = (tp == 1) ? (char)'*' : c;
            k++;
        }
        while (k < NV_CAMPO_ANCHO) {
            b[j++] = ' ';
            k++;
        }
        b[j++] = ']';
        nv_pon_texto((const unsigned char *)b, j, 1);
        NV_campos[ci].ancho = j;
    }
}

/* v2.7-fase2: <textarea>: bloque propio de alto filas, edicion
   multilinea. Valor inicial = texto interior (sin el primer salto
   de linea, como el HTML). */
static void nv_area(dom_node *h) {
    char nombre[64], valor[NV_CAMPO_TAM + 1];
    char filas[16], cols[16];
    int ci, f, r, vw, alto, n = 0;
    if (nv_atributo(h, "name", nombre, (int)sizeof(nombre)))
        nombre[0] = 0;
    nv_texto_hijo(h, valor, (int)sizeof(valor), 1);
    if (valor[0] == '\n') {
        while (valor[n]) {
            valor[n] = valor[n + 1];
            n++;
        }
    }
    alto = 3;
    if (!nv_atributo(h, "rows", filas, (int)sizeof(filas))) {
        alto = u_atoi(filas);
        if (alto < 1) alto = 1;
        if (alto > 6) alto = 6;
    }
    vw = NV_CAMPO_ANCHO;
    if (!nv_atributo(h, "cols", cols, (int)sizeof(cols))) {
        vw = u_atoi(cols);
        if (vw < 8) vw = 8;
        if (vw > 40) vw = 40;
    }
    ci = nv_campo_nuevo(5, nombre, valor, 0);
    if (ci < 0) return;
    NV_campos[ci].alto = alto;
    NV_campos[ci].ancho = vw + 2;
    nv_salto();
    f = NV_nfilas;
    NV_campos[ci].fila = f;
    NV_campos[ci].col = NV_ntxt - NV_filas[f].ini;
    for (r = 0; r < alto; r++) {
        int j = 0, k = 0;
        char b[44];
        b[j++] = '[';
        while (k < vw) {
            b[j++] = ' ';
            k++;
        }
        b[j++] = ']';
        nv_pon_texto((const unsigned char *)b, j, 1);
        nv_salto();
    }
}

/* v2.7-fase2: <select>: cerrado, se cicla con arriba/abajo.
   Recoge las <option> hijas (valor o texto, selected). */
static void nv_lista(dom_node *h) {
    char nombre[64];
    char ops[6][32];
    int ci, no = 0, sel = 0, hay = 0, maxlen = 0;
    dom_node *c = 0;
    if (nv_atributo(h, "name", nombre, (int)sizeof(nombre)))
        nombre[0] = 0;
    if (dom_node_get_first_child(h, &c) == DOM_NO_ERR && c) {
        for (;;) {
            dom_node_type tt = (dom_node_type)0;
            if (dom_node_get_node_type(c, &tt) == DOM_NO_ERR &&
                tt == DOM_ELEMENT_NODE) {
                char nb[16];
                if (!nv_nom_el(c, nb) &&
                    nv_id_nom(nb) == T_OPTION && no < 6) {
                    char val[64], aux[16], txt[64];
                    int has = 0;
                    if (!nv_atributo(c, "value", val,
                                     (int)sizeof(val)))
                        has = 1;
                    nv_texto_hijo(c, txt, (int)sizeof(txt), 0);
                    {
                        const char *s = has ? val : txt;
                        int k = 0;
                        while (s[k] && k < 31) {
                            ops[no][k] = s[k];
                            k++;
                        }
                        ops[no][k] = 0;
                        if (k > maxlen) maxlen = k;
                    }
                    if (!nv_atributo(c, "selected", aux,
                                     (int)sizeof(aux))) {
                        sel = no;
                        hay = 1;
                    }
                    no++;
                }
            }
            {
                dom_node *s = 0;
                if (dom_node_get_next_sibling(c, &s) != DOM_NO_ERR ||
                    !s) {
                    dom_node_unref(c);
                    break;
                }
                dom_node_unref(c);
                c = s;
            }
        }
    }
    (void)hay;
    if (no == 0) return;
    ci = nv_campo_nuevo(6, nombre, ops[sel], 0);
    if (ci < 0) return;
    {
        int k;
        for (k = 0; k < no; k++) {
            int q = 0;
            while (ops[k][q] && q < 31) {
                NV_campos[ci].ops[k][q] = ops[k][q];
                q++;
            }
            NV_campos[ci].ops[k][q] = 0;
        }
    }
    NV_campos[ci].nopc = no;
    NV_campos[ci].pos = sel;
    NV_campos[ci].pini = sel;
    if (maxlen > 24) maxlen = 24;
    {
        int est = maxlen + 5, col;
        if (NV_MAXW - nv_ancho_px() < (est + 1) * NV_CELDA)
            nv_salto();
        col = NV_ntxt - NV_filas[NV_nfilas].ini;
        NV_campos[ci].fila = NV_nfilas;
        NV_campos[ci].col = col;
        NV_campos[ci].ancho = est;
        {
            char b[36];
            int j = 0;
            b[j++] = '[';
            b[j++] = ' ';
            while (j < est - 2) b[j++] = ' ';
            b[j++] = 'v';
            b[j++] = ']';
            nv_pon_texto((const unsigned char *)b, j, 1);
        }
    }
}

/* v2.7-fase2: <button>: como enviar con la etiqueta del contenido. */
static void nv_boton(dom_node *h) {
    char tipo[16], nombre[64], valor[64], txt[64];
    int tp = 3;
    if (nv_atributo(h, "type", tipo, (int)sizeof(tipo)))
        tipo[0] = 0;
    {
        int i;
        for (i = 0; tipo[i]; i++)
            if (tipo[i] >= 'A' && tipo[i] <= 'Z')
                tipo[i] += (char)('a' - 'A');
    }
    if (!u_strcmp(tipo, "reset")) tp = 8;
    else if (!u_strcmp(tipo, "button")) tp = 9;
    if (nv_atributo(h, "name", nombre, (int)sizeof(nombre)))
        nombre[0] = 0;
    nv_texto_hijo(h, txt, (int)sizeof(txt), 0);
    if (nv_atributo(h, "value", valor, (int)sizeof(valor)))
        valor[0] = 0;
    {
        char b[64];
        const char *et = valor[0] ? valor :
            (txt[0] ? txt : "Enviar");
        int j = 0, k = 0, ci, est;
        while (et[k] && k < 56) k++;
        est = k + 4;
        if (NV_MAXW - nv_ancho_px() < (est + 1) * NV_CELDA)
            nv_salto();
        ci = nv_campo_nuevo(tp, nombre, valor, 0);
        if (ci < 0) return;
        NV_campos[ci].fila = NV_nfilas;
        NV_campos[ci].col = NV_ntxt - NV_filas[NV_nfilas].ini;
        b[j++] = '[';
        b[j++] = ' ';
        k = 0;
        while (et[k] && j < 60) b[j++] = et[k++];
        b[j++] = ' ';
        b[j++] = ']';
        nv_pon_texto((const unsigned char *)b, j, 1);
        NV_campos[ci].ancho = j;
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
                        } else if (id == T_INPUT) {
                            nv_campo(h);
                        }
                    } else {
                        int fg0 = NV_fg, bg0 = NV_bg;
                        int al0 = NV_al, ebg = 0, eal = -1;
                        int es_bloque = nv_es_bloque(id);
                        int es_enl = 0, ienl = -1, ntxt0 = 0;
                        int es_sub = 0, es_bold = 0, es_h = 0;
                        int es_form = 0, es_salta = 0;
                        nv_estilo_nodo(h);
                        ebg = NV_last_bgexp;
                        eal = NV_last_al;
                        if (NV_last_bold) {
                            es_bold = 1;
                            NV_b++;
                        }
                        /* Hito 6: fondo explicito y alineacion
                           rigen el subarbol del bloque */
                        if (es_bloque) {
                            if (ebg) NV_bgexp++;
                            if (eal >= 0) NV_al = eal;
                        }
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
                                NV_enl[ienl].c0 =
                                    NV_ntxt -
                                    NV_filas[NV_nfilas].ini;
                                NV_enl[ienl].c1 = -1;
                                while (NV_tmpurl[q] && q < 255) {
                                    NV_enl[ienl].url[q] =
                                        NV_tmpurl[q];
                                    q++;
                                }
                                NV_enl[ienl].url[q] = 0;
                                es_enl = 1;
                                ntxt0 = NV_ntxt;
                            }
                            /* Hito 6: subraya si la cascada lo pide
                               (hoja UA por defecto) */
                            if (NV_last_dec &
                                CSS_TEXT_DECORATION_UNDERLINE) {
                                es_sub = 1;
                                NV_sub++;
                            }
                        }
                        /* v2.7: abre el formulario (action/method);
                           los <input> interiores se le asignan */
                        if (id == T_FORM) {
                            char acc[NV_MAXURL + 1], met[16];
                            int fi = -1, w2 = 0;
                            if (NV_nforms < NV_MAXFORM) {
                                fi = NV_nforms++;
                                if (!nv_atributo(h, "action", acc,
                                                 (int)sizeof(acc)) &&
                                    acc[0]) {
                                    if (nv_resuelve(acc, NV_tmpurl)) {
                                        int v = 0;
                                        while (NV_base[v] &&
                                               v < NV_MAXURL) {
                                            NV_tmpurl[v] =
                                                NV_base[v];
                                            v++;
                                        }
                                        NV_tmpurl[v] = 0;
                                    }
                                    while (NV_tmpurl[w2] &&
                                           w2 < NV_MAXURL) {
                                        NV_forms[fi].accion[w2] =
                                            NV_tmpurl[w2];
                                        w2++;
                                    }
                                    NV_forms[fi].accion[w2] = 0;
                                } else {
                                    while (NV_base[w2] &&
                                           w2 < NV_MAXURL) {
                                        NV_forms[fi].accion[w2] =
                                            NV_base[w2];
                                        w2++;
                                    }
                                    NV_forms[fi].accion[w2] = 0;
                                }
                                if (!nv_atributo(h, "method", met,
                                                 (int)sizeof(met))) {
                                    int z = 0;
                                    while (met[z]) {
                                        if (met[z] >= 'A' &&
                                            met[z] <= 'Z')
                                            met[z] +=
                                                (char)('a' - 'A');
                                        z++;
                                    }
                                    NV_forms[fi].metodo =
                                        !u_strcmp(met, "post") ?
                                        1 : 0;
                                } else {
                                    NV_forms[fi].metodo = 0;
                                }
                                if (NV_npila_form < 4) {
                                    NV_pila_form[NV_npila_form++] =
                                        fi;
                                    es_form = 1;
                                }
                            }
                        }
                        if (id == T_LI) {
                            nv_bloque();
                            nv_pon_texto(
                                (const unsigned char *)"  * ",
                                4, 1);
                        } else if (id == T_TEXTAREA) {
                            /* v2.7-fase2: bloque propio, no se
                               recorre (el texto ya es el valor) */
                            nv_area(h);
                            es_salta = 1;
                        } else if (id == T_SELECT) {
                            /* v2.7-fase2: cerrado, recoge opciones */
                            nv_lista(h);
                            es_salta = 1;
                        } else if (id == T_BUTTON) {
                            /* v2.7-fase2: como enviar con etiqueta */
                            nv_boton(h);
                            es_salta = 1;
                        } else if (es_bloque) {
                            nv_bloque();
                        }
                        {
                            int fg1 = NV_fg, bg1 = NV_bg;
                            int al1 = NV_al;
                            /* v2.5-ux: titulares en color (marca o CSS),
                               sin doble pasada: la 8x16 ya es gruesa. */
                            es_h = (id == T_H1 || id == T_H2);
                            if (es_h) {
                                if (!NV_last_fgset)
                                    NV_fg = NS_TITULO;
                            }
                            if (!es_salta)
                                nv_recorre(h, prof + 1);
                            NV_fg = fg1;
                            NV_bg = bg1;
                            NV_al = al1;
                        }
                        /* v2.7: cierra el formulario (los <input>
                           interiores ya quedaron asignados) */
                        if (es_form && NV_npila_form > 0)
                            NV_npila_form--;
                        if (es_bloque)
                            nv_salto();
                        if (es_sub)
                            NV_sub--;
                        if (es_bold)
                            NV_b--;
                        if (es_bloque) {
                            if (ebg) NV_bgexp--;
                            NV_al = al0;
                        }
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
                                NV_enl[ienl].c1 = NV_ntxt -
                                    NV_filas[f1].ini;
                            }
                        }
                        NV_fg = fg0;
                        NV_bg = bg0;
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

/* puente hubbub->dom global (evita 24K en pila) */
static nv_t NV_pu;

/* v2.5: hojas externas <link rel="stylesheet"> (max 2, 6K cada una).
   Se traen tras el parse (el DOM ya esta en heap; nsfetch puede
   reusar su bufer estatico sin pisar la pagina). */
#define NV_MAXLINK 2
#define NV_LINKCAP 6144
static int NV_nlink;

static void nv_css_link_uno(dom_node *h) {
    char rel[32], href[256];
    nsfetch_res_t r;
    int i, n, k;
    if (NV_nlink >= NV_MAXLINK) return;
    if (nv_atributo(h, "rel", rel, (int)sizeof(rel))) return;
    {
        const char *q = "stylesheet";
        k = 0;
        while (q[k] && rel[k] &&
               (rel[k] == q[k] || rel[k] + 32 == q[k]))
            k++;
        if (q[k] || rel[k]) return;
    }
    if (nv_atributo(h, "href", href, (int)sizeof(href))) return;
    if (nv_resuelve(href, NV_tmpurl) || !nv_es_url(NV_tmpurl))
        return;
    if (nsfetch_get_ck(NV_tmpurl, &r) != NSFETCH_OK) return;
    if (r.codigo < 200 || r.codigo > 299 || r.tam <= 0) return;
    n = r.tam > NV_LINKCAP ? NV_LINKCAP : r.tam;
    if (NV_pu.nestilo + n + 2 >= NV_MAXCSS) return;
    NV_pu.estilo[NV_pu.nestilo++] = '\n';
    for (i = 0; i < n; i++)
        NV_pu.estilo[NV_pu.nestilo++] = (char)r.cuerpo[i];
    NV_pu.estilo[NV_pu.nestilo++] = '\n';
    NV_pu.estilo[NV_pu.nestilo] = 0;
    NV_nlink++;
}

static void nv_css_links(dom_node *n, int prof) {
    dom_node *h = 0;
    if (!n || prof > 32 || NV_nlink >= NV_MAXLINK) return;
    if (dom_node_get_first_child(n, &h) != DOM_NO_ERR || !h)
        return;
    for (;;) {
        dom_node_type t = (dom_node_type)0;
        if (dom_node_get_node_type(h, &t) == DOM_NO_ERR &&
            t == DOM_ELEMENT_NODE) {
            char nb[16];
            if (!nv_nom_el(h, nb) && nv_id_nom(nb) == T_LINK)
                nv_css_link_uno(h);
            nv_css_links(h, prof + 1);
            if (NV_nlink >= NV_MAXLINK) {
                dom_node_unref(h);
                return;
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
    NV_pend_esp = 0;
    NV_nenl = 0;
    NV_foco_enl = -1;
    NV_nforms = 0;
    NV_ncampos = 0;
    NV_foco_campo = -1;
    NV_npila_form = 0;
    NV_ninline = 0;
    nv_ref_libera();
    NV_sub = 0;
    NV_bgexp = 0;
    NV_al = 0;
    NV_b = 0;
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
    /* CSS del <style> + <link rel=stylesheet> (si falla,
       sigue sin estilos) */
    nv_sel_init();
    NV_nlink = 0;
    nv_css_links((dom_node *)doc, 0);
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

/* v2.7: si el cuerpo no parece HTML (texto plano, JSON, errores
   de API...), se envuelve en <pre> escapando &<>: sin esto el texto
   sin etiquetas no llegaba al arbol y la pagina salia vacia. */
#define NV_PLANO_MAX 65536
static unsigned char NV_plano[NV_PLANO_MAX];

static int nv_parece_html(const unsigned char *pg, int n) {
    int i = 0;
    if (n >= 3 && pg[0] == 0xEF && pg[1] == 0xBB && pg[2] == 0xBF)
        i = 3;   /* BOM UTF-8 fuera */
    while (i < n && (pg[i] == ' ' || pg[i] == '\t' ||
                     pg[i] == '\r' || pg[i] == '\n'))
        i++;
    return i < n && pg[i] == '<';
}

static int nv_procesa_auto(const unsigned char *pg, int n) {
    int i = 0, t = 0, k;
    static const char pre[] =
        "<html><head><title>texto</title></head><body><pre>";
    static const char post[] = "</pre></body></html>";
    if (n < 0 || !pg) return -1;
    if (nv_parece_html(pg, n))
        return nv_procesa(pg, n);
    while (pre[t]) {
        if (t + 1 >= (int)sizeof(NV_plano)) return -1;
        NV_plano[t] = (unsigned char)pre[t];
        t++;
    }
    for (k = 0; k < (int)sizeof(post); k++) ;
    for (i = 0; i < n; i++) {
        unsigned char c = pg[i];
        const char *ent = 0;
        if (c == '&') ent = "&amp;";
        else if (c == '<') ent = "&lt;";
        else if (c == '>') ent = "&gt;";
        if (ent) {
            int j = 0;
            while (ent[j]) {
                if (t + 1 >= (int)sizeof(NV_plano) - 24)
                    break;
                NV_plano[t++] = (unsigned char)ent[j++];
            }
            if (ent[j]) break;
        } else {
            if (t + 1 >= (int)sizeof(NV_plano) - 24) break;
            NV_plano[t++] = c;
        }
    }
    k = 0;
    while (post[k]) {
        if (t + 1 >= (int)sizeof(NV_plano)) break;
        NV_plano[t++] = (unsigned char)post[k++];
    }
    NV_plano[t] = 0;
    return nv_procesa(NV_plano, t);
}
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

/* ---- carga: URL (nsfetch) o fichero local ---- */
static int nv_es_url(const char *s);
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
    if (!u_strcmp(f, NV_BIENV_URL)) {
        int i = 0;
        while (NV_BIENV_HTML[i]) i++;
        NV_ult_cod = 200;
        NV_ult_tam = i;
        pg = (const unsigned char *)NV_BIENV_HTML;
        n = i;
        rc = nv_procesa_auto(pg, n);
        nv_css_fin();
        return rc;
    }
    if (nv_es_url(f)) {
        nsfetch_res_t r;
        int e = nsfetch_get_ck(f, &r);
        if (e != NSFETCH_OK) return -10 - e;
        NV_ult_cod = r.codigo;
        NV_ult_tam = r.tam;
        if (r.codigo < 200 || r.codigo > 299) return -20;
        pg = (const unsigned char *)r.cuerpo;
        n = r.tam;
        /* base para resolver relativas (tambien en -p, que no
           pasa por nv_navegar) */
        {
            int i = 0;
            while (f[i] && f[i] != '#' && i < NV_MAXURL) {
                NV_base[i] = f[i];
                i++;
            }
            NV_base[i] = 0;
        }
        rc = nv_procesa_auto(pg, n);
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
        rc = nv_procesa_auto(NV_txt, tam);
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

/* Hito 6: la barra es buscador si no parece URL.
   Sin esquema y sin puntos ni barras: consulta al motor.
   v2.5: old-search responde GET simple con resultados reales
   y sin reto JS (Marginalia principal exige JS; DDG Lite pone
   captcha segun IP). */
#define NV_BUSCA_BASE "https://old-search.marginalia.nu/search?query="

static int nv_es_busqueda(const char *s) {
    int i = 0;
    while (s[i] == ' ') i++;
    if (!s[i]) return 0;
    while (s[i]) {
        if (s[i] == ':') return 0;
        if (s[i] == '/' || s[i] == '.') return 0;
        if (s[i] == ' ') return 1;
        i++;
    }
    return 1;
}

static void nv_busca_url(const char *q, char *sal) {
    int t = 0, i = 0;
    static const char *hex = "0123456789ABCDEF";
    while (NV_BUSCA_BASE[t]) {
        sal[t] = NV_BUSCA_BASE[t];
        t++;
    }
    while (q[i] == ' ') i++;
    while (q[i] && t < NV_MAXURL - 4) {
        unsigned char c = (unsigned char)q[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            sal[t++] = (char)c;
            i++;
        } else if (c == ' ') {
            sal[t++] = '+';
            i++;
        } else {
            if (t + 3 >= NV_MAXURL) break;
            sal[t++] = '%';
            sal[t++] = hex[(c >> 4) & 15];
            sal[t++] = hex[c & 15];
            i++;
        }
    }
    while (t > 0 && sal[t - 1] == '+') t--;
    sal[t] = 0;
}

/* entrada de la barra / arranque: URL, dominio o busqueda */
static void nv_ir(const char *texto);

/* -p [url]: sin url = bateria contra stubs; con url = resumen */
static void nv_envia_form(int fi, int ci_boton);
static void nv_pide_post(const char *url, const char *qs, int empuja);
static void nv_campo_radio(int ci);
static void nv_campo_repone(int fi);

static int nv_modo_p(const char *resto) {
    while (*resto == ' ') resto++;
    if (*resto == 0) {
        /* bateria: necesita http_stub en 18080 (ftest lo arranca) */
        u_print("netsurf: bateria Hito 6...\n");
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
        /* v2.5: hoja externa <link rel=stylesheet> */
        nv_ok(nv_pagina("http://10.0.2.2:18080/con-css") == 0,
              "csslink-carga");
        nv_ok(!u_strcmp(NV_titulo, "ConCSS"), "csslink-titulo");
        nv_ok(NV_nestilos >= 2, "csslink-estilos");
        nv_ok(nv_contiene("Verde externo"), "csslink-texto");
        /* v2.8: galletas (/galleta fija dos, /eco-cookie las devuelve) */
        nv_navegar("http://10.0.2.2:18080/galleta", 1);
        nv_ok(NV_tiene, "galleta-carga");
        nv_navegar("http://10.0.2.2:18080/eco-cookie", 1);
        nv_ok(NV_tiene && nv_contiene("sesion=abc123"),
              "galleta-sesion");
        nv_ok(nv_contiene("tema=oscuro"), "galleta-tema");
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
        /* v2.7: formularios POST (/form-post -> /eco-post) */
        nv_navegar("http://10.0.2.2:18080/form-post", 1);
        nv_ok(NV_tiene, "form-carga");
        nv_ok(NV_ncampos == 1, "form-ncampos");
        nv_ok(!u_strcmp(NV_campos[0].nombre, "q"), "form-nombre");
        nv_ok(NV_campos[0].tipo == 0, "form-tipo");
        nv_ok(NV_nforms == 1 && NV_forms[0].metodo == 1,
              "form-post");
        nv_ok(!u_strcmp(NV_forms[0].accion,
                        "http://10.0.2.2:18080/eco-post"),
              "form-accion");
        {
            int i = 0;
            const char *t = "hola mundo";
            while (t[i] && i < NV_CAMPO_TAM) {
                NV_campos[0].valor[i] = t[i];
                i++;
            }
            NV_campos[0].valor[i] = 0;
            NV_campos[0].pos = i;
        }
        nv_envia_form(0, -1);
        nv_ok(NV_tiene && nv_contiene("q=hola+mundo"),
              "form-post-eco");
        /* v2.7: formularios GET (/form-get -> /eco-get) */
        nv_navegar("http://10.0.2.2:18080/form-get", 1);
        nv_ok(NV_tiene, "formg-carga");
        nv_ok(NV_ncampos == 3, "formg-ncampos");
        nv_ok(NV_campos[1].tipo == 4 && NV_campos[1].marca,
              "formg-caja");
        nv_ok(NV_campos[2].tipo == 3, "formg-enviar");
        {
            int i = 0;
            const char *t = "hola mundo";
            while (t[i] && i < NV_CAMPO_TAM) {
                NV_campos[0].valor[i] = t[i];
                i++;
            }
            NV_campos[0].valor[i] = 0;
            NV_campos[0].pos = i;
        }
        nv_envia_form(0, 2);
        nv_ok(NV_tiene && nv_contiene("q=hola+mundo") &&
              nv_contiene("ok=si"), "formg-eco");
        nv_navegar("http://10.0.2.2:18080/form-get", 1);
        {
            int i = 0;
            const char *t = "hola mundo";
            while (t[i] && i < NV_CAMPO_TAM) {
                NV_campos[0].valor[i] = t[i];
                i++;
            }
            NV_campos[0].valor[i] = 0;
            NV_campos[0].pos = i;
        }
        NV_campos[1].marca = 0;
        nv_envia_form(0, 2);
        nv_ok(NV_tiene && nv_contiene("q=hola+mundo") &&
              !nv_contiene("ok="), "formg-sin-caja");
        /* v2.7-fase2: area + lista + radios + reset (/form2) */
        nv_navegar("http://10.0.2.2:18080/form2", 1);
        nv_ok(NV_tiene, "form2-carga");
        nv_ok(NV_ncampos == 6, "form2-ncampos");
        nv_ok(NV_campos[0].tipo == 5, "form2-area");
        nv_ok(NV_campos[0].alto == 3 &&
              !u_strcmp(NV_campos[0].valor, "hola"),
              "form2-area-valor");
        nv_ok(NV_campos[1].tipo == 6 &&
              NV_campos[1].nopc == 3, "form2-lista");
        nv_ok(NV_campos[1].pos == 1, "form2-lista-sel");
        nv_ok(NV_campos[2].tipo == 7 &&
              !NV_campos[2].marca, "form2-radio-a");
        nv_ok(NV_campos[3].tipo == 7 && NV_campos[3].marca,
              "form2-radio-b");
        nv_ok(NV_campos[4].tipo == 8, "form2-reset");
        nv_ok(NV_campos[5].tipo == 3, "form2-enviar");
        {
            int i = 0;
            const char *t = "a\nb";
            while (t[i] && i < NV_CAMPO_TAM) {
                NV_campos[0].valor[i] = t[i];
                i++;
            }
            NV_campos[0].valor[i] = 0;
            NV_campos[0].pos = i;
        }
        NV_campos[1].pos = 2;
        nv_campo_radio(2);
        nv_envia_form(0, 5);
        nv_ok(NV_tiene && nv_contiene("txt=a%0D%0Ab") &&
              nv_contiene("col=azul") &&
              nv_contiene("r=a"), "form2-eco");
        nv_navegar("http://10.0.2.2:18080/form2", 1);
        {
            int i = 0;
            const char *t = "cambiado";
            while (t[i] && i < NV_CAMPO_TAM) {
                NV_campos[0].valor[i] = t[i];
                i++;
            }
            NV_campos[0].valor[i] = 0;
        }
        NV_campos[1].pos = 0;
        nv_campo_repone(0);
        nv_ok(!u_strcmp(NV_campos[0].valor, "hola") &&
              NV_campos[1].pos == 1 &&
              !NV_campos[2].marca && NV_campos[3].marca,
              "form2-reset");
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
        /* Hito 6: fidelidad NetSurf (hoja UA) sobre /enlaces */
        nv_navegar("http://10.0.2.2:18080/enlaces", 1);
        {
            int f, k, azul = 0, sub = 0;
            for (f = 0; f < NV_nfilas; f++) {
                int m = NV_filas[f].len;
                for (k = 0; k + 8 <= m; k++) {
                    if (!u_memcmp((const char *)NV_txt +
                                  NV_filas[f].ini + k,
                                  "absoluto", 8)) {
                        if (NV_filas[f].fg == 0x0000EE)
                            azul = 1;
                        if (NV_filas[f].sub)
                            sub = 1;
                    }
                }
            }
            nv_ok(azul, "ua-azul");
            nv_ok(sub, "ua-sub");
        }
        /* Hito 6: fondo a ancho completo + negrita en /css */
        nv_navegar("http://10.0.2.2:18080/css", 1);
        {
            int f, k, lleno = 0, neg = 0;
            for (f = 0; f < NV_nfilas; f++) {
                int m = NV_filas[f].len;
                for (k = 0; k + 7 <= m; k++) {
                    if (k + 8 <= m &&
                        !u_memcmp((const char *)NV_txt +
                                   NV_filas[f].ini + k,
                                   "Cabecera", 8)) {
                        if (NV_filas[f].full)
                            lleno = 1;
                    }
                    if (!u_memcmp((const char *)NV_txt +
                                  NV_filas[f].ini + k,
                                  "Negrita", 7)) {
                        if (NV_filas[f].bd)
                            neg = 1;
                    }
                }
            }
            nv_ok(lleno, "ua-lleno");
            nv_ok(neg, "ua-neg");
        }
        /* Hito 6: buscador de la barra (puro, sin red) */
        {
            char u[NV_MAXURL + 1];
            nv_busca_url("hola mundo", u);
            nv_ok(!u_strcmp(u, "https://old-search.marginalia.nu"
                               "/search?query=hola+mundo"),
                  "busca-url");
            nv_ok(nv_es_busqueda("hola mundo") &&
                  !nv_es_busqueda("http://x/y") &&
                  !nv_es_busqueda("example.com"),
                  "busca-det");
        }
        /* Hito 6: clase sobre div (regresion del aliasing) */
        {
            static const char m1[] =
                "<html><head><style>.x{color:red}</style>"
                "</head><body><div class=\"x\">T</div></body></html>";
            int k, ok = 0;
            nv_procesa((const unsigned char *)m1,
                       (int)sizeof(m1) - 1);
            nv_css_fin();
            for (k = 0; k < NV_nfilas; k++)
                if (NV_filas[k].fg == 0xFF0000) ok = 1;
            nv_ok(ok, "ua-clsdiv");
        }
        u_print("netsurf: ");
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
            u_print("netsurf: no se pudo cargar (");
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
/* Buscador integrado de la bienvenida: casilla central. */
static char NV_qbuf[128];
static int NV_qpos, NV_qfoco;
/* 1 si la pagina visible es la bienvenida interna. */
static int nv_bienv(void) {
    return NV_tiene && !u_strcmp(NV_url, NV_BIENV_URL);
}
/* Geometria del panel de busqueda (compartida dibujo/clic). */
static void nv_qgeom(int w, int *fx, int *fy, int *fw, int *bx) {
    int fww = w - 220;
    if (fww < 200) fww = 200;
    if (fww > 420) fww = 420;
    *fx = (w - fww - 76) / 2;
    if (*fx < 8) *fx = 8;
    *fy = NV_BAR_H + 30;
    *fw = fww;
    *bx = *fx + fww + 8;
}
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
/* Navegacion en dos fases (v2.6): el fetch bloquea segundos y sin
   avisar la ventana parece congelada. Se guarda la peticion, se pinta
   "cargando..." y la descarga corre en el siguiente DRAW. */
static char NV_pend[NV_MAXURL + 1];
static int NV_pend_empuja = 0, NV_hay_pend = 0, NV_pend_fase = 0;
/* 1 en el bucle grafico (main); 0 en -p: sin bucle de eventos el
   pendiente nunca se ejecutaria, asi que nv_pide navega en el acto. */
static int NV_gui = 0;
/* POST diferido: cuerpo y marca (van con el pendiente). */
static char NV_post_cuerpo[2048];
static int NV_pend_post = 0, NV_post_n = 0;
static int nv_envia_post(const char *url, const char *qs);

static void nv_pide(const char *f, int empuja) {
    int i = 0;
    if (!f || !f[0]) return;
    if (!NV_gui) {
        /* -p: sin bucle, en el acto (mismo efecto que antes). */
        nv_navegar(f, empuja);
        return;
    }
    while (f[i] && i < NV_MAXURL) {
        NV_pend[i] = f[i];
        i++;
    }
    NV_pend[i] = 0;
    NV_pend_empuja = empuja ? 1 : 0;
    NV_hay_pend = 1;
    NV_pend_fase = 0;   /* fase 0: pintar "cargando..."; fase 1: descargar */
    NV_pend_post = 0;
    nv_pon_estado("cargando...");
    ug_tick(&NV_v, 1);
}

/* Codifica application/x-www-form-urlencoded (espacio -> +,
   salto de linea -> %0D%0A como manda el formulario). */
static void nv_urlcod(const char *s, char *d, int max) {
    int i = 0, j = 0;
    static const char hx[] = "0123456789ABCDEF";
    while (s[i] && j + 1 < max) {
        unsigned char c = (unsigned char)s[i++];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            d[j++] = (char)c;
        } else if (c == ' ') {
            d[j++] = '+';
        } else if (c == '\n') {
            if (j + 6 >= max) break;
            d[j++] = '%';
            d[j++] = '0';
            d[j++] = 'D';
            d[j++] = '%';
            d[j++] = '0';
            d[j++] = 'A';
        } else if (c == '\r') {
            if (j + 3 >= max) break;
            d[j++] = '%';
            d[j++] = '0';
            d[j++] = 'D';
        } else if (j + 3 < max) {
            d[j++] = '%';
            d[j++] = hx[(c >> 4) & 15];
            d[j++] = hx[c & 15];
        } else {
            break;
        }
    }
    d[j] = 0;
}

/* Junta los campos del formulario y lo envia (GET = navega a la URL
   con consulta; POST = cuerpo). ci_boton = boton que lo activo
   (manda su par solo si tiene nombre). */
static void nv_envia_form(int fi, int ci_boton) {
    char qs[2048], nb[128], vb[256];
    int p = 0, i, k;
    if (fi < 0 || fi >= NV_nforms) return;
    for (i = 0; i < NV_ncampos; i++) {
        nv_campo_t *c = &NV_campos[i];
        if (c->form != fi || !c->nombre[0]) continue;
        if (c->tipo == 8 || c->tipo == 9) continue;   /* nunca viajan */
        if ((c->tipo == 4 || c->tipo == 7) && !c->marca) continue;
        if (c->tipo == 3 && i != ci_boton) continue;
        if (p > 0 && p < 2046) qs[p++] = '&';
        nv_urlcod(c->nombre, nb, (int)sizeof(nb));
        k = 0;
        while (nb[k] && p < 2046) qs[p++] = nb[k++];
        if (p < 2046) qs[p++] = '=';
        if (c->tipo == 6 && c->pos >= 0 && c->pos < c->nopc)
            nv_urlcod(c->ops[c->pos], vb, (int)sizeof(vb));
        else
            nv_urlcod((c->tipo == 4 || c->tipo == 7) ?
                      (c->valor[0] ? c->valor : "on") : c->valor,
                      vb, (int)sizeof(vb));
        k = 0;
        while (vb[k] && p < 2046) qs[p++] = vb[k++];
    }
    qs[p] = 0;
    if (NV_forms[fi].metodo == 0) {
        char url[NV_MAXURL + 1];
        int u = 0, hay_q = 0;
        while (NV_forms[fi].accion[u] && u < NV_MAXURL) {
            if (NV_forms[fi].accion[u] == '?') hay_q = 1;
            url[u] = NV_forms[fi].accion[u];
            u++;
        }
        if (u < NV_MAXURL) url[u++] = hay_q ? (char)'&' : (char)'?';
        k = 0;
        while (qs[k] && u < NV_MAXURL) {
            url[u++] = qs[k++];
        }
        url[u] = 0;
        nv_pide(url, 1);
    } else {
        nv_pide_post(NV_forms[fi].accion, qs, 1);
    }
}

/* POST diferido: mismo mecanismo que nv_pide pero con cuerpo. */
static void nv_pide_post(const char *url, const char *qs, int empuja) {
    int i = 0, n = 0;
    if (!url || !url[0]) return;
    if (!NV_gui) {
        /* -p: sin bucle, en el acto. */
        {
            char tmp[NV_MAXURL + 1];
            int k = 0;
            while (url[k] && k < NV_MAXURL) {
                tmp[k] = url[k];
                k++;
            }
            tmp[k] = 0;
            nv_envia_post(tmp, qs ? qs : "");
        }
        (void)empuja;
        return;
    }
    while (url[i] && i < NV_MAXURL) {
        NV_pend[i] = url[i];
        i++;
    }
    NV_pend[i] = 0;
    while (qs && qs[n] && n < 2047) {
        NV_post_cuerpo[n] = qs[n];
        n++;
    }
    NV_post_cuerpo[n] = 0;
    NV_post_n = n;
    NV_pend_empuja = empuja ? 1 : 0;
    NV_hay_pend = 1;
    NV_pend_fase = 0;
    NV_pend_post = 1;
    nv_pon_estado("cargando...");
    ug_tick(&NV_v, 1);
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

/* entrada de la barra / arranque: URL, dominio o busqueda */
static void nv_ir(const char *texto) {
    const char *s = texto;
    while (*s == ' ') s++;
    if (!*s) return;
    if (!u_strcmp(s, NV_BIENV_URL)) {
        nv_pide(s, 1);
        return;
    }
    if (nv_es_url(s)) {
        nv_pide(s, 1);
        return;
    }
    if (nv_es_busqueda(s)) {
        nv_busca_url(s, NV_tmpurl);
        nv_pide(NV_tmpurl, 1);
        return;
    }
    {
        int i = 0, j = 0;
        static char con_esquema[NV_MAXURL + 1];
        const char *pre = "http://";
        while (pre[i]) {
            con_esquema[i] = pre[i];
            i++;
        }
        while (s[j] && i < NV_MAXURL) {
            con_esquema[i++] = s[j++];
        }
        con_esquema[i] = 0;
        nv_pide(con_esquema, 1);
    }
}

static void nv_hist_ir(int d);
static void nv_registra(const char *f, int rc, int empuja, int ms);

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
        if (ms < 0) ms = 0;
        nv_registra(f, rc, empuja, ms);
    }
}

/* Cola comun tras cargar (GET de nv_cargar o POST de nv_envia_post):
   NV_ult_* ya puestos por quien cargo; NV_qfoco, estado, historial
   y tick van aqui para no duplicarlos. */
static void nv_registra(const char *f, int rc, int empuja, int ms) {
    {
        int p = 0, q;
        /* La bienvenida deja el foco en su casilla de busqueda. */
        NV_qfoco = !u_strcmp(f, NV_BIENV_URL);
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
        NV_foco_campo = -1;
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
        NV_foco_campo = -1;
        if (rc <= -10 && rc >= -19)
            nv_pon_estado("error de red");
        else if (rc == -20)
            nv_pon_estado("pagina no encontrada");
        else
            nv_pon_estado("no se pudo cargar");
    }
    /* Hito 6: el contenido solo se compone en DRAW del servidor;
       el dibujo explicito no pega. Se pide un DRAW con TICK. */
    ug_tick(&NV_v, 1);
}

/* Respuesta POST: usa HCUERPO via nsfetch_post_ck (mismo tope que
   GET); sin bufer propio. */

/* Envia un formulario por POST y registra la respuesta como pagina.
   Devuelve como nv_cargar (0 ok, -20 no-2xx, resto error). */
static int nv_envia_post(const char *url, const char *qs) {
    nsfetch_res_t r;
    int n = 0, rc, t0, i;
    static const char ct[] =
        "Content-Type: application/x-www-form-urlencoded\r\n";
    while (qs[n] && n < 60000) n++;
    /* base para resolver relativas de la respuesta */
    i = 0;
    while (url[i] && url[i] != '#' && i < NV_MAXURL) {
        NV_base[i] = url[i];
        i++;
    }
    NV_base[i] = 0;
    NV_ult_cod = 0;
    NV_ult_tam = 0;
    t0 = u_ticks();
    rc = nsfetch_post_ck(url, (const unsigned char *)qs, n, ct, &r);
    if (rc != 0) return -10 - (rc < 0 ? -rc : rc);
    if (r.codigo < 200 || r.codigo > 299) return -20;
    NV_ult_cod = r.codigo;
    NV_ult_tam = r.tam;
    rc = nv_procesa_auto((const unsigned char *)r.cuerpo, r.tam);
    nv_css_fin();
    {
        int ms = (u_ticks() - t0) * 10;
        if (ms < 0) ms = 0;
        nv_registra(url, rc, 1, ms);
    }
    return rc;
}

static void nv_hist_ir(int d) {
    int j = NV_ihist + d;
    if (j < 0 || j >= NV_nhist) return;
    NV_hscroll[NV_ihist] = NV_desplaza;
    NV_ihist = j;
    nv_pide(NV_hist[j], 0);
    NV_desplaza = NV_hscroll[j];
    if (NV_desplaza < 0) NV_desplaza = 0;
}

/* v2.7-fase2: linea y columna visuales de pos en un valor con \n */
static void nv_linea_col(const char *v, int pos, int *ln, int *cl) {
    int i = 0, l = 0, c = 0;
    while (i < pos && v[i]) {
        if (v[i] == '\n') {
            l++;
            c = 0;
        } else {
            c++;
        }
        i++;
    }
    *ln = l;
    *cl = c;
}

/* Offset del inicio de la linea ln (0 = primera) */
static int nv_linea_ini(const char *v, int ln) {
    int i = 0, l = 0;
    while (v[i] && l < ln) {
        if (v[i] == '\n') l++;
        i++;
    }
    return i;
}

/* Offset del fin de la linea que empieza en ini (el \n o el NUL) */
static int nv_linea_fin(const char *v, int ini) {
    int i = ini;
    while (v[i] && v[i] != '\n') i++;
    return i;
}

/* Origen en pantalla de la caja del campo en la fila f
   (0 ok, -1 no visible). Misma geometria que el pintado de filas
   (alineacion incluida). */
static int nv_campo_xy(int ci, int f, int *xo, int *yo) {
    int rel, y, x, q, cw, margen;
    if (ci < 0 || ci >= NV_ncampos) return -1;
    if (NV_campos[ci].tipo == 2) return -1;
    if (f < NV_campos[ci].fila ||
        f >= NV_campos[ci].fila + NV_campos[ci].alto)
        return -1;
    if (f < 0 || f >= NV_nfilas) return -1;
    margen = nv_bienv() ? 92 : 0;
    rel = f - NV_desplaza;
    {
        int vis = (NV_v.h - NV_BAR_H - margen - NV_EST_H) / NV_FH;
        if (vis < 1) vis = 1;
        if (rel < 0 || rel >= vis) return -1;
    }
    y = NV_BAR_H + margen + rel * NV_FH;
    cw = NV_v.w - 14;
    if (cw < 80) cw = 80;
    x = 4;
    q = NV_filas[f].len;
    if (q > 159) q = 159;
    /* v2.8: la caja arranca donde mide la columna (no col*8) */
    {
        int qv = nv_ajusta(f, cw - 8);
        int wq = nv_ancho_hasta(f, qv);
        if (NV_filas[f].al == 1 || NV_filas[f].al == 2) {
            int disp = (cw - 8) - wq;
            if (disp > 0)
                x += NV_filas[f].al == 1 ? disp / 2 : disp;
        }
    }
    *xo = x + nv_ancho_hasta(f, NV_campos[ci].col);
    *yo = y;
    return 0;
}

/* Pinta la caja del campo (rel = fila dentro del campo).
   Ventana de valor con desplazamiento + cursor si tiene el foco. */
static void nv_campo_caja(int ci, int bx, int by, int rel) {
    int enf = NV_tiene && ci == NV_foco_campo;
    int tp = NV_campos[ci].tipo;
    int bw = NV_campos[ci].ancho * UG_FW;
    int fg = enf ? C_BLANCO : NS_TINTA;
    ug_relleno(&NV_v, bx, by, bw, NV_FH,
               enf ? T_SELECCION : NS_BLANCO);
    if (tp == 4) {
        ug_texto(&NV_v, bx, by,
                 NV_campos[ci].marca ? "[X]" : "[ ]", fg);
    } else if (tp == 7) {
        ug_texto(&NV_v, bx, by,
                 NV_campos[ci].marca ? "(o)" : "( )", fg);
    } else if (tp == 3 || tp == 8 || tp == 9) {
        const char *et = NV_campos[ci].valor[0] ?
            NV_campos[ci].valor :
            (tp == 8 ? "Restablecer" :
             (tp == 9 ? "Boton" : "Enviar"));
        char b[64];
        int j = 0, k = 0;
        b[j++] = '[';
        b[j++] = ' ';
        while (et[k] && j < 60) b[j++] = et[k++];
        b[j++] = ' ';
        b[j++] = ']';
        b[j] = 0;
        ug_texto(&NV_v, bx, by, b, fg);
    } else if (tp == 6) {
        const char *et = "?";
        char b[36];
        int j = 0, k, n = 0;
        if (NV_campos[ci].pos >= 0 &&
            NV_campos[ci].pos < NV_campos[ci].nopc)
            et = NV_campos[ci].ops[NV_campos[ci].pos];
        while (et[n] && n < 24) n++;
        if (n > NV_campos[ci].ancho - 5)
            n = NV_campos[ci].ancho - 5;
        b[j++] = '[';
        b[j++] = ' ';
        for (k = 0; k < n; k++) b[j++] = et[k];
        while (j < NV_campos[ci].ancho - 2) b[j++] = ' ';
        b[j++] = 'v';
        b[j++] = ']';
        b[j] = 0;
        ug_texto(&NV_v, bx, by, b, fg);
    } else if (tp == 5) {
        /* area: rebanada visible de lineas del valor */
        int vw = NV_campos[ci].ancho - 2;
        int ln0 = 0, cur = 0, cl = 0, k, ini, fin, nc;
        char b[44];
        int j = 0;
        nv_linea_col(NV_campos[ci].valor, NV_campos[ci].pos,
                     &cur, &cl);
        ln0 = cur - NV_campos[ci].alto + 1;
        if (ln0 < 0) ln0 = 0;
        ini = nv_linea_ini(NV_campos[ci].valor, ln0 + rel);
        fin = nv_linea_fin(NV_campos[ci].valor, ini);
        nc = fin - ini;
        if (nc > vw) nc = vw;
        b[j++] = '[';
        for (k = 0; k < vw; k++)
            b[j++] = k < nc ? NV_campos[ci].valor[ini + k] : ' ';
        b[j++] = ']';
        b[j] = 0;
        ug_texto(&NV_v, bx, by, b, fg);
        if (enf && cur == ln0 + rel) {
            int cx = bx + (1 + cl) * UG_FW;
            if (cx > bx + bw - 8) cx = bx + bw - 8;
            ug_relleno(&NV_v, cx, by + 2, 7, 12, C_BLANCO);
        }
    } else {
        int vw = NV_campos[ci].ancho - 2;
        char b[NV_CAMPO_ANCHO + 3];
        int j = 0, k, ini = 0, n = 0;
        while (NV_campos[ci].valor[n]) n++;
        if (n > vw) {
            ini = NV_campos[ci].pos - vw + 1;
            if (ini < 0) ini = 0;
            if (ini > n - vw) ini = n - vw;
        }
        b[j++] = '[';
        for (k = 0; k < vw; k++) {
            char c = NV_campos[ci].valor[ini + k];
            if (!c) break;
            b[j++] = (tp == 1) ? (char)'*' : c;
        }
        while (k < vw) {
            b[j++] = ' ';
            k++;
        }
        b[j++] = ']';
        b[j] = 0;
        ug_texto(&NV_v, bx, by, b, fg);
        if (enf) {
            int cx = bx + (1 + NV_campos[ci].pos - ini) * UG_FW;
            ug_relleno(&NV_v, cx, by + 2, 7, 12, C_BLANCO);
        }
    }
}

static void nv_dibuja(void) {
    int w = NV_v.w, h = NV_v.h, i;
    int cw, ch;
    ug_comienza(&NV_v);
    ug_limpia(&NV_v, NS_BLANCO);
    /* Barra superior clara con borde inferior */
    ug_relleno(&NV_v, 0, 0, w, NV_BAR_H, NS_BLANCO);
    ug_linea(&NV_v, 0, NV_BAR_H - 1, w - 1, NV_BAR_H - 1,
             NS_BORDE);
    ug_boton(&NV_v, 8, 8, 36, 28, "<",
             (NV_nhist > 0 && NV_ihist > 0) ? UG_BOTON_NORMAL :
             UG_BOTON_OFF);
    ug_boton(&NV_v, 48, 8, 36, 28, ">",
             (NV_ihist < NV_nhist - 1) ? UG_BOTON_NORMAL :
             UG_BOTON_OFF);
    ug_boton(&NV_v, 88, 8, 36, 28, "R", UG_BOTON_NORMAL);
    ug_boton(&NV_v, 128, 8, 36, 28, "X", UG_BOTON_NORMAL);
    ug_boton(&NV_v, 168, 8, 36, 28, "H", UG_BOTON_NORMAL);
    ug_campo(&NV_v, 212, 12, w - 212 - 68, NV_bar, NV_bpos,
             NV_foco_barra);
    /* Ir con acento de marca */
    ug_relleno(&NV_v, w - 60, 8, 44, 28, T_ACENTO);
    ug_texto(&NV_v, w - 60 + 14, 8 + 6, "Ir", C_BLANCO);
    cw = w - 14;
    if (cw < 80) cw = 80;
    ch = h - NV_BAR_H - NV_EST_H;
    if (ch < NV_FH) ch = NV_FH;
    {
        /* En la bienvenida el panel de busqueda ocupa la parte
           superior; el contenido baja. */
        int margen = nv_bienv() ? 92 : 0;
        ch -= margen;
        if (ch < NV_FH) ch = NV_FH;
        NV_vis = ch / NV_FH;
        if (NV_vis < 1) NV_vis = 1;
        if (margen) {
            int fx, fy, fw, bx, qx;
            char *hint = "Escribe una palabra y pulsa Intro";
            nv_qgeom(w, &fx, &fy, &fw, &bx);
            ug_relleno(&NV_v, 0, NV_BAR_H, w, margen, NS_PANEL);
            ug_linea(&NV_v, 0, NV_BAR_H + margen - 1, w - 1,
                     NV_BAR_H + margen - 1, NS_BORDE);
            ug_texto(&NV_v, fx, fy - 16, "Buscador",
                     NS_TITULO);
            ug_relleno(&NV_v, fx, fy, fw, 20, NS_BLANCO);
            ug_rect(&NV_v, fx, fy, fw, 20,
                    NV_qfoco ? NS_TITULO : NS_BORDE);
            if (NV_qbuf[0]) {
                int nq = 0;
                static char vq[120];
                while (NV_qbuf[nq] && nq < 119) {
                    vq[nq] = NV_qbuf[nq];
                    nq++;
                }
                vq[nq] = 0;
                ug_texto(&NV_v, fx + 4, fy + 2, vq, NS_TINTA);
                if (NV_qfoco)
                    ug_relleno(&NV_v, fx + 4 + nq * UG_FW,
                               fy + 3, 7, 14, NS_TINTA);
            } else {
                int nq = 0;
                while (hint[nq]) nq++;
                if (nq * UG_FW < fw - 8)
                    ug_texto(&NV_v, fx + 4, fy + 2, hint,
                             NS_GRIS);
                if (NV_qfoco)
                    ug_relleno(&NV_v, fx + 4, fy + 3, 7, 14,
                               NS_TINTA);
            }
            ug_relleno(&NV_v, bx, fy, 68, 20, T_ACENTO);
            ug_texto(&NV_v, bx + 10, fy + 2, "Buscar", C_BLANCO);
        }
    }
    if (!NV_tiene) {
        ug_texto(&NV_v, 24, NV_BAR_H + 40, "netsurf",
                 NS_TITULO);
        ug_texto(&NV_v, 24, NV_BAR_H + 40, "netsurf",
                 NS_TITULO);
        ug_texto(&NV_v, 24, NV_BAR_H + 64,
                 NV_estado[0] ? NV_estado :
                 "Escribe una direccion y pulsa Intro.",
                 NS_TINTA);
        ug_texto(&NV_v, 24, NV_BAR_H + 88,
                 "Ej: http://info.cern.ch/ (sin TLS) o una busqueda.",
                 NS_GRIS);
        ug_texto(&NV_v, 24, NV_BAR_H + 112,
                 "Intro=ir  R=recargar  H=inicio  Retroceso=barra",
                 NS_GRIS);
        ug_texto(&NV_v, 24, NV_BAR_H + 136,
                 "Tab=enlace/campo  Sin JS.",
                 NS_GRIS);
    } else {
        int maxd = NV_nfilas - NV_vis;
        char tmp[160];
        if (maxd < 0) maxd = 0;
        if (NV_desplaza > maxd) NV_desplaza = maxd;
        if (NV_desplaza < 0) NV_desplaza = 0;
        for (i = 0; i < NV_vis; i++) {
            int f = NV_desplaza + i;
            int y = NV_BAR_H + (nv_bienv() ? 92 : 0) + i * NV_FH,
                q, k;
            int x = 4, color = NS_TINTA;
            if (f >= NV_nfilas) break;
            if (NV_filas[f].estilo == 1) continue;
            if (NV_filas[f].estilo == 2) {
                ug_linea(&NV_v, 4, y + NV_FH / 2, cw - 4,
                         y + NV_FH / 2, C_GRIS);
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
                    for (yy = 0; yy < NV_FH &&
                         band * NV_FH + yy < dh; yy++) {
                        int sy = (band * NV_FH + yy) *
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
            /* v2.8: la fila se mide en px y se pinta proporcional.
               tmp lleva los bytes visibles (frontera UTF-8). */
            q = nv_ajusta(f, cw - 8);
            for (k = 0; k < q; k++)
                tmp[k] = (char)NV_txt[NV_filas[f].ini + k];
            tmp[q] = 0;
            if (NV_filas[f].fg >= 0)
                color = 0x01000000 |
                    (NV_filas[f].fg & 0xFFFFFF);
            else if (NV_filas[f].sub)
                color = NS_ENLACE;
            /* Hito 6: alineacion del bloque */
            if (NV_filas[f].al == 1 || NV_filas[f].al == 2) {
                int wq = ttf_ancho(tmp, q, NV_PX);
                int disp = (cw - 8) - wq;
                if (disp > 0)
                    x += NV_filas[f].al == 1 ? disp / 2 :
                                               disp;
            }
            if (NV_filas[f].bg >= 0) {
                if (NV_filas[f].full) {
                    /* Hito 6: el bloque cubre todo el ancho */
                    ug_relleno(&NV_v, 4, y, cw - 8, NV_FH,
                               0x01000000 |
                               (NV_filas[f].bg & 0xFFFFFF));
                } else {
                    int wpx = ttf_ancho(tmp, q, NV_PX);
                    ug_relleno(&NV_v, x, y, wpx, NV_FH,
                               0x01000000 |
                               (NV_filas[f].bg & 0xFFFFFF));
                }
            }
            /* Hito 4: enlace con foco -> banda de seleccion */
            if (NV_tiene && NV_foco_enl >= 0 &&
                NV_foco_enl < NV_nenl &&
                f >= NV_enl[NV_foco_enl].f0 &&
                f <= NV_enl[NV_foco_enl].f1) {
                int wpx = ttf_ancho(tmp, q, NV_PX);
                if (wpx < 8) wpx = 8;
                ug_relleno(&NV_v, x, y, wpx, NV_FH,
                           T_SELECCION);
                color = 0xFFFFFF;
            }
            if (!(color & 0x01000000))
                color = 0xFFFFFF;
            /* v2.8: texto proporcional con base en yb.
               ug_* suma NV_v.ox/oy en recorta; ttf_texto va por
               sc_64 directo (absoluto), asi que se suma aqui. */
            if (q > 0) {
                int yb = y + NV_asc + NV_v.oy;
                int xa = x + NV_v.ox;
                ttf_texto(xa, yb, tmp, q, NV_PX,
                          (unsigned)color);
                /* Hito 6: negrita por doble pasada */
                if (NV_filas[f].bd)
                    ttf_texto(xa + 1, yb, tmp, q, NV_PX,
                              (unsigned)color);
            }
            /* Hito 6: subrayado por tramo de enlace */
            if (NV_filas[f].sub && q > 0) {
                int li;
                for (li = 0; li < NV_nenl; li++) {
                    if (f >= NV_enl[li].f0 &&
                        f <= NV_enl[li].f1) {
                        int xs = (f == NV_enl[li].f0) ?
                            NV_enl[li].c0 : 0;
                        int xe = (f == NV_enl[li].f1 &&
                                 NV_enl[li].c1 >= 0) ?
                            NV_enl[li].c1 : q;
                        int xspx, xepx;
                        if (xs < 0) xs = 0;
                        if (xe > q) xe = q;
                        if (xe > xs) {
                            xspx = nv_ancho_hasta(f, xs);
                            xepx = nv_ancho_hasta(f, xe);
                            ug_linea(&NV_v, x + xspx,
                                     y + NV_asc + 2,
                                     x + xepx - 1,
                                     y + NV_asc + 2, color);
                        }
                    }
                }
            }
            /* v2.7: cajas de formulario de esta fila */
            if (NV_tiene && NV_ncampos > 0) {
                int cc;
                for (cc = 0; cc < NV_ncampos; cc++) {
                    if (NV_campos[cc].tipo != 2 &&
                        f >= NV_campos[cc].fila &&
                        f < NV_campos[cc].fila +
                            NV_campos[cc].alto) {
                        int bx, by;
                        if (!nv_campo_xy(cc, f, &bx, &by))
                            nv_campo_caja(cc, bx, by,
                                          f - NV_campos[cc].fila);
                    }
                }
            }
        }
        /* scrollbar */
        if (NV_nfilas > NV_vis) {
            int th = NV_vis * NV_FH;
            int tam = th * NV_vis / NV_nfilas;
            int pos = th * NV_desplaza / NV_nfilas;
            if (tam < 8) tam = 8;
            ug_relleno(&NV_v, w - 8, NV_BAR_H + pos, 6, tam,
                       C_GRIS);
        }
    }
    ug_relleno(&NV_v, 0, h - NV_EST_H, w, NV_EST_H, NS_EST);
    ug_texto(&NV_v, 8, h - NV_EST_H + 4,
             NV_estado[0] ? NV_estado : "netsurf",
             NS_TINTA);
    /* Hito 4: resultado de red a la derecha (codigo, bytes, ms) */
    if (NV_red[0]) {
        int lr = 0, xr;
        while (NV_red[lr]) lr++;
        xr = w - 8 - lr * UG_FW;
        if (xr < w / 2) xr = w / 2;
        ug_texto(&NV_v, xr, h - NV_EST_H + 4, NV_red, NS_GRIS);
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

/* v2.7: lleva el campo con foco a la vista */
static void nv_asegura_campo(int idx) {
    int f;
    if (idx < 0 || idx >= NV_ncampos) return;
    f = NV_campos[idx].fila;
    if (f < 0) return;
    if (f < NV_desplaza)
        NV_desplaza = f;
    else if (f >= NV_desplaza + NV_vis)
        NV_desplaza = f - NV_vis + 1;
    if (NV_desplaza < 0) NV_desplaza = 0;
}

/* v2.7: teclas de edicion dentro de un campo de texto (0 = texto,
   1 = clave) o area (5 = multilinea con Intro). Devuelve 1 si la
   consumio. */
static int nv_campo_tecla(int ci, int tecla) {
    nv_campo_t *c;
    int n;
    if (ci < 0 || ci >= NV_ncampos) return 0;
    c = &NV_campos[ci];
    if (c->tipo != 0 && c->tipo != 1 && c->tipo != 5) return 0;
    n = 0;
    while (c->valor[n]) n++;
    if (tecla == 8) {
        if (c->pos > 0) {
            int i;
            c->pos--;
            for (i = c->pos; i < n; i++) c->valor[i] = c->valor[i + 1];
        }
        return 1;
    }
    if (tecla == 0x103) {
        if (c->pos > 0) c->pos--;
        return 1;
    }
    if (tecla == 0x104) {
        if (c->pos < n) c->pos++;
        return 1;
    }
    if (tecla == 0x107) {
        if (c->tipo == 5) {
            int ln, cl, ini;
            nv_linea_col(c->valor, c->pos, &ln, &cl);
            (void)cl;
            ini = nv_linea_ini(c->valor, ln);
            c->pos = ini;
        } else {
            c->pos = 0;
        }
        return 1;
    }
    if (tecla == 0x108) {
        if (c->tipo == 5) {
            int ln, cl, ini, fin;
            nv_linea_col(c->valor, c->pos, &ln, &cl);
            ini = nv_linea_ini(c->valor, ln);
            fin = nv_linea_fin(c->valor, ini);
            (void)fin;
            c->pos = ini + cl;
            if (c->pos > fin) c->pos = fin;
        } else {
            c->pos = n;
        }
        return 1;
    }
    if (tecla == 0x106) {
        if (c->pos < n) {
            int i;
            for (i = c->pos; i < n; i++) c->valor[i] = c->valor[i + 1];
        }
        return 1;
    }
    if (tecla == 0x101 || tecla == 0x102) {
        /* arriba/abajo en el area: mueve de linea */
        if (c->tipo == 5) {
            int ln, cl, ini, fin, nl;
            nv_linea_col(c->valor, c->pos, &ln, &cl);
            if (tecla == 0x101) {
                if (ln == 0) return 1;
                nl = ln - 1;
            } else {
                nl = ln + 1;
            }
            ini = nv_linea_ini(c->valor, nl);
            if (c->valor[ini] == 0 && nl > ln) return 1;
            fin = nv_linea_fin(c->valor, ini);
            c->pos = ini + cl;
            if (c->pos > fin) c->pos = fin;
            return 1;
        }
        return 0;
    }
    if (tecla == 13 && c->tipo == 5) {
        if (n < NV_CAMPO_TAM) {
            int i;
            for (i = n; i > c->pos; i--) c->valor[i] = c->valor[i - 1];
            c->valor[c->pos++] = '\n';
            c->valor[n + 1] = 0;
        }
        return 1;
    }
    if (tecla >= 32 && tecla < 127 && n < NV_CAMPO_TAM) {
        int i;
        for (i = n; i > c->pos; i--) c->valor[i] = c->valor[i - 1];
        c->valor[c->pos++] = (char)tecla;
        c->valor[n + 1] = 0;
        return 1;
    }
    return 0;
}

/* v2.7-fase2: marca el radio y desmarca a sus hermanos
   (mismo formulario y nombre). */
static void nv_campo_radio(int ci) {
    int i;
    if (ci < 0 || ci >= NV_ncampos) return;
    if (NV_campos[ci].tipo != 7) return;
    NV_campos[ci].marca = 1;
    for (i = 0; i < NV_ncampos; i++) {
        if (i != ci && NV_campos[i].tipo == 7 &&
            NV_campos[i].form == NV_campos[ci].form &&
            !u_strcmp(NV_campos[i].nombre, NV_campos[ci].nombre))
            NV_campos[i].marca = 0;
    }
}

/* v2.7-fase2: restaura los valores iniciales del formulario. */
static void nv_campo_repone(int fi) {
    int i;
    for (i = 0; i < NV_ncampos; i++) {
        if (NV_campos[i].form != fi) continue;
        {
            int q = 0;
            while (NV_campos[i].ini[q]) {
                NV_campos[i].valor[q] = NV_campos[i].ini[q];
                q++;
            }
            NV_campos[i].valor[q] = 0;
        }
        NV_campos[i].pos = NV_campos[i].pini;
        NV_campos[i].marca = NV_campos[i].mini;
    }
}

/* v2.7: activa el campo con foco (Intro): enviar lo manda,
   reponer restaura, la caja manda (conmuta con espacio o clic),
   el texto/lista/radio mandan su formulario, el area mete salto
   (se come el Intro en edicion) y el inerte nada. */
static void nv_campo_activa(int ci) {
    nv_campo_t *c;
    if (ci < 0 || ci >= NV_ncampos) return;
    c = &NV_campos[ci];
    if (c->tipo == 4) {
        nv_envia_form(c->form, -1);
        return;
    }
    if (c->tipo == 7) {
        nv_campo_radio(ci);
        return;
    }
    if (c->tipo == 8) {
        nv_campo_repone(c->form);
        return;
    }
    if (c->tipo == 9 || c->tipo == 2) return;
    if (c->tipo == 3 || c->tipo == 0 || c->tipo == 1 ||
        c->tipo == 5 || c->tipo == 6)
        nv_envia_form(c->form, ci);
}

static void nv_ev_tecla(int tecla) {
    /* Red de seguridad: en la bienvenida, una tecla imprimible sin
       foco claro va a la casilla (evita perder pulsaciones). */
    if (!NV_foco_barra && !NV_qfoco && NV_tiene && nv_bienv() &&
        tecla >= 32 && tecla < 127)
        NV_qfoco = 1;
    /* Casilla de busqueda de la bienvenida (tiene prioridad si la
       barra no la tiene). */
    if (NV_qfoco && !NV_foco_barra) {
        if (tecla == 13) {
            if (NV_qbuf[0]) {
                /* v2.5: la casilla acepta URL directa o busqueda
                   (nv_ir decide: esquema/dominio -> va; texto -> motor). */
                NV_qfoco = 0;
                nv_ir(NV_qbuf);
            }
        } else if (tecla == 27) {
            NV_qfoco = 0;
            NV_foco_barra = 1;
        } else if (tecla == 8) {
            if (NV_qpos > 0) {
                NV_qpos--;
                NV_qbuf[NV_qpos] = 0;
            }
        } else if (tecla >= 32 && tecla < 127) {
            if (NV_qpos < (int)sizeof(NV_qbuf) - 1) {
                NV_qbuf[NV_qpos++] = (char)tecla;
                NV_qbuf[NV_qpos] = 0;
            }
        }
        ug_tick(&NV_v, 1);
        return;
    }
    if (NV_foco_barra) {
        if (tecla == 13) {
            nv_ir(NV_bar);
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
            /* Tab en barra: primer enlace, si no el primer campo
               visible (los ocultos/inertes no paran). */
            int cc;
            NV_foco_barra = 0;
            NV_foco_campo = -1;
            NV_foco_enl = (NV_tiene && NV_nenl > 0) ? 0 : -1;
            if (NV_foco_enl >= 0) {
                nv_asegura_enlace(NV_foco_enl);
            } else if (NV_tiene) {
                for (cc = 0; cc < NV_ncampos; cc++) {
                    if (NV_campos[cc].tipo != 2 &&
                        NV_campos[cc].tipo != 9)
                        break;
                }
                if (cc < NV_ncampos) {
                    NV_foco_campo = cc;
                    nv_asegura_campo(cc);
                }
            }
        } else {
            ug_campo_tecla(NV_bar, (int)sizeof(NV_bar),
                           &NV_bpos, tecla);
        }
        ug_tick(&NV_v, 1);
        return;
    }
    /* v2.7: el campo de texto/area/clave se come la edicion
       (incluido el espacio, el retroceso y el Intro del area). */
    if (NV_tiene && NV_foco_campo >= 0 &&
        NV_foco_campo < NV_ncampos &&
        (NV_campos[NV_foco_campo].tipo == 0 ||
         NV_campos[NV_foco_campo].tipo == 1 ||
         NV_campos[NV_foco_campo].tipo == 5) &&
        (tecla == 8 || tecla == 0x103 || tecla == 0x104 ||
         tecla == 0x106 || tecla == 0x107 || tecla == 0x108 ||
         tecla == 0x101 || tecla == 0x102 || tecla == 13 ||
         (tecla >= 32 && tecla < 127))) {
        nv_campo_tecla(NV_foco_campo, tecla);
    } else if (NV_tiene && NV_foco_campo >= 0 &&
               NV_foco_campo < NV_ncampos &&
               NV_campos[NV_foco_campo].tipo == 6 &&
               (tecla == 0x101 || tecla == 0x102 ||
                tecla == 0x103 || tecla == 0x104)) {
        /* v2.7-fase2: arriba/abajo/izq/der ciclan las opciones */
        int n = NV_campos[NV_foco_campo].nopc;
        if (n > 0) {
            if (tecla == 0x101 || tecla == 0x103)
                NV_campos[NV_foco_campo].pos =
                    (NV_campos[NV_foco_campo].pos + n - 1) % n;
            else
                NV_campos[NV_foco_campo].pos =
                    (NV_campos[NV_foco_campo].pos + 1) % n;
        }
    } else if (NV_tiene && NV_foco_campo >= 0 &&
               NV_foco_campo < NV_ncampos &&
               NV_campos[NV_foco_campo].tipo == 7 &&
               (tecla == 0x101 || tecla == 0x102 ||
                tecla == 0x103 || tecla == 0x104)) {
        /* v2.7-fase2: flechas mueven el marcado dentro del grupo */
        int i, cur = NV_foco_campo, nxt = -1;
        if (tecla == 0x101 || tecla == 0x103) {
            for (i = cur - 1; i >= 0; i--) {
                if (NV_campos[i].tipo == 7 &&
                    NV_campos[i].form == NV_campos[cur].form &&
                    !u_strcmp(NV_campos[i].nombre,
                              NV_campos[cur].nombre)) {
                    nxt = i;
                    break;
                }
            }
            if (nxt < 0) {
                for (i = NV_ncampos - 1; i > cur; i--) {
                    if (NV_campos[i].tipo == 7 &&
                        NV_campos[i].form ==
                            NV_campos[cur].form &&
                        !u_strcmp(NV_campos[i].nombre,
                                NV_campos[cur].nombre)) {
                        nxt = i;
                        break;
                    }
                }
            }
        } else {
            int n;
            for (n = cur + 1, i = cur + 1; n < NV_ncampos; n++, i++) {
                if (i >= NV_ncampos) i -= NV_ncampos;
                if (i == cur) break;
                if (NV_campos[i].tipo == 7 &&
                    NV_campos[i].form == NV_campos[cur].form &&
                    !u_strcmp(NV_campos[i].nombre,
                              NV_campos[cur].nombre)) {
                    nxt = i;
                    break;
                }
            }
        }
        if (nxt >= 0) {
            nv_campo_radio(nxt);
            NV_foco_campo = nxt;
            nv_asegura_campo(nxt);
        }
    } else if (tecla == 0x101) {
        NV_foco_campo = -1;
        NV_desplaza--;
        if (NV_desplaza < 0) NV_desplaza = 0;
    } else if (tecla == 0x102) {
        NV_foco_campo = -1;
        NV_desplaza++;
    } else if (tecla == 0x109) {
        NV_foco_campo = -1;
        NV_desplaza -= NV_vis;
        if (NV_desplaza < 0) NV_desplaza = 0;
    } else if (tecla == 0x10A) {
        NV_foco_campo = -1;
        NV_desplaza += NV_vis;
    } else if (tecla == 0x107) {
        NV_foco_campo = -1;
        NV_desplaza = 0;
    } else if (tecla == 0x108) {
        NV_foco_campo = -1;
        NV_desplaza = NV_nfilas - NV_vis;
        if (NV_desplaza < 0) NV_desplaza = 0;
    } else if (tecla == 8) {
        NV_foco_campo = -1;
        nv_hist_ir(-1);
    } else if (tecla == 'r' || tecla == 'R') {
        if (NV_url[0])
            nv_pide(NV_url, 0);
    } else if (tecla == 9) {
        /* Tab: enlaces, luego campos visibles, al final la barra */
        if (NV_tiene && NV_foco_campo < 0 && NV_nenl > 0 &&
            NV_foco_enl + 1 < NV_nenl) {
            NV_foco_enl++;
            nv_asegura_enlace(NV_foco_enl);
        } else if (NV_tiene) {
            int cc, ini = NV_foco_campo;
            if (ini < -1) ini = -1;
            for (cc = ini + 1; cc < NV_ncampos; cc++) {
                if (NV_campos[cc].tipo != 2 &&
                    NV_campos[cc].tipo != 9)
                    break;
            }
            if (cc < NV_ncampos) {
                NV_foco_enl = -1;
                NV_foco_campo = cc;
                nv_asegura_campo(NV_foco_campo);
            } else {
                NV_foco_enl = -1;
                NV_foco_campo = -1;
                NV_foco_barra = 1;
                NV_qfoco = 0;
            }
        } else {
            NV_foco_enl = -1;
            NV_foco_campo = -1;
            NV_foco_barra = 1;
            NV_qfoco = 0;
        }
    } else if (tecla == 13) {
        /* Intro: campo con foco, si no enlace con foco */
        if (NV_tiene && NV_foco_campo >= 0 &&
            NV_foco_campo < NV_ncampos)
            nv_campo_activa(NV_foco_campo);
        else if (NV_tiene && NV_foco_enl >= 0 &&
                 NV_foco_enl < NV_nenl)
            nv_pide(NV_enl[NV_foco_enl].url, 1);
        else if (!NV_tiene)
            NV_foco_barra = 1;
    } else if (tecla == 32) {
        /* Espacio: conmuta la caja o marca el radio con foco */
        if (NV_tiene && NV_foco_campo >= 0 &&
            NV_foco_campo < NV_ncampos) {
            int tp = NV_campos[NV_foco_campo].tipo;
            if (tp == 4)
                NV_campos[NV_foco_campo].marca =
                    !NV_campos[NV_foco_campo].marca;
            else if (tp == 7)
                nv_campo_radio(NV_foco_campo);
        }
    } else if (tecla == 27) {
        NV_foco_enl = -1;
        NV_foco_campo = -1;
        NV_foco_barra = 1;
        NV_qfoco = 0;
    }
    ug_tick(&NV_v, 1);
}

static void nv_ev_raton(int lx, int ly, int bot, int rueda) {
    int clic = ug_clic(NV_b_antes, bot);
    NV_b_antes = bot;
    if (rueda) {
        NV_desplaza -= rueda * 2;
        if (NV_desplaza < 0) NV_desplaza = 0;
        ug_tick(&NV_v, 1);
        return;
    }
    if (!clic) return;
    if (ly >= 8 && ly < 36) {
        if (lx >= 8 && lx < 44) {
            nv_hist_ir(-1);
            ug_tick(&NV_v, 1);
            return;
        }
        if (lx >= 48 && lx < 84) {
            nv_hist_ir(1);
            ug_tick(&NV_v, 1);
            return;
        }
        if (lx >= 88 && lx < 124) {
            if (NV_url[0])
                nv_pide(NV_url, 0);
            ug_tick(&NV_v, 1);
            return;
        }
        if (lx >= 128 && lx < 164) {
            NV_tiene = 0;
            NV_foco_enl = -1;
            NV_url[0] = 0;
            NV_bar[0] = 0;
            NV_bpos = 0;
            NV_foco_barra = 1;
            NV_qfoco = 0;
            nv_pon_estado("");
            NV_red[0] = 0;
            ug_tick(&NV_v, 1);
            return;
        }
        if (lx >= 168 && lx < 204) {
            nv_pide(NV_BIENV_URL, 1);
            ug_tick(&NV_v, 1);
            return;
        }
        if (lx >= NV_v.w - 60 && lx < NV_v.w - 16) {
            nv_ir(NV_bar);
            NV_foco_barra = 0;
            ug_tick(&NV_v, 1);
            return;
        }
        if (lx >= 212 && lx < NV_v.w - 68) {
            NV_foco_barra = 1;
            NV_qfoco = 0;
            ug_tick(&NV_v, 1);
            return;
        }
        NV_foco_barra = 0;
        NV_qfoco = 0;
        ug_tick(&NV_v, 1);
        return;
    }
    if (ly >= NV_BAR_H && ly < NV_v.h - NV_EST_H) {
        int margen = nv_bienv() ? 92 : 0;
        if (margen && ly < NV_BAR_H + margen) {
            /* Panel de busqueda de la bienvenida */
            int fx, fy, fw, bx;
            nv_qgeom(NV_v.w, &fx, &fy, &fw, &bx);
            if (lx >= fx && lx < fx + fw && ly >= fy &&
                ly < fy + 20) {
                NV_qfoco = 1;
                NV_foco_barra = 0;
                ug_tick(&NV_v, 1);
                return;
            }
            if (lx >= bx && lx < bx + 68 && ly >= fy &&
                ly < fy + 20) {
                if (NV_qbuf[0]) {
                    static char tmpq[NV_MAXURL + 1];
                    NV_qfoco = 0;
                    nv_busca_url(NV_qbuf, tmpq);
                    nv_pide(tmpq, 1);
                }
                ug_tick(&NV_v, 1);
                return;
            }
            /* Resto del panel: enfoca la casilla. */
            NV_qfoco = 1;
            NV_foco_barra = 0;
            ug_tick(&NV_v, 1);
            return;
        }
        if (lx >= NV_v.w - 12 && NV_nfilas > NV_vis) {
            int th = NV_vis * NV_FH;
            int rel = ly - NV_BAR_H;
            int tam = th * NV_vis / NV_nfilas;
            int pos = th * NV_desplaza / NV_nfilas;
            if (rel < pos)
                NV_desplaza -= NV_vis;
            else if (rel > pos + tam)
                NV_desplaza += NV_vis;
            if (NV_desplaza < 0) NV_desplaza = 0;
        } else if (lx < NV_v.w - 12) {
            /* v2.7: clic en caja antes que en enlace */
            int f = NV_desplaza +
                (ly - NV_BAR_H - (nv_bienv() ? 92 : 0)) /
                NV_FH, idx;
            if ((ly - NV_BAR_H - (nv_bienv() ? 92 : 0)) < 0)
                f = -1;
            if (f >= 0 && f < NV_nfilas) {
                int cc = nv_campo_en_px(f, lx);
                if (cc >= 0) {
                    int tp = NV_campos[cc].tipo;
                    NV_foco_enl = -1;
                    NV_foco_campo = cc;
                    NV_foco_barra = 0;
                    NV_qfoco = 0;
                    if (tp == 0 || tp == 1 || tp == 5) {
                        int bx, by, rel, n;
                        rel = f - NV_campos[cc].fila;
                        n = 0;
                        while (NV_campos[cc].valor[n]) n++;
                        if (!nv_campo_xy(cc, f, &bx, &by)) {
                            if (tp == 5) {
                                /* pos en la linea clicada */
                                int ini, fin, col;
                                int rr, ln0, cl;
                                nv_linea_col(NV_campos[cc].valor,
                                             NV_campos[cc].pos,
                                             &rr, &cl);
                                ln0 = rr - NV_campos[cc].alto + 1;
                                if (ln0 < 0) ln0 = 0;
                                ini = nv_linea_ini(
                                    NV_campos[cc].valor,
                                    ln0 + rel);
                                fin = nv_linea_fin(
                                    NV_campos[cc].valor, ini);
                                col = (lx - bx) / UG_FW - 1;
                                if (col < 0) col = 0;
                                if (col > fin - ini)
                                    col = fin - ini;
                                NV_campos[cc].pos = ini + col;
                            } else {
                                int off;
                                off = (lx - bx) / UG_FW - 1;
                                if (off < 0) off = 0;
                                if (off > n) off = n;
                                NV_campos[cc].pos = off;
                            }
                        }
                    } else if (tp == 4) {
                        NV_campos[cc].marca =
                            !NV_campos[cc].marca;
                    } else if (tp == 7) {
                        nv_campo_radio(cc);
                    } else if (tp == 6) {
                        /* lista: el clic solo enfoca (las flechas
                           cambian la opcion, Intro la manda) */
                    } else if (tp == 9) {
                        /* inerte: solo repinta */
                    } else {
                        nv_campo_activa(cc);
                    }
                    ug_tick(&NV_v, 1);
                    return;
                }
                idx = nv_enlace_en(f);
                if (idx >= 0) {
                    NV_foco_campo = -1;
                    nv_pide(NV_enl[idx].url, 1);
                }
            }
        }
    }
    ug_tick(&NV_v, 1);
}

int main(void) {
    const char *a = u_args();
    int srv = 0;
    ug_ev_t ev;
    nv_fuente_ini();
    while (*a == ' ') a++;
    if (a[0] == '-' && a[1] == 'p' &&
        (a[2] == 0 || a[2] == ' ')) {
        return nv_modo_p(a + 2);
    }
    srv = u_atoi(a);
    while (*a >= '0' && *a <= '9') a++;
    while (*a == ' ') a++;
    if (srv <= 0) {
        u_print("Uso: netsurf <srv> [url] | netsurf -p [url]\n");
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
    NV_qpos = 0;
    NV_qbuf[0] = 0;
    NV_qfoco = 0;
    NV_b_antes = 0;
    NV_gui = 1;   /* a partir de aqui hay bucle de eventos */
    if (ug_abre(&NV_v, srv, NV_W, NV_H, "netsurf"))
        return 98;
    ug_edit(&NV_v, 1);
    if (!*a) {
        /* Sin URL: bienvenida interna (sin red), como NetSurf. */
        int i = 0;
        while (NV_BIENV_URL[i] && i < NV_MAXURL) {
            NV_bar[i] = NV_BIENV_URL[i];
            i++;
        }
        NV_bar[i] = 0;
        NV_bpos = i;
        /* Foco directo en la casilla de busqueda (sin raton). */
        NV_foco_barra = 0;
        NV_qfoco = 1;
        NV_ini = 1;
        nv_pon_estado("bienvenido");
    } else if (*a) {
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
            /* Hito 6: solo se pinta en DRAW del servidor; el
               dibujo explicito no se compone. Navegacion en dos
               fases: fase 0 pinta "cargando..." y su ACK lo lleva
               a pantalla; fase 1 (siguiente DRAW) descarga ya con
               el aviso visible. */
            nv_dibuja();
            if (NV_ini) {
                /* Arranque con URL: por la cola pendiente (dos
                   fases) para no bloquear el primer pintado. */
                NV_ini = 0;
                nv_pide(NV_bar, 1);
            }
            if (NV_hay_pend && NV_pend_fase == 0) {
                NV_pend_fase = 1;
                ug_tick(&NV_v, 1);
            } else {
                ug_tick(&NV_v, 0);
                if (NV_hay_pend) {
                    /* Segunda fase: la descarga bloquea; ya se
                       presento "cargando...". Al terminar se
                       repinta de inmediato. */
                    static char tmp[NV_MAXURL + 1];
                    int i = 0, e = NV_pend_empuja, es_post =
                        NV_pend_post;
                    NV_hay_pend = 0;
                    NV_pend_post = 0;
                    while (NV_pend[i] && i < NV_MAXURL) {
                        tmp[i] = NV_pend[i];
                        i++;
                    }
                    tmp[i] = 0;
                    if (es_post)
                        nv_envia_post(tmp, NV_post_cuerpo);
                    else
                        nv_navegar(tmp, e);
                    nv_dibuja();
                    ug_tick(&NV_v, 1);
                }
            }
        }
    }
    nv_css_fin();
    ug_cierra(&NV_v);
    return 0;
}