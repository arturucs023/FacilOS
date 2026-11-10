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
    ok(d.hay_forma == 0, "forma-post-no");
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
    prueba_layout();
    prueba_css();
    prueba_forma();
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
