void __main(void) {
}

#include "ulib.h"
#include "fuente/ttf.h"

/* t_font (v2.8): KAT del rasterizador TrueType propio + DejaVu Sans
   enlazada (objcopy binario). Sin red. Estilo t_ns: cuenta
   pruebas/fallos en QEMU. */

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

extern char fuente_datos[];
extern char fuente_fin[];

static int t_tinta(const unsigned char *a, int n) {
    int i, t = 0;
    for (i = 0; i < n; i++) if (a[i]) t++;
    return t;
}

/* Prueba visual (manual, fuera de FULL): `ejecutar t_font g` dibuja
   texto proporcional + referencia de mapa de bits, presenta, cuenta
   tinta en el dorso y duerme 8 s para el screendump. */
static int modo_g(void) {
    int px = 16, asc, w, x, y, tinta = 0;
    const char *msg = "Hola Ag 123";
    int tam = (int)(fuente_fin - fuente_datos);
    if (ttf_abre((const unsigned char *)fuente_datos, tam)) {
        u_print("t_fontg: sin fuente\n");
        return 1;
    }
    u_texto(0, 0, "", 0);   /* toma propiedad del dorso */
    u_destino(1);
    u_limpiar(0);
    asc = ttf_ascenso(px);
    w = ttf_texto(20, 60, msg, 11, px, 0xFFFFFF);
    u_texto(20, 90, "mapa de bits:", 10);
    u_texto(20, 106, msg, 15);
    /* A aislada grande para comparar (mismaCache, sin vecinos). */
    ttf_texto(20, 200, "A", 1, 32, 0xFFFFFF);
    u_presentar(0, 0, 800, 600);
    /* La captura se hace durante este sueño (imagen limpia: aun no
       se ha impreso nada en consola encima). */
    u_dormir(8000);
    for (y = 60 - asc; y < 60; y++)
        for (x = 20; x < 20 + w; x++)
            if (u_punto(x, y)) tinta++;
    u_print("t_fontg: tinta=");
    u_print_num(tinta);
    u_print(" ancho=");
    u_print_num(w);
    u_print("\n");
    u_print(tinta > 50 && w > 0 ? "t_fontg: ok\n" : "t_fontg: FALLO\n");
    u_destino(0);
    return (tinta > 50 && w > 0) ? 0 : 1;
}

int main(void) {
    static unsigned char alfa[64 * 64];
    int tam, w = 0, h = 0, av = 0, dx = 0, dy = 0;
    {
        /* `ejecutar t_font g` = prueba visual (no la usa FULL). */
        const char *ag = u_args();
        int i = 0, g = 0;
        while (ag[i]) {
            if (ag[i] == 'g' || ag[i] == 'G') g = 1;
            i++;
        }
        if (g) return modo_g();
    }
    u_print("t_font: rasterizador propio + DejaVu...\n");
    tam = (int)(fuente_fin - fuente_datos);
    t_ok(tam > 100000, "fuente-presente");
    t_ok(ttf_abre((const unsigned char *)fuente_datos, tam) == 0,
         "abre-ok");
    t_ok(ttf_abre((const unsigned char *)fuente_datos, 100) != 0,
         "abre-corta-falla");
    t_ok(ttf_abre(0, 0) != 0, "abre-nula-falla");
    /* metricas sanas a 16 px */
    t_ok(ttf_alto_linea(16) >= 14 && ttf_alto_linea(16) <= 24,
         "linea-16");
    t_ok(ttf_ascenso(16) > 8 && ttf_ascenso(16) < 16, "ascenso-16");
    /* cobertura latina + simbolos */
    t_ok(ttf_cubre('A') && ttf_cubre('z'), "cubre-ascii");
    t_ok(ttf_cubre(0x00F1) && ttf_cubre(0x00E9) &&
         ttf_cubre(0x00FC), "cubre-tildes");
    t_ok(ttf_cubre(0x20AC), "cubre-euro");
    t_ok(!ttf_cubre(0x10FFFF), "falta-imposible");
    /* avances coherentes y proporcionales */
    {
        int ai = ttf_avance('i', 16);
        int am = ttf_avance('m', 16);
        int ae = ttf_avance('e', 16);
        t_ok(ai > 0 && am > ai, "avance-proporcional");
        t_ok(ae > 0, "avance-e");
        t_ok(ttf_ancho("Ho", 2, 16) ==
             ttf_avance('H', 16) + ttf_avance('o', 16),
             "ancho-suma");
        t_ok(ttf_ancho("Hola", 4, 16) > ttf_ancho("Ho", 2, 16),
             "ancho-crece");
    }
    /* raster de 'A': tinta presente y encajada */
    {
        int rc = ttf_glifo('A', 16, alfa, (int)sizeof(alfa),
                           &w, &h, &av, &dx, &dy);
        t_ok(rc == 0, "glifo-A-ok");
        t_ok(w > 4 && w < 32 && h > 4 && h < 32, "glifo-A-caja");
        t_ok(t_tinta(alfa, w * h) > 10, "glifo-A-tinta");
        t_ok(av > 0 && av <= w + 8, "glifo-A-avance");
    }
    /* determinismo de cache: dos veces, mismo mapa */
    {
        static unsigned char b2[64 * 64];
        int w2 = 0, h2 = 0, i, ok = 1;
        int rc = ttf_glifo('A', 16, b2, (int)sizeof(b2),
                           &w2, &h2, 0, 0, 0);
        t_ok(rc == 0 && w2 == w && h2 == h, "glifo-cache-caja");
        for (i = 0; i < w * h; i++)
            if (alfa[i] != b2[i]) {
                ok = 0;
                break;
            }
        t_ok(ok, "glifo-cache-igual");
    }
    /* compuestas/acentuadas traen tinta (ejercita composites) */
    {
        int n = 0, k;
        static const unsigned cps[] = {0x00E9, 0x00F1, 0x00FC,
                                       0x00C4, 0x00D1};
        for (k = 0; k < 5; k++) {
            int ww = 0, hh = 0;
            static unsigned char b[64 * 64];
            if (ttf_glifo(cps[k], 16, b, (int)sizeof(b),
                          &ww, &hh, 0, 0, 0) == 0 &&
                t_tinta(b, ww * hh) > 4)
                n++;
        }
        t_ok(n >= 4, "glifo-acentos-tinta");
    }
    /* espacio: sin tinta pero con avance */
    {
        int ww = -1, hh = -1;
        static unsigned char b[64 * 64];
        int rc = ttf_glifo(' ', 16, b, (int)sizeof(b),
                           &ww, &hh, 0, 0, 0);
        t_ok(rc != 0 && ttf_avance(' ', 16) > 0, "espacio-vacio");
    }
    /* texto completo: "Hola" pinta y avanza igual que la suma */
    {
        int a = ttf_ancho("Hola", 4, 16);
        int wpx = ttf_texto(0, 100, "Hola", 4, 16, 0xFFFFFF);
        t_ok(a > 0 && wpx == a, "texto-ancho");
    }
    /* UTF-8 */
    {
        unsigned cp = 0;
        int n = ttf_utf8("\xc3\xb1x", 3, &cp);
        t_ok(n == 2 && cp == 0x00F1, "utf8-2b");
        n = ttf_utf8("A", 1, &cp);
        t_ok(n == 1 && cp == 'A', "utf8-1b");
        n = ttf_utf8("\xff", 1, &cp);
        t_ok(n == 1 && cp == 0xFFFD, "utf8-invalido");
    }
    {
        char b[64];
        int i = 0, t = 0;
        const char *s = "t_font: ";
        while (s[i]) b[t++] = s[i++];
        u_itoa(t_pruebas, b + t);
        while (b[t]) t++;
        b[t++] = ' ';
        b[t++] = 'p';
        b[t++] = 'r';
        b[t++] = 'u';
        b[t++] = 'e';
        b[t++] = 'b';
        b[t++] = 'a';
        b[t++] = 's';
        b[t++] = ' ';
        u_itoa(t_fallos, b + t);
        while (b[t]) t++;
        b[t++] = ' ';
        b[t++] = 'f';
        b[t++] = 'a';
        b[t++] = 'l';
        b[t++] = 'l';
        b[t++] = 'o';
        b[t++] = 's';
        b[t++] = '\n';
        b[t] = 0;
        u_print(b);
    }
    return t_fallos > 99 ? 99 : t_fallos;
}
