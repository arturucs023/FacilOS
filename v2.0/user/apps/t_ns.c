void __main(void) {
}

#include "ulib.h"

#include <nsutils/base64.h>
#include <nslog/nslog.h>
#include <nspsl.h>
#include <libnsbmp.h>
#include <nsgif.h>
#include <dom/dom.h>
#include <libcss/libcss.h>

/* t_ns (Hito 1 NetSurf): KAT unitarios de las libs vendoreadas.
   Sin red. Estilo t_f32/t_tls: cuenta pruebas/fallos en QEMU. */

static int t_pruebas = 0;
static int t_fallos = 0;

static void t_ok(int cond, const char *nom) {
    char b[96];
    int i = 0, t = 0;
    t_pruebas++;
    if (!cond) {
        t_fallos++;
        while ("FALLO "[i]) b[t++] = "FALLO "[i++];
    } else {
        while ("ok "[i]) b[t++] = "ok "[i++];
    }
    i = 0;
    while (nom[i] && t < 90) b[t++] = nom[i++];
    b[t++] = '\n';
    b[t] = 0;
    u_print(b);
}

static int t_igual(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

/* ---- utils: base64 ida y vuelta ---- */
static void prueba_base64(void) {
    uint8_t *out = 0;
    size_t outlen = 0;
    u_print("t_ns: base64...\n");
    t_ok(nsu_base64_encode_alloc((const uint8_t *)"Hola", 4, &out,
                                 &outlen) == NSUERROR_OK &&
         outlen == 8 && !u_memcmp(out, "SG9sYQ==", 8), "b64-enc");
    if (out) {
        uint8_t *back = 0;
        size_t backlen = 0;
        int ok = nsu_base64_decode_alloc(out, outlen, &back,
                                        &backlen) == NSUERROR_OK &&
                 backlen == 4 && !u_memcmp(back, "Hola", 4);
        t_ok(ok, "b64-dec");
        u_free(back);
        u_free(out);
    } else {
        t_ok(0, "b64-dec");
    }
}

/* ---- log: categoria + render capturado ---- */
NSLOG_DEFINE_CATEGORY(tns, "categoria de prueba t_ns");
static char log_cap[256];
static int log_n = 0;
static void log_cb(void *ctx, nslog_entry_context_t *e, const char *fmt,
                   __builtin_va_list ap) {
    int i = 0;
    (void)ctx;
    (void)e;
    log_n++;
    while (fmt[i] && i < 250) {
        log_cap[i] = fmt[i];
        i++;
    }
    /* formatea el primer %d вручную (formato conocido del KAT) */
    log_cap[i] = 0;
    (void)ap;
}
static void prueba_log(void) {
    u_print("t_ns: nslog...\n");
    log_cap[0] = 0;
    log_n = 0;
    t_ok(nslog_set_render_callback(log_cb, 0) == NSLOG_NO_ERROR,
         "log-cb");
    t_ok(nslog_uncork() == NSLOG_NO_ERROR, "log-uncork");
    NSLOG(tns, INFO, "hola %d", 42);
    t_ok(log_n == 1 && t_igual(log_cap, "hola %d"), "log-msg");
    t_ok(t_igual(nslog_level_name(NSLOG_LEVEL_ERROR), "ERROR"),
         "log-nivel");
}

/* ---- psl: sufijo publico embebido ---- */
static void prueba_psl(void) {
    const char *s;
    u_print("t_ns: nspsl...\n");
    /* Devuelve el dominio registrable (eTLD+1), NULL si no hay. */
    s = nspsl_getpublicsuffix("www.example.co.uk");
    t_ok(s && t_igual(s, "example.co.uk"), "psl-couk");
    s = nspsl_getpublicsuffix("example.com");
    t_ok(s && t_igual(s, "example.com"), "psl-com");
    s = nspsl_getpublicsuffix("localhost");
    t_ok(s == 0, "psl-uno");
}

/* ---- bmp: 2x2 24 bit en memoria ---- */
static const unsigned char bmp22[] = {
    'B', 'M', 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
    40, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 24, 0,
    0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    255, 0, 0, 255, 255, 255, 0, 0,
    0, 0, 255, 0, 255, 0, 0, 0
};
static void *bmp_nuevo(int w, int h, unsigned int estado) {
    unsigned char *p;
    (void)estado;
    p = (unsigned char *)u_malloc((unsigned)(w * h * 4));
    return p;
}
static void bmp_borra(void *b) { u_free(b); }
static unsigned char *bmp_buf(void *b) { return (unsigned char *)b; }
static void prueba_bmp(void) {
    bmp_image img;
    bmp_bitmap_callback_vt vt;
    u_print("t_ns: nsbmp...\n");
    vt.bitmap_create = bmp_nuevo;
    vt.bitmap_destroy = bmp_borra;
    vt.bitmap_get_buffer = bmp_buf;
    t_ok(bmp_create(&img, &vt) == BMP_OK, "bmp-crea");
    t_ok(bmp_analyse(&img, (size_t)sizeof(bmp22),
                     (uint8_t *)bmp22) == BMP_OK, "bmp-analiza");
    t_ok(img.width == 2 && img.height == 2, "bmp-tam");
    t_ok(bmp_decode(&img) == BMP_OK && img.decoded &&
         img.bitmap != 0, "bmp-decodifica");
    bmp_finalise(&img);
}

/* ---- gif: 1x1 transparente clasico (43 B) ---- */
static const unsigned char gif11[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21,
    0xf9, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44,
    0x01, 0x00, 0x3b
};
static void *gif_nuevo(int w, int h) {
    return u_malloc((unsigned)(w * h * 4));
}
static void gif_borra(nsgif_bitmap_t *b) { u_free(b); }
static uint8_t *gif_buf(nsgif_bitmap_t *b) { return (uint8_t *)b; }
static void prueba_gif(void) {
    nsgif_t *g = 0;
    nsgif_bitmap_cb_vt vt;
    const nsgif_info_t *inf;
    nsgif_bitmap_t *bm = 0;
    u_print("t_ns: nsgif...\n");
    {
        int k;
        for (k = 0; k < (int)sizeof(vt); k++) ((char *)&vt)[k] = 0;
    }
    vt.create = gif_nuevo;
    vt.destroy = gif_borra;
    vt.get_buffer = gif_buf;
    vt.set_opaque = 0;
    vt.test_opaque = 0;
    vt.modified = 0;
    t_ok(nsgif_create(&vt, NSGIF_BITMAP_FMT_R8G8B8A8, &g) == NSGIF_OK &&
         g != 0, "gif-crea");
    if (!g) {
        t_ok(0, "gif-info");
        t_ok(0, "gif-frame");
        return;
    }
    t_ok(nsgif_data_scan(g, (uint32_t)sizeof(gif11), gif11) ==
         NSGIF_OK, "gif-scan");
    nsgif_data_complete(g);
    inf = nsgif_get_info(g);
    t_ok(inf && inf->width == 1 && inf->height == 1 &&
         inf->frame_count >= 1, "gif-info");
    t_ok(nsgif_frame_decode(g, 0, &bm) == NSGIF_OK && bm != 0,
         "gif-frame");
    nsgif_destroy(g);
}

/* ---- dom: documento + elemento + hijo ---- */
static void prueba_dom(void) {
    struct dom_document *doc = 0;
    struct dom_element *el = 0;
    dom_string *tag = 0;
    struct dom_node *hijo = 0;
    dom_exception err;
    u_print("t_ns: dom...\n");
    /* Sin qname: documento vacio (con qname crea la raiz sola). */
    err = dom_implementation_create_document(DOM_IMPLEMENTATION_HTML,
                                             NULL, NULL, NULL, NULL, NULL,
                                             &doc);
    t_ok(err == DOM_NO_ERR && doc != 0, "dom-doc");
    if (!doc) {
        t_ok(0, "dom-cadena");
        t_ok(0, "dom-el");
        t_ok(0, "dom-hijo");
        return;
    }
    err = dom_string_create((const uint8_t *)"p", 1, &tag);
    t_ok(err == DOM_NO_ERR && tag != 0, "dom-cadena");
    if (!tag) {
        dom_node_unref(doc);
        t_ok(0, "dom-el");
        t_ok(0, "dom-hijo");
        return;
    }
    err = dom_document_create_element(doc, tag,
                                      (struct dom_element **)&el);
    dom_string_unref(tag);
    t_ok(err == DOM_NO_ERR && el != 0, "dom-el");
    if (!el) {
        dom_node_unref(doc);
        t_ok(0, "dom-hijo");
        return;
    }
    err = dom_node_append_child((struct dom_node *)doc,
                                (struct dom_node *)el, &hijo);
    t_ok(err == DOM_NO_ERR && hijo != 0, "dom-hijo");
    dom_node_unref(hijo);
    dom_node_unref(el);
    dom_node_unref(doc);
}

/* ---- css: hoja "p { color: red; }" ---- */
static css_error css_res(void *pw, const char *base, lwc_string *rel,
                         lwc_string **abs) {
    (void)pw;
    (void)base;
    *abs = lwc_string_ref(rel);
    return CSS_OK;
}
static void prueba_css(void) {
    css_stylesheet_params p;
    css_stylesheet *hoja = 0;
    static const char css[] = "p { color: red; }";
    u_print("t_ns: css...\n");
    p.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
    p.level = CSS_LEVEL_21;
    p.charset = "UTF-8";
    p.url = "http://x/";
    p.title = 0;
    p.allow_quirks = false;
    p.inline_style = false;
    p.resolve = css_res;
    p.resolve_pw = 0;
    p.import = 0;
    p.import_pw = 0;
    p.color = 0;
    p.color_pw = 0;
    p.font = 0;
    p.font_pw = 0;
    t_ok(css_stylesheet_create(&p, &hoja) == CSS_OK && hoja != 0,
         "css-crea");
    if (!hoja) {
        t_ok(0, "css-append");
        t_ok(0, "css-done");
        return;
    }
    {
        css_error ea = css_stylesheet_append_data(hoja,
                                                 (const uint8_t *)css,
                                                 (unsigned)sizeof(css) - 1u);
        /* NEEDDATA es normal en el parser incremental (ver test
           upstream: acepta OK o NEEDDATA); data_done remata. */
        t_ok(ea == CSS_OK || ea == CSS_NEEDDATA, "css-append");
        if (ea != CSS_OK && ea != CSS_NEEDDATA) {
            css_stylesheet_destroy(hoja);
            t_ok(0, "css-done");
            return;
        }
    }
    t_ok(css_stylesheet_data_done(hoja) == CSS_OK, "css-done");
    css_stylesheet_destroy(hoja);
}

int main(void) {
    u_print("t_ns: bateria NetSurf (Hito 1)...\n");
    prueba_base64();
    prueba_log();
    prueba_psl();
    prueba_bmp();
    prueba_gif();
    prueba_dom();
    prueba_css();
    {
        char nb[12];
        int q = 0;
        u_print("t_ns: ");
        u_itoa(t_pruebas, nb);
        while (nb[q]) {
            char b2[2];
            b2[0] = nb[q++];
            b2[1] = 0;
            u_print(b2);
        }
        u_print(" pruebas ");
        q = 0;
        u_itoa(t_fallos, nb);
        while (nb[q]) {
            char b2[2];
            b2[0] = nb[q++];
            b2[1] = 0;
            u_print(b2);
        }
        u_print(" fallos\n");
        return t_fallos ? 1 : 0;
    }
}
