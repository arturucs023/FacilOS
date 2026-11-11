#include "ulib.h"
#include "ugfx.h"
#include "utext.h"

void __main(void) {
}

/* FacilBrowser v1 (F7-C): navegador grafico Ring 3. Una ventana
   UGFX; parser/layout en ulib (u_html_*); red con u_http_trae
   tal cual (GET + Connection: close, 1 conexion por pagina).
   Sin JS/CSS; img solo estructural; sin HTTPS. */

#define FB_W 640
#define FB_H 460
#define FB_BAR_H 44
#define FB_EST_H 32
#define FB_HIST 16
#define FB_ESPERA 30000u

static ug_win_t v;
static char bar[U_HTML_URL + 1];
static int bpos;
static char url[U_HTML_URL + 1];
static char hist[FB_HIST][U_HTML_URL + 1];
static int hscroll[FB_HIST];
static int nhist, ihist;
static u_html_doc_t doc;
static int tiene_doc;
static u_html_fila_t *filas;
static int nfilas, filas_cols;
static int desplaza, vis;
static int foco_barra, enl_foco, b_antes;
static char estado[160];
static int codigo, tam_cpo, t_ms;
static unsigned int chk_cpo;
static unsigned char cuerpo[U_HTTP_MAX_CPO + 1];
static char base_busca[U_HTML_URL + 1];
static int detener;
static int nfetches;
static int tiene_red;

static unsigned int ahora(void) {
    int t = u_ticks();
    return t < 0 ? 0u : (unsigned int)t;
}

static void pon_estado(const char *s) {
    int i = 0;
    while (s[i] && i < 158) {
        estado[i] = s[i];
        i++;
    }
    estado[i] = 0;
}

/* Codifica consulta para ?q=: espacio -> '+', resto %XX. */
static int codifica_q(const char *s, char *d, int cap) {
    int t = 0, i = 0;
    static const char *hex = "0123456789ABCDEF";
    while (s[i] && t < cap - 1) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            d[t++] = (char)c;
            i++;
        } else if (c == ' ') {
            d[t++] = '+';
            i++;
        } else {
            if (t + 3 >= cap) break;
            d[t++] = '%';
            d[t++] = hex[(c >> 4) & 15];
            d[t++] = hex[c & 15];
            i++;
        }
    }
    d[t] = 0;
    return t;
}

/* 1 si la entrada es busqueda (sin esquema y sin punto). */
static int es_busqueda(const char *s) {
    int i = 0, punto = 0, dos = 0;
    if (!s[0]) return 0;
    while (s[i]) {
        if (s[i] == ' ') return 1;
        if (s[i] == '.') punto = 1;
        if (s[i] == ':' && s[i + 1] == '/' && s[i + 2] == '/')
            dos = 1;
        i++;
    }
    if (dos) return 0;
    if (!punto) {
        i = 0;
        while (s[i]) {
            if (s[i] == '/' || s[i] == '?' || s[i] == '#')
                return 0;
            i++;
        }
        return 1;
    }
    return 0;
}

/* Empuja url al historial (recorta futuro, tope 16). */
static void hist_empuja(const char *u) {
    int i;
    if (u[0] && nhist > 0 && ihist >= 0 && ihist < nhist &&
        !u_strcmp(hist[ihist], u))
        return;
    if (ihist >= 0 && ihist < nhist)
        hscroll[ihist] = desplaza;
    if (ihist < nhist - 1)
        nhist = ihist + 1;
    if (nhist >= FB_HIST) {
        for (i = 1; i < FB_HIST; i++) {
            u_strcpy(hist[i - 1], hist[i]);
            hscroll[i - 1] = hscroll[i];
        }
        nhist = FB_HIST - 1;
    }
    u_strcpy(hist[nhist], u);
    hscroll[nhist] = 0;
    nhist++;
    ihist = nhist - 1;
}

/* Carga y muestra entrada (URL o busqueda). empujar=1 la guarda.
   Redirecciones 301/302 http, maximo 3 saltos, validadas. */
static void navegar(const char *entrada, int empujar) {
    char u[U_HTML_URL + 1];
    char loc[U_HTTP_VAL];
    u_http_res_t r;
    int k, i, saltos, nbody, redir = 0;
    unsigned int t0;
    if (detener) {
        detener = 0;
        pon_estado("detenido");
        return;
    }
    {
        int j = 0;
        while (entrada[j] == ' ') j++;
        if (!u_strncmp(entrada + j, "https://", 8)) {
            pon_estado("requiere HTTPS (no soportado)");
            return;
        }
        if (!u_strncmp(entrada + j, "HTTPS://", 8)) {
            pon_estado("requiere HTTPS (no soportado)");
            return;
        }
        if (!u_strncmp(entrada + j, "javascript:", 11) ||
            !u_strncmp(entrada + j, "mailto:", 7) ||
            !u_strncmp(entrada + j, "ftp://", 6) ||
            !u_strncmp(entrada + j, "data:", 5)) {
            pon_estado("esquema no soportado (solo http)");
            return;
        }
    }
    if (es_busqueda(entrada)) {
        char q[U_HTML_URL + 1];
        /* v2.4: si la pagina trae formulario GET, se usa como buscador
           (p. ej. SearXNG propio); si no, el de /fbrowser.cfg. */
        if (tiene_doc && doc.hay_forma && doc.forma[0] && doc.campo[0]) {
            codifica_q(entrada, q, (int)sizeof(q));
            i = 0;
            while (doc.forma[i] && i < U_HTML_URL) {
                u[i] = doc.forma[i];
                i++;
            }
            {
                int j = 0;
                char sep = '?';
                {
                    int k = 0;
                    while (doc.forma[k]) {
                        if (doc.forma[k] == '?') sep = '&';
                        k++;
                    }
                }
                u[i++] = sep;
                while (doc.campo[j] && i < U_HTML_URL) u[i++] = doc.campo[j++];
                u[i++] = '=';
                j = 0;
                while (q[j] && i < U_HTML_URL) u[i++] = q[j++];
            }
            u[i] = 0;
            if (i >= U_HTML_URL) {
                pon_estado("busqueda demasiado larga");
                return;
            }
        } else if (!base_busca[0]) {
            pon_estado("sin buscador (crea /fbrowser.cfg)");
            return;
        } else {
            codifica_q(entrada, q, (int)sizeof(q));
            i = 0;
            while (base_busca[i] && i < U_HTML_URL) {
                u[i] = base_busca[i];
                i++;
            }
            {
                int j = 0;
                while (q[j] && i < U_HTML_URL) u[i++] = q[j++];
            }
            u[i] = 0;
            if (i >= U_HTML_URL) {
                pon_estado("busqueda demasiado larga");
                return;
            }
        }
    } else if (!u_strchr(entrada, ':')) {
        const char *p = entrada;
        int t = 0;
        while (*p == ' ') p++;
        u_strcpy(u, "http://");
        t = 7;
        while (*p && t < U_HTML_URL) u[t++] = *p++;
        while (t > 7 && u[t - 1] == ' ') t--;
        u[t] = 0;
    } else {
        int t = 0;
        while (entrada[t] && t < U_HTML_URL) {
            u[t] = entrada[t];
            t++;
        }
        u[t] = 0;
    }
    saltos = 0;
    for (;;) {
        t0 = ahora();
        k = u_http_trae(u, &r, cuerpo, U_HTTP_MAX_CPO, FB_ESPERA);
        t_ms = (int)((ahora() - t0) * 10u);
        nfetches++;
        codigo = r.codigo;
        tam_cpo = (int)r.cuerpo_n;
        {
            unsigned int s = 0;
            int j;
            for (j = 0; j < tam_cpo; j++) s += cuerpo[j];
            chk_cpo = s;
        }
        nbody = tam_cpo;
        (void)nbody;
        if (k == -2) {
            pon_estado("URL invalida (solo http://)");
            return;
        }
        if (k == -3) {
            pon_estado("no existe (NXDOMAIN)");
            return;
        }
        if (k == -5) {
            pon_estado("sin respuesta (revisa la red)");
            return;
        }
        if (k == -4) {
            pon_estado("error DNS (sin respuesta valida)");
            return;
        }
        if (k == -6) {
            pon_estado("respuesta malformada");
            return;
        }
        if (k == -7) {
            pon_estado("transferencia no soportada");
            return;
        }
        if (k == -8) {
            pon_estado("cuerpo truncado o demasiado grande");
            return;
        }
        if (k) {
            pon_estado("sin respuesta (revisa la red)");
            return;
        }
        if ((codigo == 301 || codigo == 302) && saltos < 3 &&
            !detener) {
            if (u_http_cabecera(&r, "Location", loc,
                    (int)sizeof(loc)) >= 0 && loc[0]) {
                char abs[U_HTML_URL];
                if (u_html_resuelve_url(u, loc, abs) == 0) {
                    u_strcpy(u, abs);
                    saltos++;
                    redir = 1;
                    continue;
                }
                pon_estado("redireccion no HTTP (sin seguir)");
                return;
            }
        }
        break;
    }
    if (detener) {
        detener = 0;
        pon_estado("detenido (se descarta lo traido)");
        return;
    }
    {
        u_html_doc_t nd;
        int pr;
        nd.texto = 0;
        nd.seg = 0;
        nd.bloq = 0;
        nd.enl = 0;
        nd.img = 0;
        if (u_html_crea(&nd)) {
            pon_estado("sin memoria para el documento");
            return;
        }
        pr = 0;
#ifdef F32_NSHUBBUB
        /* v2.4.3+hubbub: tokenizador HTML5 real; si falla (-2),
           se reintenta con el legado (misma salida esperada). */
        pr = u_html_parse_hb(&nd, cuerpo, tam_cpo, u);
#endif
        if (pr < 0)
            pr = u_html_parse(&nd, cuerpo, tam_cpo, u);
        if (pr < 0) {
            u_html_libera(&nd);
            pon_estado("documento ilegible");
            return;
        }
        u_html_libera(&doc);
        doc = nd;
        tiene_doc = 1;
        filas_cols = 0;
    }
    u_strcpy(url, u);
    u_strcpy(bar, u);
    bpos = u_strlen(bar);
    if (empujar)
        hist_empuja(u);
    desplaza = 0;
    enl_foco = -1;
    {
        /* Traza para el harness de navegacion (serial QEMU):
           permite esperar la pagina sin sleeps a ciegas. */
        char tg[U_HTML_URL + 9];
        int q = 0;
        const char *pre = "fbrowser: ";
        while (pre[q] && q < 500) {
            tg[q] = pre[q];
            q++;
        }
        {
            int j = 0;
            while (u[j] && q < 500) tg[q++] = u[j++];
        }
        tg[q++] = '\n';
        tg[q] = 0;
        u_print(tg);
    }
    if (redir)
        pon_estado("listo (redirigido)");
    else if (codigo >= 400)
        pon_estado("listo (el servidor devolvio error)");
    else
        pon_estado("listo");
}

static void ir_atras(void) {
    if (ihist > 0) {
        hscroll[ihist] = desplaza;
        ihist--;
        desplaza = hscroll[ihist];
        navegar(hist[ihist], 0);
        desplaza = hscroll[ihist];
    } else {
        pon_estado("no hay anterior");
    }
}

static void ir_adelante(void) {
    if (ihist < nhist - 1) {
        hscroll[ihist] = desplaza;
        ihist++;
        desplaza = hscroll[ihist];
        navegar(hist[ihist], 0);
        desplaza = hscroll[ihist];
    } else {
        pon_estado("no hay siguiente");
    }
}

/* Primera fila con el enlace e (para enfocar con teclado). */
static int fila_de_enlace(int e) {
    int i;
    for (i = 0; i < nfilas; i++) {
        if (filas[i].enlace == e) return i;
    }
    return -1;
}

static void asegura_visible(int f) {
    if (f < desplaza)
        desplaza = f;
    else if (f > desplaza + vis - 1)
        desplaza = f - vis + 1;
    if (desplaza < 0) desplaza = 0;
}

static void dibuja(void) {
    int w = v.w, h = v.h, i;
    int cw, ch, cols;
    ug_comienza(&v);
    ug_limpia(&v, T_VENT_FONDO);
    ug_relleno(&v, 0, 0, w, FB_BAR_H, T_PANEL);
    ug_linea(&v, 0, FB_BAR_H - 1, w - 1, FB_BAR_H - 1,
        T_PANEL_BORDE);
    ug_boton(&v, 8, 8, 48, 28, "<",
        (nhist > 0 && ihist > 0) ? UG_BOTON_NORMAL : UG_BOTON_OFF);
    ug_boton(&v, 60, 8, 48, 28, ">",
        (ihist < nhist - 1) ? UG_BOTON_NORMAL : UG_BOTON_OFF);
    ug_boton(&v, 112, 8, 48, 28, "R", UG_BOTON_NORMAL);
    ug_boton(&v, 164, 8, 48, 28, "X", UG_BOTON_NORMAL);
    ug_campo(&v, 220, 12, w - 220 - 60, bar, bpos, foco_barra);
    ug_boton(&v, w - 52, 8, 44, 28, "Ir", UG_BOTON_NORMAL);
    cw = w - 14;
    if (cw < 80) cw = 80;
    cols = cw / UG_FW;
    ch = h - FB_BAR_H - FB_EST_H;
    if (ch < UG_FH) ch = UG_FH;
    vis = ch / UG_FH;
    if (vis < 1) vis = 1;
    if (tiene_doc && (filas_cols != cols)) {
        nfilas = u_html_maqueta(&doc, cols, filas, U_HTML_MAX_FILA);
        if (nfilas < 0) nfilas = 0;
        filas_cols = cols;
        if (desplaza > nfilas - vis) desplaza = nfilas - vis;
        if (desplaza < 0) desplaza = 0;
    }
    if (!tiene_doc) {
        ug_texto(&v, 24, FB_BAR_H + 40, "FacilBrowser", C_AMARILLO);
        ug_texto(&v, 24, FB_BAR_H + 64,
            estado[0] ? estado : "Escribe una direccion y pulsa Intro.",
            T_PANEL_TEXTO);
        ug_texto(&v, 24, FB_BAR_H + 88,
            "Ej: http://10.0.2.2:18080/pagina", C_GRIS);
        ug_texto(&v, 24, FB_BAR_H + 112,
            "Tab=enlaces Barra, Intro=ir, Retroceso=atras, r=recargar",
            C_GRIS);
    } else {
        int maxd = nfilas - vis;
        char tmp[512];
        if (maxd < 0) maxd = 0;
        if (desplaza > maxd) desplaza = maxd;
        if (desplaza < 0) desplaza = 0;
        for (i = 0; i < vis; i++) {
            int f = desplaza + i, y = FB_BAR_H + i * UG_FH, q;
            int x, color;
            if (f >= nfilas) break;
            x = 4 + filas[f].x * UG_FW;
            if (filas[f].estilo == U_HTML_E_VACIA)
                continue;
            if (filas[f].estilo == U_HTML_E_HR) {
                ug_linea(&v, 4, y + 8, cw - 4, y + 8, C_GRIS);
                continue;
            }
            if (filas[f].estilo == U_HTML_E_IMG) {
                int gi = filas[f].img;
                ug_rect(&v, x, y + 2, 34, 12, T_ACENTO);
                ug_linea(&v, x, y + 2, x + 34, y + 14, T_ACENTO);
                ug_linea(&v, x + 34, y + 2, x, y + 14, T_ACENTO);
                if (gi >= 0 && gi < doc.nimg && doc.img[gi].alt[0]) {
                    q = 0;
                    while (doc.img[gi].alt[q] && q < 60) {
                        tmp[q] = doc.img[gi].alt[q];
                        q++;
                    }
                    tmp[q] = 0;
                } else {
                    tmp[0] = 'i';
                    tmp[1] = 'm';
                    tmp[2] = 'g';
                    tmp[3] = 0;
                }
                ug_texto(&v, x + 38, y, tmp, C_GRIS);
                continue;
            }
            q = 0;
            while (q < filas[f].len && q < 500) {
                tmp[q] = doc.texto[filas[f].ini + q];
                q++;
            }
            tmp[q] = 0;
            /* v2.4 CSS: el color del tramo manda; si no, el estilo. */
            if (filas[f].fg >= 0)
                color = 0x01000000 | (filas[f].fg & 0xFFFFFF);
            else if (filas[f].estilo == U_HTML_E_TITULO)
                color = C_AMARILLO;
            else if (filas[f].estilo == U_HTML_E_NEGRITA)
                color = C_BLANCO;
            else if (filas[f].estilo == U_HTML_E_ENFASIS)
                color = C_CYAN_CLARO;
            else if (filas[f].estilo == U_HTML_E_ENLACE)
                color = C_AZUL_CLARO;
            else
                color = C_GRIS;
            if (filas[f].enlace >= 0 &&
                filas[f].enlace == enl_foco) {
                int wpx = ut_largo(tmp) * UG_FW;
                ug_relleno(&v, x, y, wpx, UG_FH, T_SELECCION);
                color = C_BLANCO;
            } else if (filas[f].bg >= 0) {
                int wpx = ut_largo(tmp) * UG_FW;
                ug_relleno(&v, x, y, wpx, UG_FH,
                    0x01000000 | (filas[f].bg & 0xFFFFFF));
            }
            ug_texto(&v, x, y, tmp, color);
            if (filas[f].estilo == U_HTML_E_ENLACE) {
                int wpx = ut_largo(tmp) * UG_FW;
                ug_linea(&v, x, y + 14, x + wpx - 1, y + 14,
                    color);
            }
        }
        if (nfilas > vis) {
            int th = vis * UG_FH;
            int tam = th * vis / nfilas;
            int pos = th * desplaza / nfilas;
            if (tam < 8) tam = 8;
            if (pos + tam > th) pos = th - tam;
            ug_relleno(&v, w - 12, FB_BAR_H, 12, th, T_PANEL);
            ug_relleno(&v, w - 10, FB_BAR_H + pos, 8, tam,
                T_ACENTO);
        }
    }
    {
        char l1[160], l2[200];
        int t = 0, j = 0;
        while (estado[t] && t < 90) {
            l1[t] = estado[t];
            t++;
        }
        l1[t] = 0;
        ug_texto(&v, 4, h - FB_EST_H, l1, C_GRIS);
        if (tiene_doc) {
            char nb[12];
            l2[0] = 0;
            t = 0;
            while (url[t] && t < 60) {
                l2[t] = url[t];
                t++;
            }
            l2[t++] = ' ';
            l2[t++] = '|';
            l2[t++] = ' ';
            u_itoa(codigo, nb);
            j = 0;
            while (nb[j] && t < 120) l2[t++] = nb[j++];
            l2[t++] = ' ';
            u_itoa(tam_cpo, nb);
            j = 0;
            while (nb[j] && t < 140) l2[t++] = nb[j++];
            l2[t++] = 'B';
            l2[t++] = ' ';
            u_itoa(t_ms, nb);
            j = 0;
            while (nb[j] && t < 160) l2[t++] = nb[j++];
            l2[t++] = 'm';
            l2[t++] = 's';
            l2[t] = 0;
            if (doc.titulo[0]) {
                ug_texto(&v, 4, h - FB_EST_H + 16, doc.titulo,
                    C_AMARILLO);
            } else {
                ug_texto(&v, 4, h - FB_EST_H + 16, l2, C_GRIS);
            }
        } else {
            ug_texto(&v, 4, h - FB_EST_H + 16,
                "http:// + Intro | Tab=enlaces | Retroceso=atras",
                C_GRIS);
        }
    }
    ug_fin(&v);
}

static void ev_tecla(int tecla) {
    if (foco_barra) {
        if (tecla == 13) {
            navegar(bar, 1);
            foco_barra = 0;
        } else if (tecla == 27) {
            u_strcpy(bar, url);
            bpos = u_strlen(bar);
            foco_barra = 0;
        } else if (tecla == 9) {
            foco_barra = 0;
            enl_foco = (doc.nenl > 0 && tiene_doc) ? 0 : -1;
            if (enl_foco >= 0) {
                int f = fila_de_enlace(enl_foco);
                if (f >= 0) asegura_visible(f);
            }
        } else {
            ug_campo_tecla(bar, (int)sizeof(bar), &bpos, tecla);
        }
        return;
    }
    if (tecla == 0x101) {
        desplaza--;
        if (desplaza < 0) desplaza = 0;
    } else if (tecla == 0x102) {
        desplaza++;
    } else if (tecla == 0x109) {
        desplaza -= vis;
        if (desplaza < 0) desplaza = 0;
    } else if (tecla == 0x10A) {
        desplaza += vis;
    } else if (tecla == 0x107) {
        desplaza = 0;
    } else if (tecla == 0x108) {
        desplaza = nfilas - vis;
        if (desplaza < 0) desplaza = 0;
    } else if (tecla == 8) {
        ir_atras();
    } else if (tecla == 'r' || tecla == 'R') {
        if (url[0]) {
            hscroll[ihist] = desplaza;
            navegar(url, 0);
        }
    } else if (tecla == 9) {
        if (tiene_doc && enl_foco + 1 < doc.nenl) {
            enl_foco++;
            {
                int f = fila_de_enlace(enl_foco);
                if (f >= 0) asegura_visible(f);
            }
        } else {
            enl_foco = -1;
            foco_barra = 1;
        }
    } else if (tecla == 13) {
        if (tiene_doc && enl_foco >= 0 && enl_foco < doc.nenl)
            navegar(doc.enl[enl_foco].url, 1);
        else if (!tiene_doc)
            foco_barra = 1;
    } else if (tecla == 27) {
        foco_barra = 1;
    }
}

static void ev_raton(int lx, int ly, int bot, int rueda) {
    int clic = ug_clic(b_antes, bot);
    b_antes = bot;
    if (rueda) {
        desplaza -= rueda * 2;
        if (desplaza < 0) desplaza = 0;
        return;
    }
    if (!clic) return;
    if (ly >= 8 && ly < 36) {
        if (lx >= 8 && lx < 56) {
            ir_atras();
            return;
        }
        if (lx >= 60 && lx < 108) {
            ir_adelante();
            return;
        }
        if (lx >= 112 && lx < 160) {
            if (url[0]) {
                hscroll[ihist] = desplaza;
                navegar(url, 0);
            }
            return;
        }
        if (lx >= 164 && lx < 212) {
            detener = 1;
            if (url[0] || bar[0])
                navegar(url[0] ? url : bar, 0);
            else
                pon_estado("nada que detener");
            detener = 0;
            return;
        }
        if (lx >= v.w - 52 && lx < v.w - 8) {
            navegar(bar, 1);
            foco_barra = 0;
            return;
        }
        if (lx >= 220 && lx < v.w - 60) {
            foco_barra = 1;
            return;
        }
        foco_barra = 0;
        return;
    }
    if (ly >= FB_BAR_H && ly < v.h - FB_EST_H) {
        if (lx >= v.w - 12) {
            int th = vis * UG_FH;
            int rel = ly - FB_BAR_H;
            if (nfilas > vis) {
                int tam = th * vis / nfilas;
                int pos = th * desplaza / nfilas;
                if (rel < pos)
                    desplaza -= vis;
                else if (rel > pos + tam)
                    desplaza += vis;
                if (desplaza < 0) desplaza = 0;
            }
            foco_barra = 0;
            return;
        }
        if (tiene_doc) {
            int f = desplaza + (ly - FB_BAR_H) / UG_FH;
            foco_barra = 0;
            if (f >= 0 && f < nfilas && filas[f].enlace >= 0) {
                enl_foco = filas[f].enlace;
                navegar(doc.enl[enl_foco].url, 1);
            }
            return;
        }
        foco_barra = 1;
        return;
    }
    foco_barra = 0;
}

static void lee_busca(void) {
    UFILE *f = u_fopen("/fbrowser.cfg", "r");
    base_busca[0] = 0;
    if (f) {
        int i = 0, c;
        char b[2];
        while (i < U_HTML_URL && u_fread(b, 1, f) == 1) {
            c = b[0];
            if (c == '\r' || c == '\n') break;
            base_busca[i++] = (char)c;
        }
        base_busca[i] = 0;
        u_fclose(f);
    }
}

int main(void) {
    const char *a = u_args();
    int srv = 0;
    ug_ev_t ev;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    while (*a >= '0' && *a <= '9') a++;
    while (*a == ' ') a++;
    if (srv <= 0) {
        u_print("Uso: fbrowser <srv> [url]\n");
        return 99;
    }
    filas = (u_html_fila_t *)u_malloc(
        U_HTML_MAX_FILA * (int)sizeof(u_html_fila_t));
    if (!filas) {
        u_print("fbrowser: sin memoria\n");
        return 1;
    }
    doc.texto = 0;
    doc.seg = 0;
    doc.bloq = 0;
    doc.enl = 0;
    doc.img = 0;
    tiene_doc = 0;
    if (u_html_crea(&doc)) {
        u_free(filas);
        u_print("fbrowser: sin memoria\n");
        return 1;
    }
    u_html_libera(&doc);
    tiene_doc = 0;
    if (ug_abre(&v, srv, FB_W, FB_H, "FacilBrowser")) {
        u_free(filas);
        return 98;
    }
    ug_edit(&v, 1);
    lee_busca();
    {
        red_if_t rif;
        int i;
        for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
        tiene_red = (u_red_interfaz(&rif) == 0 && rif.presente);
    }
    bar[0] = 0;
    bpos = 0;
    url[0] = 0;
    estado[0] = 0;
    nhist = 0;
    ihist = -1;
    desplaza = 0;
    vis = 20;
    nfilas = 0;
    filas_cols = 0;
    foco_barra = 1;
    enl_foco = -1;
    b_antes = 0;
    codigo = 0;
    tam_cpo = 0;
    t_ms = 0;
    chk_cpo = 0;
    detener = 0;
    nfetches = 0;
    if (!tiene_red)
        pon_estado("sin adaptador (configura la red primero)");
    if (*a) {
        char ini[U_HTML_URL + 1];
        int i = 0;
        while (a[i] && i < U_HTML_URL) {
            ini[i] = a[i];
            i++;
        }
        ini[i] = 0;
        u_strcpy(bar, ini);
        bpos = i;
        foco_barra = 0;
        if (tiene_red)
            navegar(ini, 1);
        else
            pon_estado("sin adaptador (configura la red primero)");
    }
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        if (t == UG_EV_SALIR) break;
        if (t == UG_EV_TECLA) {
            ev_tecla(ev.tecla);
        } else if (t == UG_EV_RATON) {
            ev_raton(ev.rx - v.x, ev.ry - (v.y + UG_CHROME), ev.rbot,
                ev.rueda);
        } else if (t == UG_EV_DRAW) {
            dibuja();
        }
    }
    u_html_libera(&doc);
    u_free(filas);
    ug_cierra(&v);
    return 0;
}
