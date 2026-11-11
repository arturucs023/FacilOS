#include "ulib.h"

void __main(void) {
}

/* t_fb (F7-C): bateria de FacilBrowser. Unitaria siempre (parser,
   URLs, layout); viva contra http_stub si hay IP configurada.
   Sin red solo avisa y sigue (via negativa como t_red). */

static int pruebas = 0;
static int fallos = 0;

static void ok(int cond, const char *nom) {
    char b[96];
    int i = 0, t = 0;
    pruebas++;
    if (!cond) {
        fallos++;
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

static u_html_doc_t d;
static u_html_fila_t filas[U_HTML_MAX_FILA];

static int parsea(const char *s, const char *base) {
    int n = u_strlen(s);
    if (u_html_crea(&d)) return -99;
    {
        int k = u_html_parse(&d, (const unsigned char *)s, n, base);
        if (k < 0) {
            u_html_libera(&d);
            return -98;
        }
    }
    return 0;
}

static void suelta(void) {
    u_html_libera(&d);
}

static int texto_igual(const char *esperado) {
    int n = u_strlen(esperado), i;
    if (d.texto_n != n) return 0;
    for (i = 0; i < n; i++) {
        if (d.texto[i] != esperado[i]) return 0;
    }
    return 1;
}

static void prueba_minimo(void) {
    u_print("t_fb: parser minimo...\n");
    ok(parsea("<html><body><p>hola</p></body></html>",
        "http://a/") == 0, "minimo-parsea");
    ok(d.nbloq == 1 && d.bloq[0].tipo == U_HTML_B_P, "minimo-bloque-p");
    ok(texto_igual("hola"), "minimo-texto");
    ok(d.truncado == 0, "minimo-sin-truncar");
    suelta();
    ok(parsea("solo texto", "http://a/") == 0, "texto-suelto");
    ok(d.nbloq == 1 && d.bloq[0].tipo == U_HTML_B_TXT,
        "texto-suelto-bloque");
    suelta();
    ok(parsea("", "http://a/") == 0, "vacio");
    ok(d.nbloq == 0, "vacio-sin-bloques");
    suelta();
    ok(parsea("<html><head><title>Mi Ti</title></head><body><p>x</p></body></html>",
        "http://a/") == 0, "titulo");
    ok(!u_strcmp(d.titulo, "Mi Ti"), "titulo-texto");
    suelta();
}

static void prueba_titulos_listas(void) {
    int i;
    u_print("t_fb: titulos y listas...\n");
    ok(parsea("<h1>A</h1><h2>B</h2><h3>C</h3><h4>D</h4><h5>E</h5><h6>F</h6>",
        "http://a/") == 0, "titulos");
    ok(d.nbloq == 6, "titulos-nbloq");
    for (i = 0; i < 6; i++) {
        if (d.bloq[i].tipo != U_HTML_B_H1 + i ||
            d.bloq[i].nivel != i + 1)
            break;
    }
    ok(i == 6, "titulos-tipos");
    suelta();
    ok(parsea("<ul><li>uno</li><li>dos</li></ul><ol><li>tres</li></ol>",
        "http://a/") == 0, "listas");
    ok(d.nbloq == 3, "listas-nbloq");
    ok(d.bloq[0].tipo == U_HTML_B_LI && d.bloq[0].nivel == 1,
        "listas-ul-nivel");
    ok(d.bloq[2].tipo == U_HTML_B_LI && d.bloq[2].nivel < 0,
        "listas-ol-ordenada");
    ok(d.bloq[2].seg_ini < d.nseg, "listas-ol-tiene-numero");
    suelta();
    ok(parsea("<ul><li>a<ul><li>b</li></ul></li></ul>",
        "http://a/") == 0, "lista-anidada");
    ok(d.nbloq == 2 && d.bloq[1].nivel == 2, "lista-anidada-nivel");
    suelta();
    ok(parsea("<hr><br><div>x</div><span>y</span>",
        "http://a/") == 0, "separadores");
    ok(d.nbloq >= 3, "separadores-nbloq");
    suelta();
}

static void prueba_enlaces_imgs(void) {
    u_print("t_fb: enlaces e imagenes...\n");
    ok(parsea("<p><a href=\"/x\">aqui</a> y <a href='http://b/y'>alla</a></p>",
        "http://a/base/p") == 0, "enlaces");
    ok(d.nenl == 2, "enlaces-nenl");
    ok(!u_strcmp(d.enl[0].url, "http://a/x"), "enlaces-rel-abs");
    ok(!u_strcmp(d.enl[1].url, "http://b/y"), "enlaces-abs");
    ok(d.nseg >= 2, "enlaces-segs");
    suelta();
    ok(parsea("<p><a href=https://s/x>seguro</a></p>",
        "http://a/") == 0, "enlace-https-no-nav");
    ok(d.nenl == 0, "enlace-https-sin-idx");
    suelta();
    ok(parsea("<p><a>sin href</a></p>", "http://a/") == 0,
        "enlace-sin-href");
    ok(d.nenl == 0, "enlace-sin-href-idx");
    suelta();
    ok(parsea("<img src=\"/i.png\" alt=\"foto\"><img src=\"r.png\">",
        "http://a/b/") == 0, "imgs");
    ok(d.nimg == 2, "imgs-nimg");
    ok(!u_strcmp(d.img[0].src, "http://a/i.png"), "imgs-src-abs");
    ok(!u_strcmp(d.img[0].alt, "foto"), "imgs-alt");
    ok(d.img[1].alt[0] == 0, "imgs-sin-alt");
    ok(d.bloq[0].tipo == U_HTML_B_IMG, "imgs-bloque");
    suelta();
}

static void prueba_entidades_utf8(void) {
    u_print("t_fb: entidades y UTF-8...\n");
    ok(parsea("<p>&amp;&lt;&gt;&quot;&apos;&#65;&#x41;&nbsp;</p>",
        "http://a/") == 0, "entidades");
    ok(d.texto_n == 9, "entidades-bytes");
    ok(d.texto[0] == '&' && d.texto[1] == '<' &&
        d.texto[2] == '>' && d.texto[3] == '"' &&
        d.texto[4] == '\'' && d.texto[5] == 'A' &&
        d.texto[6] == 'A', "entidades-ascii");
    ok(((unsigned char)d.texto[7] & 0xC0) == 0xC0,
        "entidades-nbsp");
    suelta();
    ok(parsea("<p>&desconocida; fin</p>", "http://a/") == 0,
        "entidad-rara");
    ok(texto_igual("&desconocida; fin"), "entidad-rara-literal");
    suelta();
    ok(parsea("<p>cana</p>", "http://a/") == 0, "ascii-simple");
    ok(texto_igual("cana"), "ascii-simple-texto");
    suelta();
    ok(parsea("<p>a\xFF""b</p>", "http://a/") == 0, "utf8-mal");
    ok(d.texto_n == 5, "utf8-mal-fffd");
    suelta();
}

static void prueba_robustos(void) {
    u_print("t_fb: robustez...\n");
    ok(parsea("<!-- hola --><p>x</p>", "http://a/") == 0,
        "comentario");
    ok(texto_igual("x"), "comentario-fuera");
    suelta();
    ok(parsea("<script>if(a<b){mala();}</script><p>y</p>",
        "http://a/") == 0, "script");
    ok(texto_igual("y"), "script-fuera");
    suelta();
    ok(parsea("<style>p{color:red;}</style><p>z</p>", "http://a/") == 0,
        "style");
    ok(texto_igual("z"), "style-fuera");
    suelta();
    ok(parsea("<foo>misterio</foo><p>ok</p>", "http://a/") == 0,
        "desconocida");
    ok(texto_igual("misteriook"), "desconocida-contenido");
    suelta();
    ok(parsea("<p>a<p>b", "http://a/") == 0, "mal-cerrada-p");
    ok(d.nbloq == 2, "mal-cerrada-p-nbloq");
    suelta();
    ok(parsea("<b><i>x</b></i>", "http://a/") == 0, "cruce");
    ok(texto_igual("x"), "cruce-texto");
    suelta();
    ok(parsea("<p>incompleto", "http://a/") == 0, "incompleto");
    ok(texto_igual("incompleto"), "incompleto-texto");
    suelta();
    ok(parsea("<p>sin fin de tag <b", "http://a/") == 0,
        "tag-cortado");
    suelta();
    ok(parsea("<a href=\"/x\">t</a><a href=\"/y\">u</a>",
        "http://a/") == 0, "dos-enlaces-cierre");
    ok(d.nenl == 2 && d.enl[0].len > 0 && d.enl[1].len > 0,
        "dos-enlaces-largos");
    suelta();
}

static void prueba_urls(void) {
    char sal[U_HTML_URL];
    u_print("t_fb: URLs...\n");
    ok(u_html_resuelve_url("http://a/b/c", "/x", sal) == 0 &&
        !u_strcmp(sal, "http://a/x"), "url-absoluta");
    ok(u_html_resuelve_url("http://a/b/c", "x", sal) == 0 &&
        !u_strcmp(sal, "http://a/b/x"), "url-relativa");
    ok(u_html_resuelve_url("http://a/b/c", "../x", sal) == 0 &&
        !u_strcmp(sal, "http://a/x"), "url-padre");
    ok(u_html_resuelve_url("http://a/b/c", "./x", sal) == 0 &&
        !u_strcmp(sal, "http://a/b/x"), "url-punto");
    ok(u_html_resuelve_url("http://a/b/c", "?q=1", sal) == 0 &&
        !u_strcmp(sal, "http://a/b/c?q=1"), "url-query");
    ok(u_html_resuelve_url("http://a/b/c", "#f", sal) == 0 &&
        !u_strcmp(sal, "http://a/b/c"), "url-frag");
    ok(u_html_resuelve_url("http://a/b", "https://s/x", sal) == -2,
        "url-https-no");
    ok(u_html_resuelve_url("http://a/b", "javascript:x()", sal) == -2,
        "url-js-no");
    ok(u_html_resuelve_url("http://a/b", "mailto:a@b", sal) == -2,
        "url-mail-no");
    ok(u_html_resuelve_url("http://a/b",
        "http://otro:81/r?x=1#f", sal) == 0 &&
        !u_strcmp(sal, "http://otro:81/r?x=1"), "url-abs-puerto");
    ok(u_html_resuelve_url("", "http://a/x", sal) == 0 &&
        !u_strcmp(sal, "http://a/x"), "url-sin-base-abs");
    ok(u_html_resuelve_url("", "rel", sal) != 0,
        "url-sin-base-rel-no");
    ok(u_html_resuelve_url(0, "/x", sal) == -1, "url-args-nul");
}

static void prueba_topes(void) {
    static char grande[4608];
    u_print("t_fb: topes...\n");
    {
        char prof[512];
        int t = 0, k;
        for (k = 0; k < 40 && t < 500; k++) {
            prof[t++] = '<';
            prof[t++] = 'b';
            prof[t++] = '>';
        }
        prof[t++] = 'x';
        prof[t] = 0;
        ok(parsea(prof, "http://a/") == 0, "pila-honda");
        ok(d.truncado == 1, "pila-honda-trunca");
        ok(texto_igual("x"), "pila-honda-texto");
        suelta();
    }
    {
        char rep[8];
        int t = 0, k;
        rep[t++] = '<';
        rep[t++] = 'p';
        rep[t++] = '>';
        rep[t++] = 'x';
        rep[t++] = '<';
        rep[t++] = '/';
        rep[t++] = 'p';
        rep[t++] = '>';
        t = 0;
        for (k = 0; k < 550; k++) {
            int j;
            for (j = 0; j < 8 && t < 4590; j++) grande[t++] = rep[j];
        }
        grande[t] = 0;
    }
    ok(1, "muchos-bloques-genera");
    ok(parsea(grande, "http://a/") == 0, "muchos-bloques");
    ok(d.truncado == 1 && d.nbloq == U_HTML_MAX_BLOQ,
        "muchos-bloques-tope");
    suelta();
}

/* v2.4.3 chunked: decodificador incremental (ver u_http_res_datos). */
static int mete_trozos(const unsigned char *m, int n, int paso,
                       unsigned char *cpo, int cap, u_http_res_t *r,
                       int *nbody) {
    int o = 0, q = 0;
    u_http_res_inicia(r);
    *nbody = 0;
    while (o < n) {
        int t = n - o < paso ? n - o : paso;
        q = u_http_res_datos(r, m + o, t, cpo, cap, nbody);
        if (q != 0) break;
        o += t;
    }
    return q;
}

static int cuerpo_igual(const unsigned char *cpo, const char *esp) {
    int i = 0;
    while (esp[i] && i < 300) {
        if (cpo[i] != (unsigned char)esp[i]) return 0;
        i++;
    }
    return esp[i] == 0;
}

#ifdef F32_NSHUBBUB
/* v2.4.3+hubbub: mismo contrato que parsea() pero tokenizado con
   Hubbub (HTML5 real). -98 = hubbub no disponible/fallo. */
static int parsea_hb(const char *s, const char *base) {
    int n = u_strlen(s);
    if (u_html_crea(&d)) return -99;
    {
        int k = u_html_parse_hb(&d, (const unsigned char *)s, n, base);
        if (k < 0) {
            u_html_libera(&d);
            return -98;
        }
    }
    return 0;
}

static unsigned hb_suma(void) {
    unsigned s = 0;
    int i;
    for (i = 0; i < d.texto_n; i++)
        s += (unsigned char)d.texto[i];
    return s;
}

/* Paridad legado-vs-hubbub sobre el mismo documento: mismo texto,
   bloques, enlaces, titulo y formulario. */
static void paridad(const char *s, const char *base, const char *nom) {
    int tn0, bl0, en0, im0, tt0 = 0, f0 = 0, okp = 1;
    unsigned sum0;
    char tit0[128], for0[512], cam0[64];
    int q;
    if (parsea(s, base) != 0) {
        ok(0, nom);
        return;
    }
    tn0 = d.texto_n;
    bl0 = d.nbloq;
    en0 = d.nenl;
    im0 = d.nimg;
    sum0 = hb_suma();
    for (q = 0; q < 127 && d.titulo[q]; q++) tit0[q] = d.titulo[q];
    tit0[q < 127 ? q : 127] = 0;
    for (q = 0; q < 511 && d.forma[q]; q++) for0[q] = d.forma[q];
    for0[q < 511 ? q : 511] = 0;
    for (q = 0; q < 63 && d.campo[q]; q++) cam0[q] = d.campo[q];
    cam0[q < 63 ? q : 63] = 0;
    tt0 = d.titulo[0] != 0;
    f0 = d.hay_forma;
    suelta();
    if (parsea_hb(s, base) != 0) {
        ok(0, nom);
        return;
    }
    if (d.texto_n != tn0) okp = 0;
    if (okp) {
        int i;
        /* compara por suma + longitudes (evita copiar 48K) */
        if (hb_suma() != sum0) okp = 0;
    }
    if (d.nbloq != bl0) okp = 0;
    if (d.nenl != en0) okp = 0;
    if (d.nimg != im0) okp = 0;
    if ((d.titulo[0] != 0) != tt0) okp = 0;
    else if (tt0 && u_strcmp(d.titulo, tit0)) okp = 0;
    if (d.hay_forma != f0) okp = 0;
    else if (f0 && (u_strcmp(d.forma, for0) || u_strcmp(d.campo, cam0)))
        okp = 0;
    ok(okp, nom);
    suelta();
}

static void prueba_hubbub(void) {
    int nf;
    u_print("t_fb: hubbub...\n");
    ok(parsea_hb("<html><body><p>hola</p></body></html>",
        "http://a/") == 0, "hb-minimo-parsea");
    ok(d.nbloq == 1 && d.bloq[0].tipo == U_HTML_B_P, "hb-minimo-bloque");
    ok(texto_igual("hola"), "hb-minimo-texto");
    suelta();
    ok(parsea_hb("<p>&amp;&lt;&gt;&quot;&apos;&#65;&#x41;&nbsp;</p>",
        "http://a/") == 0, "hb-entidades");
    ok(d.texto_n == 9, "hb-entidades-bytes");
    ok(d.texto[0] == '&' && d.texto[1] == '<' &&
        d.texto[2] == '>' && d.texto[3] == '"' &&
        d.texto[4] == '\'' && d.texto[5] == 'A' &&
        d.texto[6] == 'A', "hb-entidades-ascii");
    ok(((unsigned char)d.texto[7] & 0xC0) == 0xC0,
        "hb-entidades-nbsp");
    suelta();
    ok(parsea_hb("<p>&desconocida; fin</p>", "http://a/") == 0,
        "hb-entidad-rara");
    ok(texto_igual("&desconocida; fin"), "hb-entidad-rara-literal");
    suelta();
    ok(parsea_hb("<!DOCTYPE html><!-- x --><p>y</p>", "http://a/") == 0,
        "hb-doctype-comentario");
    ok(texto_igual("y"), "hb-comentario-fuera");
    suelta();
    ok(parsea_hb("<script>if(a<b){mala();}</script><p>ok</p>",
        "http://a/") == 0, "hb-script");
    ok(texto_igual("ok"), "hb-script-fuera");
    suelta();
    ok(parsea_hb("<style>p{color:red;}</style><p>z</p>",
        "http://a/") == 0, "hb-style");
    ok(texto_igual("z"), "hb-style-fuera");
    suelta();
    ok(parsea_hb("<html><head><title>Mi Ti</title></head>"
        "<body><p>x</p></body></html>", "http://a/") == 0,
        "hb-titulo");
    ok(!u_strcmp(d.titulo, "Mi Ti"), "hb-titulo-texto");
    suelta();
    ok(parsea_hb("<P CLASS=\"v\">T</P>", "http://a/") == 0,
        "hb-mayusculas");
    ok(d.nbloq == 1 && d.bloq[0].tipo == U_HTML_B_P,
        "hb-mayus-bloque");
    suelta();
    ok(parsea_hb("<p>hola", "http://a/") == 0, "hb-sin-cerrar");
    ok(texto_igual("hola"), "hb-sin-cerrar-texto");
    suelta();
    ok(parsea_hb("<p>a<br/>b</p>", "http://a/") == 0, "hb-selfclose");
    ok(texto_igual("ab"), "hb-selfclose-texto");
    suelta();
    ok(parsea_hb("<object><p>q</p></object><p>w</p>", "http://a/") == 0,
        "hb-object-salta");
    ok(texto_igual("w"), "hb-object-fuera");
    suelta();
    ok(parsea_hb("<p><a href=\"/x?a=1&amp;b=2\">t</a></p>",
        "http://a/base/p") == 0, "hb-enlace-args");
    ok(d.nenl == 1, "hb-enlace-args-nenl");
    ok(!u_strcmp(d.enl[0].url, "http://a/x?a=1&b=2"),
        "hb-enlace-args-url");
    suelta();
    ok(parsea_hb("<img src=\"/i.png\" alt=\"foto\">",
        "http://a/b/") == 0, "hb-img");
    ok(d.nimg == 1 && !u_strcmp(d.img[0].src, "http://a/i.png"),
        "hb-img-src");
    suelta();
    ok(parsea_hb("<form action=\"/busca\" method=\"get\">"
        "<input name=\"q\"></form>", "http://a/") == 0,
        "hb-forma");
    ok(d.hay_forma == 1 && !u_strcmp(d.forma, "http://a/busca") &&
        !u_strcmp(d.campo, "q"), "hb-forma-datos");
    suelta();
    ok(parsea_hb("<style>p{color:red}</style><p>hola</p>",
        "http://a/") == 0, "hb-css-regla");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf > 0 && filas[0].fg == 0xFF0000, "hb-css-rojo");
    suelta();
    ok(parsea_hb("<p style=\"color:#00ff00\">x</p>",
        "http://a/") == 0, "hb-css-inline");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf > 0 && filas[0].fg == 0x00FF00, "hb-css-inline-verde");
    suelta();
    paridad("<html><body><p>hola <b>mundo</b></p></body></html>",
        "http://a/", "hb-par-minimo");
    paridad("<h1>T</h1><p>uno <a href=\"/x\">dos</a></p>"
        "<ul><li>a</li><li>b</li></ul>", "http://a/",
        "hb-par-bloques");
    paridad("<html><head><title>Ti</title><style>p{color:red}</style>"
        "</head><body><p>&amp; <b>negrita</b></p>"
        "<form action=\"/b\"><input name=\"q\"></form></body></html>",
        "http://a/base/", "hb-par-pagina");
    paridad("solo texto", "http://a/", "hb-par-texto");
    paridad("<script>var x = 1 < 2;</script><p>tras</p>", "http://a/",
        "hb-par-script");
    paridad("<!-- c --><p>a</p><!-- d --><p>b</p>", "http://a/",
        "hb-par-comentarios");
    paridad("<TABLE><TR><TD>c1</TD><TD>c2</TD></TR></TABLE>",
        "http://a/", "hb-par-mayus");
}
#else
static void prueba_hubbub(void) {
    u_print("t_fb: hubbub no compilado (omitido, sin contar)\n");
}
#endif

static void prueba_chunked(void) {
    static unsigned char cpo[512];
    static const char cab[] =
        "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    u_print("t_fb: chunked...\n");
    {
        /* simple de una pieza */
        static const char m[] =
            "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nhola0\r\n0\r\n\r\n";
        u_http_res_t r;
        int nb = 0, q;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, (int)sizeof(cpo),
                             &nb);
        ok(q == 1 && nb == 5 && cuerpo_igual(cpo, "hola0"),
           "chunk-simple");
    }
    {
        /* varios trozos + extension + trailers, partido a varios pasos */
        static const char m[] =
            "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "A;ext=x\r\n0123456789\r\n3\r\nabc\r\n"
            "0\r\nX-T: 1\r\nY: 2\r\n\r\n";
        int pasos[6], ip, todo = 1;
        u_http_res_t r;
        int nb = 0, q = 0;
        pasos[0] = 1;
        pasos[1] = 2;
        pasos[2] = 3;
        pasos[3] = 5;
        pasos[4] = 7;
        pasos[5] = 64;
        for (ip = 0; ip < 6; ip++) {
            nb = 0;
            q = mete_trozos((const unsigned char *)m,
                            (int)sizeof(m) - 1, pasos[ip], cpo,
                            (int)sizeof(cpo), &r, &nb);
            if (!(q == 1 && nb == 13 &&
                  cuerpo_igual(cpo, "0123456789abc"))) {
                todo = 0;
                break;
            }
        }
        ok(todo, "chunk-trozos");
        (void)cab;
    }
    {
        /* tamano malformado */
        static const char m[] =
            "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "ZZ\r\nxx\r\n0\r\n\r\n";
        u_http_res_t r;
        int nb = 0, q;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, (int)sizeof(cpo),
                             &nb);
        ok(q == -1, "chunk-mal-hex");
    }
    {
        /* truncado: sin terminador -> res_fin falla */
        static const char m[] =
            "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nho";
        u_http_res_t r;
        int nb = 0, q, qf;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, (int)sizeof(cpo),
                             &nb);
        qf = u_http_res_fin(&r);
        ok(q == 0 && qf == -1, "chunk-corte");
    }
    {
        /* tope de cuerpo: 5 bytes con cap 4 */
        static const char m[] =
            "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nhola0\r\n0\r\n\r\n";
        u_http_res_t r;
        int nb = 0, q;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, 4, &nb);
        ok(q == -1 && r.err == -8, "chunk-tope");
    }
    {
        /* CL + chunked: manda chunked, CL se ignora */
        static const char m[] =
            "HTTP/1.0 200 OK\r\nContent-Length: 999\r\n"
            "Transfer-Encoding: chunked\r\n\r\n"
            "2\r\nok\r\n0\r\n\r\n";
        u_http_res_t r;
        int nb = 0, q;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, (int)sizeof(cpo),
                             &nb);
        ok(q == 1 && nb == 2 && cuerpo_igual(cpo, "ok"),
           "chunk-cl-ignora");
    }
    {
        /* chunk-0 directo: cuerpo vacio */
        static const char m[] =
            "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "0\r\n\r\n";
        u_http_res_t r;
        int nb = 0, q;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, (int)sizeof(cpo),
                             &nb);
        ok(q == 1 && nb == 0, "chunk-vacio");
    }
}

/* v2.4.3: tolerancia a cabeceras largas desconocidas (CSP reales de
   600+ B, caso iana.org): se ignoran; el framing sigue estricto y la
   linea sigue topada en U_HTTP_MAX_LIN (1024). */
static void prueba_cablarga(void) {
    static unsigned char cpo[512];
    static unsigned char msg[2048];
    u_print("t_fb: cablarga...\n");
    {
        /* X-Relleno 600 B + CL: debe ignorarse y entregar el cuerpo */
        static const char p1[] = "HTTP/1.0 200 OK\r\nX-Relleno: ";
        static const char p2[] = "\r\nContent-Length: 5\r\n\r\nhola0";
        int o = 0, i, nb = 0, q;
        u_http_res_t r;
        for (i = 0; p1[i]; i++) msg[o++] = (unsigned char)p1[i];
        for (i = 0; i < 600; i++) msg[o++] = (unsigned char)'A';
        for (i = 0; p2[i]; i++) msg[o++] = (unsigned char)p2[i];
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, msg, o, cpo, (int)sizeof(cpo), &nb);
        ok(q == 1 && nb == 5 && cuerpo_igual(cpo, "hola0"),
           "cablarga-cl-ignora");
    }
    {
        /* misma cabecera larga pero en trozos de 7 B (incremental) */
        static const char p1[] = "HTTP/1.0 200 OK\r\nX-Relleno: ";
        static const char p2[] = "\r\nContent-Length: 5\r\n\r\nhola0";
        int o = 0, i, nb = 0, q;
        u_http_res_t r;
        for (i = 0; p1[i]; i++) msg[o++] = (unsigned char)p1[i];
        for (i = 0; i < 600; i++) msg[o++] = (unsigned char)'A';
        for (i = 0; p2[i]; i++) msg[o++] = (unsigned char)p2[i];
        q = mete_trozos(msg, o, 7, cpo, (int)sizeof(cpo), &r, &nb);
        ok(q == 1 && nb == 5 && cuerpo_igual(cpo, "hola0"),
           "cablarga-trozos");
    }
    {
        /* larga + chunked (caso iana.org: CSP + Transfer-Encoding) */
        static const char p1[] = "HTTP/1.0 200 OK\r\nX-Relleno: ";
        static const char p2[] =
            "\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nhola0\r\n0\r\n\r\n";
        int o = 0, i, nb = 0, q;
        u_http_res_t r;
        for (i = 0; p1[i]; i++) msg[o++] = (unsigned char)p1[i];
        for (i = 0; i < 600; i++) msg[o++] = (unsigned char)'A';
        for (i = 0; p2[i]; i++) msg[o++] = (unsigned char)p2[i];
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, msg, o, cpo, (int)sizeof(cpo), &nb);
        ok(q == 1 && nb == 5 && cuerpo_igual(cpo, "hola0"),
           "cablarga-chunked");
    }
    {
        /* framing largo sigue siendo error: CL de 300 digitos */
        static const char p1[] = "HTTP/1.0 200 OK\r\nContent-Length: ";
        static const char p2[] = "\r\n\r\nxx";
        int o = 0, i, nb = 0, q;
        u_http_res_t r;
        for (i = 0; p1[i]; i++) msg[o++] = (unsigned char)p1[i];
        for (i = 0; i < 300; i++) msg[o++] = (unsigned char)'9';
        for (i = 0; p2[i]; i++) msg[o++] = (unsigned char)p2[i];
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, msg, o, cpo, (int)sizeof(cpo), &nb);
        ok(q == -1, "cablarga-cl-rigida");
    }
    {
        /* linea > U_HTTP_MAX_LIN (1100 B) sigue siendo error */
        static const char p1[] = "HTTP/1.0 200 OK\r\nX-Larga: ";
        static const char p2[] = "\r\n\r\nxx";
        int o = 0, i, nb = 0, q;
        u_http_res_t r;
        for (i = 0; p1[i]; i++) msg[o++] = (unsigned char)p1[i];
        for (i = 0; i < 1100; i++) msg[o++] = (unsigned char)'C';
        for (i = 0; p2[i]; i++) msg[o++] = (unsigned char)p2[i];
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, msg, o, cpo, (int)sizeof(cpo), &nb);
        ok(q == -1, "cablarga-linea-tope");
    }
}

/* v2.4.4: jar de cookies + POST (sin red: parser y formato). */
static int cab_igual(const char *s, const char *esp) {
    int i = 0;
    while (esp[i] && i < 120) {
        if (s[i] != esp[i]) return 0;
        i++;
    }
    return esp[i] == 0 && s[i] == 0;
}

static void prueba_galletas(void) {
    char sal[512];
    u_print("t_fb: galletas...\n");
    u_http_galleta_limpia();
    ok(u_http_galleta_cab("a", sal, (int)sizeof(sal)) == 0 &&
       sal[0] == 0, "galleta-vacia");
    ok(u_http_galleta_guarda("a", "sesion=abc123; Path=/") == 0,
       "galleta-guarda");
    ok(u_http_galleta_cab("a", sal, (int)sizeof(sal)) > 0 &&
       cab_igual(sal, "Cookie: sesion=abc123\r\n"),
       "galleta-cab-uno");
    ok(u_http_galleta_guarda("a", "tema=oscuro; Path=/") == 0,
       "galleta-guarda2");
    ok(u_http_galleta_cab("a", sal, (int)sizeof(sal)) > 0 &&
       cab_igual(sal, "Cookie: sesion=abc123; tema=oscuro\r\n"),
       "galleta-cab-dos");
    ok(u_http_galleta_guarda("a", "sesion=zzz") == 0 &&
       u_http_galleta_cab("a", sal, (int)sizeof(sal)) > 0 &&
       cab_igual(sal, "Cookie: sesion=zzz; tema=oscuro\r\n"),
       "galleta-reescribe");
    ok(u_http_galleta_cab("b", sal, (int)sizeof(sal)) == 0 &&
       sal[0] == 0, "galleta-aisla-host");
    ok(u_http_galleta_cab("A.COM", sal, (int)sizeof(sal)) == 0 &&
       sal[0] == 0, "galleta-aisla-mayus");
    ok(u_http_galleta_guarda("h", "x=1; Expires=ayer; Secure") == 0,
       "galleta-attrs");
    ok(u_http_galleta_cab("h", sal, (int)sizeof(sal)) > 0 &&
       cab_igual(sal, "Cookie: x=1\r\n"), "galleta-attrs-fuera");
    ok(u_http_galleta_guarda("h", "t=a=b") == 0 &&
       u_http_galleta_cab("h", sal, (int)sizeof(sal)) > 0 &&
       cab_igual(sal, "Cookie: x=1; t=a=b\r\n"), "galleta-igual-dentro");
    ok(u_http_galleta_guarda("", "s=v") != 0, "galleta-sin-host");
    ok(u_http_galleta_guarda("h", "sinigual") != 0, "galleta-sin-igual");
    u_http_galleta_limpia();
    ok(u_http_galleta_cab("a", sal, (int)sizeof(sal)) == 0 &&
       sal[0] == 0, "galleta-limpia");
    {
        /* nth Set-Cookie: dos iguales, tercera ausente */
        static const char m[] =
            "HTTP/1.0 200 OK\r\n"
            "Set-Cookie: uno=1; Path=/\r\n"
            "Content-Type: text/plain\r\n"
            "Set-Cookie: dos=2\r\n"
            "Content-Length: 2\r\n\r\nok";
        static unsigned char cpo[64];
        u_http_res_t r;
        char v[128];
        int nb = 0, q;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, (int)sizeof(cpo),
                             &nb);
        ok(q == 1, "galleta-resp-ok");
        ok(u_http_cabecera_n(&r, "Set-Cookie", 0, v, (int)sizeof(v)) > 0 &&
           cab_igual(v, "uno=1; Path=/"), "galleta-nth-0");
        ok(u_http_cabecera_n(&r, "SET-cookie", 1, v, (int)sizeof(v)) > 0 &&
           cab_igual(v, "dos=2"), "galleta-nth-1-mayus");
        ok(u_http_cabecera_n(&r, "Set-Cookie", 2, v, (int)sizeof(v)) < 0,
           "galleta-nth-2-no");
        ok(u_http_cabecera_n(&r, "Set-Cookie", -1, v,
                             (int)sizeof(v)) < 0, "galleta-nth-neg");
    }
    {
        /* pide POST: metodo, Content-Length y Tipo */
        u_http_url_t u;
        char pet[512];
        int q;
        u_http_url("http://a/eco-post", &u);
        q = u_http_pide_post(&u, 0, 11, pet, (int)sizeof(pet));
        ok(q > 0, "post-pide-ok");
        {
            int i, bien = 1;
            static const char ini[] = "POST /eco-post HTTP/1.0\r\n";
            for (i = 0; ini[i]; i++)
                if (pet[i] != ini[i]) bien = 0;
            ok(bien, "post-pide-metodo");
        }
        {
            int i = 0, hall = 0;
            while (pet[i]) {
                static const char cl[] = "Content-Length: 11\r\n";
                int j = 0, ok2 = 1;
                while (cl[j] && pet[i + j]) {
                    if (pet[i + j] != cl[j]) {
                        ok2 = 0;
                        break;
                    }
                    j++;
                }
                if (ok2 && !cl[j]) {
                    hall = 1;
                    break;
                }
                i++;
            }
            ok(hall, "post-pide-longitud");
        }
        {
            int i = 0, hall = 0;
            while (pet[i]) {
                static const char ct[] =
                    "Content-Type: application/x-www-form-urlencoded\r\n";
                int j = 0, ok2 = 1;
                while (ct[j] && pet[i + j]) {
                    if (pet[i + j] != ct[j]) {
                        ok2 = 0;
                        break;
                    }
                    j++;
                }
                if (ok2 && !ct[j]) {
                    hall = 1;
                    break;
                }
                i++;
            }
            ok(hall, "post-pide-tipo");
        }
        ok(u_http_pide_post(&u, "Cookie: s=v\r\n", 3, pet,
                            (int)sizeof(pet)) > 0,
           "post-pide-extra");
        {
            /* metodo no soportado: -1 sin tocar la red */
            static unsigned char tb2[64];
            u_http_res_t r2;
            u_http_res_inicia(&r2);
            ok(u_http_trae_x("http://a/x", "PUT", 0, 0, 0, &r2,
                             tb2, (int)sizeof(tb2), 100) == -1 &&
               r2.codigo == 0, "traex-metodo-malo");
        }
    }
    {
        /* parser POST: misma action/campo, marca forma_post */
        suelta();
        ok(parsea("<form method=\"post\" action=\"/eco-post\">"
                 "<input name=\"q\"></form>", "http://a/") == 0,
           "forma-post-parsea");
        ok(d.hay_forma == 0 && d.forma_post == 1, "forma-post-marca");
        ok(!u_strcmp(d.forma, "http://a/eco-post") &&
           !u_strcmp(d.campo, "q"), "forma-post-datos");
        suelta();
        ok(parsea("<form method=\"get\" action=\"/b\">"
                 "<input name=\"q\"></form>", "http://a/") == 0,
           "forma-get-parsea");
        ok(d.hay_forma == 1 && d.forma_post == 0, "forma-get-marca");
        suelta();
        ok(parsea("<form action=\"/b\"><input name=\"q\"></form>",
            "http://a/") == 0, "forma-defecto-parsea");
        ok(d.hay_forma == 1 && d.forma_post == 0, "forma-defecto-get");
        suelta();
    }
}

/* Vectores de referencia (generados con PIL): BMP24 4x3,
   GIF 4x3 (2-4 colores) y GIF 16x16 entrelazado. */
static const unsigned char VBMP24[] = {
    0x42, 0x4d, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00,
    0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00,
    0x00, 0x00, 0xc4, 0x0e, 0x00, 0x00, 0xc4, 0x0e, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff,
    0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00,
    0xff, 0x00, 0x00, 0xff, 0xff, 0xff,
};
static const unsigned char VGIF43[] = {
    0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x04, 0x00, 0x03, 0x00, 0x81, 0x00,
    0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
    0xff, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00, 0x08,
    0x0d, 0x00, 0x01, 0x04, 0x10, 0x30, 0x60, 0xe0, 0x00, 0x00, 0x04, 0x05,
    0x06, 0x04, 0x00, 0x3b,
};
static const unsigned char VGIF16[] = {
    0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x83, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x0d, 0x1d, 0x20, 0x1a, 0x3a, 0x30, 0x27,
    0x57, 0x40, 0x34, 0x74, 0x50, 0x41, 0x91, 0x60, 0x4e, 0xae, 0x70, 0x5b,
    0xcb, 0x80, 0x68, 0xe8, 0x90, 0x75, 0x05, 0xa0, 0x82, 0x22, 0xb0, 0x8f,
    0x3f, 0xc0, 0x9c, 0x5c, 0xd0, 0xa9, 0x79, 0xe0, 0xb6, 0x96, 0xf0, 0xc3,
    0xb3, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x40, 0x08,
    0x69, 0x00, 0x01, 0x04, 0x10, 0x30, 0x80, 0x40, 0x01, 0x03, 0x07, 0x10,
    0x24, 0x50, 0xb0, 0x80, 0x41, 0x03, 0x07, 0x0f, 0x14, 0x32, 0x74, 0x08,
    0x51, 0x20, 0x41, 0x83, 0x08, 0x31, 0x26, 0x5c, 0xd8, 0xf0, 0xe1, 0x03,
    0x8b, 0x03, 0x28, 0x7e, 0x1c, 0x58, 0xf0, 0xe0, 0x46, 0x86, 0x17, 0x4d,
    0x4a, 0xec, 0x58, 0x31, 0x00, 0xc2, 0x95, 0x22, 0x41, 0x1a, 0x9c, 0xe8,
    0x51, 0xa6, 0xca, 0x04, 0x2d, 0x53, 0xbe, 0xe4, 0xe8, 0x90, 0xa4, 0x46,
    0x98, 0x35, 0x4b, 0xee, 0xa4, 0x99, 0xf3, 0x26, 0xd1, 0x91, 0x17, 0x4f,
    0xb2, 0x44, 0x2a, 0x94, 0x67, 0x4d, 0x9f, 0x2a, 0x97, 0xda, 0x1c, 0xfa,
    0x54, 0xa7, 0x52, 0x06, 0x4c, 0x7f, 0x3a, 0x75, 0x10, 0x10, 0x00, 0x3b,
};

static int px_igual(const unsigned char *px, int r, int g, int b) {
    return px[0] == r && px[1] == g && px[2] == b;
}

static void prueba_img(void) {
    u_print("t_fb: img...\n");
    ok(u_img_dec(0, 10, 0) == -1, "img-args-nul");
    {
        u_img_t o;
        static unsigned char tb[16];
        o.w = 1;
        o.px = tb;
        ok(u_img_dec(VBMP24, (int)sizeof(VBMP24), 0) == -1,
           "img-args-sal-nul");
        u_img_lib(&o);
        ok(o.px == 0 && o.w == 0, "img-lib-limpia");
    }
    {
        /* BMP 24-bit 4x3: fila 0 = rojo,verde,azul,blanco */
        u_img_t o;
        static const unsigned char esp0[12] = {
            255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255
        };
        static const unsigned char esp2[12] = {
            0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255, 0
        };
        int i, bien0 = 1, bien2 = 1;
        ok(u_img_dec(VBMP24, (int)sizeof(VBMP24), &o) == 0,
           "img-bmp24-ok");
        ok(o.w == 4 && o.h == 3 && o.px != 0, "img-bmp24-dims");
        for (i = 0; i < 12; i++) {
            if (o.px[i] != esp0[i]) bien0 = 0;
            if (o.px[24 + i] != esp2[i]) bien2 = 0;
        }
        ok(bien0, "img-bmp24-fila0");
        ok(bien2, "img-bmp24-fila2");
        u_img_lib(&o);
    }
    {
        /* BMP 8-bit 4x2 construido (paleta rojo,verde,azul,blanco):
           fila 0 = 0,1,2,3; fila 1 = 3,2,1,0. */
        static unsigned char m[80];
        u_img_t o;
        int i;
        for (i = 0; i < 80; i++) m[i] = 0;
        m[0] = 'B';
        m[1] = 'M';
        m[10] = 70;
        m[14] = 40;
        m[18] = 4;
        m[22] = 2;
        m[26] = 1;
        m[28] = 8;
        m[46] = 4;
        m[54] = 0;
        m[55] = 0;
        m[56] = 255;
        m[58] = 0;
        m[59] = 255;
        m[60] = 0;
        m[62] = 0;
        m[63] = 0;
        m[64] = 255;
        m[65] = 0;
        m[66] = 255;
        m[67] = 255;
        m[68] = 255;
        m[70] = 3;
        m[71] = 2;
        m[72] = 1;
        m[73] = 0;
        m[74] = 0;
        m[75] = 1;
        m[76] = 2;
        m[77] = 3;
        ok(u_img_dec(m, 78, &o) == 0, "img-bmp8-ok");
        ok(o.w == 4 && o.h == 2, "img-bmp8-dims");
        ok(px_igual(o.px, 255, 0, 0) &&
           px_igual(o.px + 9, 255, 255, 255), "img-bmp8-filas");
        ok(px_igual(o.px + 12, 255, 255, 255) &&
           px_igual(o.px + 21, 255, 0, 0), "img-bmp8-abajo");
        u_img_lib(&o);
    }
    {
        /* BMP 1-bit 200x10 blanco: escala a 120x6. */
        static unsigned char m[512];
        u_img_t o;
        int i, j, todo = 1;
        for (i = 0; i < 512; i++) m[i] = 0;
        m[0] = 'B';
        m[1] = 'M';
        m[10] = 62;
        m[14] = 40;
        m[18] = 200;
        m[22] = 10;
        m[26] = 1;
        m[28] = 1;
        m[58] = 255;
        m[59] = 255;
        m[60] = 255;
        for (j = 0; j < 10; j++)
            for (i = 0; i < 28; i++) m[62 + j * 28 + i] = 0xFF;
        ok(u_img_dec(m, 342, &o) == 0, "img-bmp1-ok");
        ok(o.w == 120 && o.h == 6, "img-bmp1-escala");
        for (i = 0; i < 120 * 6 * 3; i++)
            if (o.px[i] != 255) todo = 0;
        ok(todo, "img-bmp1-blanco");
        u_img_lib(&o);
    }
    {
        /* BMP RLE (compresion=1) -> -3 sin colgarse. */
        static unsigned char m[90];
        u_img_t o;
        int i;
        for (i = 0; i < 90; i++) m[i] = VBMP24[i];
        m[30] = 1;
        ok(u_img_dec(m, 90, &o) == -3, "img-bmp-rle");
        ok(o.px == 0, "img-bmp-rle-limpio");
    }
    {
        /* BMP 1000x1000 declarado -> -4 sin reservar. */
        static unsigned char m[54];
        u_img_t o;
        int i;
        for (i = 0; i < 54; i++) m[i] = 0;
        m[0] = 'B';
        m[1] = 'M';
        m[10] = 54;
        m[14] = 40;
        m[18] = 232;
        m[19] = 3;
        m[22] = 232;
        m[23] = 3;
        m[26] = 1;
        m[28] = 24;
        ok(u_img_dec(m, 54, &o) == -4, "img-bmp-grande");
    }
    {
        /* BMP truncado ("BM" + 10 B) -> -2. */
        static const unsigned char m[] = {
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        u_img_t o;
        ok(u_img_dec(m, (int)sizeof(m), &o) == -2,
           "img-bmp-corto");
    }
    {
        /* GIF 4x3: fila 0 = rojo,verde,azul,blanco. */
        u_img_t o;
        ok(u_img_dec(VGIF43, (int)sizeof(VGIF43), &o) == 0,
           "img-gif-ok");
        ok(o.w == 4 && o.h == 3 && o.px != 0, "img-gif-dims");
        ok(px_igual(o.px, 255, 0, 0) &&
           px_igual(o.px + 3, 0, 255, 0) &&
           px_igual(o.px + 6, 0, 0, 255) &&
           px_igual(o.px + 9, 255, 255, 255), "img-gif-fila0");
        ok(px_igual(o.px + 24, 0, 0, 255) &&
           px_igual(o.px + 33, 0, 255, 0), "img-gif-fila2");
        u_img_lib(&o);
    }
    {
        /* GIF 16x16 entrelazado: suma RGB de referencia. */
        u_img_t o;
        int i, s = 0;
        ok(u_img_dec(VGIF16, (int)sizeof(VGIF16), &o) == 0,
           "img-gif16-ok");
        ok(o.w == 16 && o.h == 16, "img-gif16-dims");
        for (i = 0; i < 16 * 16 * 3; i++) s += o.px[i];
        ok(s == 82688, "img-gif16-suma");
        u_img_lib(&o);
    }
    {
        /* Magia mala y GIF truncado -> -2. */
        static const unsigned char m1[] = {
            'G', 'I', 'F', '8', '0', 'a', 0, 0, 0, 0, 0, 0, 0
        };
        static const unsigned char m2[] = {
            'G', 'I', 'F', '8', '9', 'a'
        };
        u_img_t o;
        ok(u_img_dec(m1, (int)sizeof(m1), &o) == -2,
           "img-gif-malo");
        ok(u_img_dec(m2, (int)sizeof(m2), &o) == -2,
           "img-gif-corto");
        ok(u_img_dec(VGIF43, 20, &o) == -2, "img-gif-trunco");
    }
    {
        /* Layout: img_fil=3 -> 3 filas E_IMG con mismo indice. */
        int nf, i, c3 = 0, c1 = 0;
        suelta();
        ok(parsea("<img src=\"/i.png\" alt=\"f\">", "http://a/") ==
           0, "img-maq-parsea");
        d.img_fil[0] = 3;
        nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
        for (i = 0; i < nf; i++) {
            if (filas[i].estilo == U_HTML_E_IMG && filas[i].img == 0)
                c3++;
        }
        ok(nf > 0 && c3 == 3, "img-maq-3filas");
        suelta();
        ok(parsea("<img src=\"/i.png\" alt=\"f\">", "http://a/") ==
           0, "img-maq-parsea2");
        nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
        for (i = 0; i < nf; i++) {
            if (filas[i].estilo == U_HTML_E_IMG && filas[i].img == 0)
                c1++;
        }
        ok(nf > 0 && c1 == 1, "img-maq-1fila");
        suelta();
    }
}

/* Redirecciones 301/302: el codigo las sigue (max 3 saltos) pero
   nunca se probaron. Unitaria del parser + viva del 302 crudo
   (u_http_trae no sigue; el seguimiento lo verifica el harness). */
static void prueba_redir(void) {
    u_print("t_fb: redir...\n");
    {
        static const char m[] =
            "HTTP/1.0 302 Found\r\n"
            "Location: /redir2\r\n"
            "Set-Cookie: ruta=redir; Path=/\r\n"
            "Content-Length: 0\r\n\r\n";
        static unsigned char cpo[64];
        u_http_res_t r;
        char v[128];
        int nb = 0, q;
        u_http_res_inicia(&r);
        q = u_http_res_datos(&r, (const unsigned char *)m,
                             (int)sizeof(m) - 1, cpo, (int)sizeof(cpo),
                             &nb);
        ok(q == 1 && r.codigo == 302, "redir-302");
        ok(u_http_cabecera(&r, "Location", v, (int)sizeof(v)) > 0 &&
           cab_igual(v, "/redir2"), "redir-location");
        ok(u_http_cabecera(&r, "location", v, (int)sizeof(v)) > 0 &&
           cab_igual(v, "/redir2"), "redir-location-mayus");
        ok(u_http_cabecera_n(&r, "Set-Cookie", 0, v,
                             (int)sizeof(v)) > 0 &&
           cab_igual(v, "ruta=redir; Path=/"), "redir-cookie-302");
        ok(u_http_cabecera(&r, "Location", v, 4) > 0,
           "redir-location-corto");
    }
}

static void prueba_layout(void) {
    int nf;
    u_print("t_fb: layout...\n");
    ok(parsea("<p>corto con varias palabras para envolver</p>",
        "http://a/") == 0, "layout-parsea");
    nf = u_html_maqueta(&d, 10, filas, U_HTML_MAX_FILA);
    ok(nf > 1, "layout-envuelve");
    {
        int i, mal = 0;
        for (i = 0; i < nf; i++) {
            if (filas[i].len < 0 || filas[i].x < 0) mal = 1;
        }
        ok(!mal, "layout-filas-sanas");
    }
    suelta();
    ok(parsea("<h1>T</h1><p>x</p><hr><ul><li>y</li></ul>",
        "http://a/") == 0, "layout-varios");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    {
        int i, hr = 0, tit = 0;
        for (i = 0; i < nf; i++) {
            if (filas[i].estilo == U_HTML_E_HR) hr++;
            if (filas[i].estilo == U_HTML_E_TITULO) tit++;
        }
        ok(hr >= 2 && tit >= 1, "layout-hr-titulo");
    }
    suelta();
    ok(parsea("<img src=\"/i.png\" alt=\"foto\">",
        "http://a/") == 0, "layout-img");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    {
        int i, img = 0;
        for (i = 0; i < nf; i++) {
            if (filas[i].estilo == U_HTML_E_IMG) img++;
        }
        ok(img == 1, "layout-img-fila");
    }
    suelta();
    ok(parsea("", "http://a/") == 0, "layout-vacio");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf == 0, "layout-vacio-cero");
    suelta();
    ok(u_html_maqueta(&d, 0, filas, U_HTML_MAX_FILA) == -1,
        "layout-cols-mal");
    ok(u_html_maqueta(0, 40, filas, U_HTML_MAX_FILA) == -1,
        "layout-args-nul");
}

static void prueba_css(void) {
    int nf;
    u_print("t_fb: css...\n");
    ok(parsea("<style>p{color:red}</style><p>hola</p>",
        "http://a/") == 0, "css-regla");
    ok(d.truncado == 0, "css-sin-truncar");
    ok(texto_igual("hola"), "css-texto-fuera");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf > 0 && filas[0].fg == 0xFF0000, "css-rojo");
    suelta();
    ok(parsea("<p style=\"color:#00ff00\">x</p>",
        "http://a/") == 0, "css-inline");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf > 0 && filas[0].fg == 0x00FF00, "css-inline-verde");
    suelta();
    ok(parsea("<style>.c{background:#0000ff}</style>"
        "<p class=\"c\">y</p>", "http://a/") == 0, "css-clase");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf > 0 && filas[0].bg == 0x0000FF, "css-clase-fondo");
    suelta();
    ok(parsea("<style>p{color:red} p{color:blue}</style><p>z</p>",
        "http://a/") == 0, "css-ultima-gana");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf > 0 && filas[0].fg == 0x0000FF, "css-azul-final");
    suelta();
    ok(parsea("<style>p{color:red</style><p>w</p>",
        "http://a/") == 0, "css-mal-cerrado");
    ok(texto_igual("w"), "css-mal-texto");
    suelta();
    ok(parsea("<p style=\"color:raro\">q</p>",
        "http://a/") == 0, "css-valor-malo");
    nf = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
    ok(nf > 0 && filas[0].fg < 0, "css-malo-defecto");
    suelta();
    ok(parsea("<style>p{font-weight:bold}</style><p>T</p>",
        "http://a/") == 0, "css-negrita");
    ok(d.seg[0].est == U_HTML_E_NEGRITA, "css-negrita-est");
    suelta();
}

static void prueba_forma(void) {
    u_print("t_fb: formularios...\n");
    ok(parsea("<form action=\"/busca\" method=\"get\">"
        "<input name=\"q\"></form>", "http://a/") == 0,
        "forma-get");
    ok(d.hay_forma == 1, "forma-hay");
    ok(!u_strcmp(d.forma, "http://a/busca"), "forma-accion");
    ok(!u_strcmp(d.campo, "q"), "forma-campo");
    suelta();
    ok(parsea("<form action=\"/x\" method=\"post\">"
        "<input name=\"q\"></form>", "http://a/") == 0,
        "forma-post");
    ok(d.hay_forma == 0 && d.forma_post == 1, "forma-post-no");
    ok(!u_strcmp(d.forma, "http://a/x") && !u_strcmp(d.campo, "q"),
        "forma-post-datos");
    suelta();
    ok(parsea("<p>sin forma</p>", "http://a/") == 0,
        "forma-ausente");
    ok(d.hay_forma == 0, "forma-ausente-no");
    suelta();
}

static void prueba_viva(const char *base) {    static unsigned char cpo[8192];
    u_http_res_t r;
    int k, n;
    u_print("t_fb: viva (stub)...\n");
    {
        char u[128];
        int i = 0;
        while (base[i] && i < 120) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'p';
        u[i++] = 'a';
        u[i++] = 'g';
        u[i++] = 'i';
        u[i++] = 'n';
        u[i++] = 'a';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 200, "viva-pagina-200");
        if (k == 0) {
            int pr;
            if (u_html_crea(&d)) {
                ok(0, "viva-mem");
                return;
            }
            pr = u_html_parse(&d, cpo, (int)r.cuerpo_n, u);
            ok(pr >= 0 && d.nenl >= 1, "viva-pagina-enlaces");
            ok(d.titulo[0] != 0, "viva-pagina-titulo");
            u_html_libera(&d);
        }
    }
    {
        char u[128];
        int i = 0;
        while (base[i] && i < 120) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'm';
        u[i++] = 'a';
        u[i++] = 'l';
        u[i++] = 'f';
        u[i++] = 'o';
        u[i++] = 'r';
        u[i++] = 'm';
        u[i++] = 'a';
        u[i++] = 'd';
        u[i++] = 'a';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0, "viva-malformada-200");
        if (k == 0) {
            int pr;
            if (u_html_crea(&d)) {
                ok(0, "viva-mem2");
                return;
            }
            pr = u_html_parse(&d, cpo, (int)r.cuerpo_n, u);
            n = u_html_maqueta(&d, 40, filas, U_HTML_MAX_FILA);
            ok(pr >= 0 && n > 0, "viva-malformada-dibuja");
            u_html_libera(&d);
        }
    }
    {
        /* v2.4.3: chunked real contra el stub (/chunked -> "hola0") */
        char u[128];
        int i = 0;
        while (base[i] && i < 110) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'c';
        u[i++] = 'h';
        u[i++] = 'u';
        u[i++] = 'n';
        u[i++] = 'k';
        u[i++] = 'e';
        u[i++] = 'd';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 200 && r.cuerpo_n == 5,
           "viva-chunked-200");
        if (k == 0)
            ok(cpo[0] == 'h' && cpo[1] == 'o' && cpo[2] == 'l' &&
               cpo[3] == 'a' && cpo[4] == '0', "viva-chunked-cuerpo");
    }
    {
        /* v2.4.3: cabecera larga desconocida (/cablarga -> 200 OK) */
        char u[128];
        int i = 0;
        while (base[i] && i < 110) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'c';
        u[i++] = 'a';
        u[i++] = 'b';
        u[i++] = 'l';
        u[i++] = 'a';
        u[i++] = 'r';
        u[i++] = 'g';
        u[i++] = 'a';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 200, "viva-cablarga-200");
        if (k == 0)
            ok(r.cuerpo_n == 15, "viva-cablarga-cuerpo");
    }
    {
        /* v2.4.4: cookies roundtrip (/galleta -> /eco-cookie) */
        char u[128], ue[128];
        u_http_url_t uh;
        char ck[512];
        int i = 0;
        while (base[i] && i < 110) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'g';
        u[i++] = 'a';
        u[i++] = 'l';
        u[i++] = 'l';
        u[i++] = 'e';
        u[i++] = 't';
        u[i++] = 'a';
        u[i] = 0;
        u_http_galleta_limpia();
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 200, "viva-galleta-200");
        if (k == 0) {
            char scv[256];
            int scn = 0, j = 0;
            if (u_http_url(u, &uh) == 0 && uh.host[0]) {
                while (u_http_cabecera_n(&r, "Set-Cookie", scn, scv,
                        (int)sizeof(scv)) >= 0) {
                    u_http_galleta_guarda(uh.host, scv);
                    scn++;
                    if (scn > 32) break;
                }
            }
            ok(scn == 2, "viva-galleta-dos");
            while (base[j] && j < 110) {
                ue[j] = base[j];
                j++;
            }
            ue[j++] = '/';
            ue[j++] = 'e';
            ue[j++] = 'c';
            ue[j++] = 'o';
            ue[j++] = '-';
            ue[j++] = 'c';
            ue[j++] = 'o';
            ue[j++] = 'o';
            ue[j++] = 'k';
            ue[j++] = 'i';
            ue[j++] = 'e';
            ue[j] = 0;
            ck[0] = 0;
            if (uh.host[0])
                u_http_galleta_cab(uh.host, ck, (int)sizeof(ck));
            ok(ck[0] != 0, "viva-galleta-jar");
            k = u_http_trae_x(ue, "GET", ck[0] ? ck : 0, 0, 0, &r,
                              cpo, (int)sizeof(cpo), 15000);
            ok(k == 0 && r.codigo == 200, "viva-eco-200");
            if (k == 0) {
                static const char esp[] =
                    "visto: sesion=abc123; tema=oscuro";
                int le = 0;
                while (esp[le]) le++;
                ok((int)r.cuerpo_n == le && cuerpo_igual(cpo, esp),
                   "viva-eco-cuerpo");
            }
        }
    }
    {
        /* v2.4.4: POST real (/eco-post) + 405 si se pide con GET */
        char u[128];
        int i = 0;
        while (base[i] && i < 110) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'e';
        u[i++] = 'c';
        u[i++] = 'o';
        u[i++] = '-';
        u[i++] = 'p';
        u[i++] = 'o';
        u[i++] = 's';
        u[i++] = 't';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 405, "viva-post-get-405");
        {
            static const unsigned char pb[] = "q=hola";
            k = u_http_trae_x(u, "POST", 0, pb, 6, &r, cpo,
                              (int)sizeof(cpo), 15000);
            ok(k == 0 && r.codigo == 200, "viva-post-200");
            if (k == 0) {
                static const char esp[] = "visto: q=hola";
                int le = 0;
                while (esp[le]) le++;
                ok((int)r.cuerpo_n == le && cuerpo_igual(cpo, esp),
                   "viva-post-cuerpo");
            }
        }
    }
    {
        /* redirecciones crudas: u_http_trae no sigue (lo hace
           fbrowser); aqui el 302 + Location + cookie del 302. */
        char u[128];
        char loc[256];
        int i = 0;
        while (base[i] && i < 110) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'r';
        u[i++] = 'e';
        u[i++] = 'd';
        u[i++] = 'i';
        u[i++] = 'r';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 302, "viva-redir-302");
        if (k == 0) {
            ok(u_http_cabecera(&r, "Location", loc,
                               (int)sizeof(loc)) > 0 &&
               cab_igual(loc, "/redir2"), "viva-redir-location");
        }
        {
            /* segundo salto: 302 a /pagina + Set-Cookie */
            int j = 0;
            while (base[j] && j < 110) {
                u[j] = base[j];
                j++;
            }
            u[j++] = '/';
            u[j++] = 'r';
            u[j++] = 'e';
            u[j++] = 'd';
            u[j++] = 'i';
            u[j++] = 'r';
            u[j++] = '2';
            u[j] = 0;
            k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
            ok(k == 0 && r.codigo == 302, "viva-redir2-302");
            if (k == 0) {
                char scv[256];
                ok(u_http_cabecera(&r, "Location", loc,
                                   (int)sizeof(loc)) > 0 &&
                   cab_igual(loc, "/pagina"), "viva-redir2-location");
                ok(u_http_cabecera_n(&r, "Set-Cookie", 0, scv,
                                     (int)sizeof(scv)) > 0 &&
                   cab_igual(scv, "ruta=redir; Path=/"),
                   "viva-redir2-cookie");
            }
        }
    }
    {
        /* Imagenes por cable: BMP y GIF del stub -> decodifican. */
        char u[128];
        u_img_t o;
        int i = 0, s;
        while (base[i] && i < 110) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'f';
        u[i++] = 'o';
        u[i++] = 't';
        u[i++] = 'o';
        u[i++] = '.';
        u[i++] = 'b';
        u[i++] = 'm';
        u[i++] = 'p';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 200, "viva-bmp-200");
        if (k == 0) {
            ok(u_img_dec(cpo, (int)r.cuerpo_n, &o) == 0,
               "img-bmp-red");
            ok(o.w == 48 && o.h == 32, "img-bmp-red-dims");
            ok(px_igual(o.px, 230, 30, 30), "img-bmp-red-px0");
            u_img_lib(&o);
        }
        u[i - 3] = 'g';
        u[i - 2] = 'i';
        u[i - 1] = 'f';
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 200, "viva-gif-200");
        if (k == 0) {
            ok(u_img_dec(cpo, (int)r.cuerpo_n, &o) == 0,
               "img-gif-red");
            ok(o.w == 48 && o.h == 32, "img-gif-red-dims");
            s = 0;
            for (i = 0; i < o.w * o.h * 3; i++) s += o.px[i];
            ok(s == 574080, "img-gif-red-suma");
            u_img_lib(&o);
        }
    }
    {
        char u[128];
        int i = 0;
        while (base[i] && i < 120) {
            u[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'g';
        u[i++] = 'r';
        u[i++] = 'a';
        u[i++] = 'n';
        u[i++] = 'd';
        u[i++] = 'e';
        u[i++] = '-';
        u[i++] = 'h';
        u[i++] = 't';
        u[i++] = 'm';
        u[i++] = 'l';
        u[i] = 0;
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == -8, "viva-grande-tope");
        (void)n;
    }
    {
        char u[128];
        char cfg[128];
        int i = 0;
        while (base[i] && i < 110) {
            u[i] = base[i];
            cfg[i] = base[i];
            i++;
        }
        u[i++] = '/';
        u[i++] = 'b';
        u[i++] = 'u';
        u[i++] = 's';
        u[i++] = 'c';
        u[i++] = 'a';
        u[i++] = '?';
        u[i++] = 'q';
        u[i++] = '=';
        u[i++] = 'h';
        u[i++] = 'o';
        u[i++] = 'l';
        u[i++] = 'a';
        u[i] = 0;
        cfg[i - 13] = 0;
        {
            int j = 0;
            const char *suf = "/busca?q=";
            while (cfg[j]) j++;
            {
                int t = 0;
                while (suf[t] && j < 120) cfg[j++] = suf[t++];
            }
            cfg[j] = 0;
        }
        k = u_http_trae(u, &r, cpo, (int)sizeof(cpo), 15000);
        ok(k == 0 && r.codigo == 200, "viva-busca-200");
        if (k == 0) {
            int pr;
            if (u_html_crea(&d)) {
                ok(0, "viva-mem3");
                return;
            }
            pr = u_html_parse(&d, cpo, (int)r.cuerpo_n, u);
            ok(pr >= 0 && d.nenl >= 2, "viva-busca-enlaces");
            if (pr >= 0 && d.nenl >= 1) {
                char u2[U_HTML_URL];
                int t = 0;
                while (d.enl[0].url[t] && t < 120) {
                    u2[t] = d.enl[0].url[t];
                    t++;
                }
                u2[t] = 0;
                k = u_http_trae(u2, &r, cpo, (int)sizeof(cpo),
                    15000);
                ok(k == 0 && r.codigo == 200,
                    "viva-busca-abre-200");
                if (k == 0)
                    ok(r.cuerpo_n > 0, "viva-busca-abre-cuerpo");
            }
            u_html_libera(&d);
        }
        {
            UFILE *f = u_fopen("/fbrowser.cfg", "w");
            if (f) {
                int L = u_strlen(cfg);
                ok(u_fwrite(cfg, L, f) == L, "viva-cfg-escribe");
                u_fclose(f);
            } else {
                ok(0, "viva-cfg-crea");
            }
        }
    }
}

int main(void) {
    red_cfg_t cfg;
    red_if_t rif;
    int i, viva = 0;
    char base[128];
    prueba_minimo();
    prueba_titulos_listas();
    prueba_enlaces_imgs();
    prueba_entidades_utf8();
    prueba_robustos();
    prueba_urls();
    prueba_topes();
    prueba_chunked();
    prueba_cablarga();
    prueba_galletas();
    prueba_redir();
    prueba_img();
    prueba_layout();
    prueba_css();
    prueba_forma();
    prueba_hubbub();
    for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
    if (u_red_interfaz(&rif) == 0 && rif.presente) {
        for (i = 0; i < 16; i++) ((unsigned char *)&cfg)[i] = 0;
        if (u_red_configurar_leer(&cfg) == 0 && cfg.ip) {
            u_itoa(0, base);
            base[0] = 'h';
            base[1] = 't';
            base[2] = 't';
            base[3] = 'p';
            base[4] = ':';
            base[5] = '/';
            base[6] = '/';
            base[7] = '1';
            base[8] = '0';
            base[9] = '.';
            base[10] = '0';
            base[11] = '.';
            base[12] = '2';
            base[13] = '.';
            base[14] = '2';
            base[15] = ':';
            base[16] = '1';
            base[17] = '8';
            base[18] = '0';
            base[19] = '8';
            base[20] = '0';
            base[21] = 0;
            viva = 1;
        }
    }
    if (viva) {
        prueba_viva(base);
    } else {
        u_print("t_fb: sin red (solo unitarias)\n");
    }
    u_print("t_fb: ");
    {
        char nb[12];
        int q = 0;
        u_itoa(pruebas, nb);
        while (nb[q]) {
            char b2[2];
            b2[0] = nb[q];
            b2[1] = 0;
            u_print(b2);
            q++;
        }
    }
    u_print(" pruebas ");
    {
        char nb[12];
        int q = 0;
        u_itoa(fallos, nb);
        while (nb[q]) {
            char b2[2];
            b2[0] = nb[q];
            b2[1] = 0;
            u_print(b2);
            q++;
        }
    }
    u_print(" fallos\n");
    return fallos > 99 ? 99 : fallos;
}
