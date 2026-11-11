void __main(void) {
}

#include "ulib.h"
#include "nsfetch.h"

/* t_fetch (Hito 2 NetSurf): KAT del shim fetch sobre los stubs
   locales (http_stub 18080 + tls_stub 18443). Sin internet. */

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

/* 1 si aguja aparece en pajar (n bytes), 0 si no. */
static int contiene(const char *pajar, int n, const char *aguja) {
    int m = 0, i;
    while (aguja[m]) m++;
    if (!m || m > n) return 0;
    for (i = 0; i + m <= n; i++) {
        if (!u_memcmp(pajar + i, aguja, m)) return 1;
    }
    return 0;
}

static void prueba_http(void) {
    nsfetch_res_t r;
    u_print("t_fetch: http...\n");
    t_ok(nsfetch_get("http://10.0.2.2:18080/normal", &r) == 0 &&
         r.codigo == 200 && r.tam == 13 &&
         !u_memcmp(r.cuerpo, "Hello, world!", 13), "http-ok");
    t_ok(nsfetch_get("http://10.0.2.2:18080/pagina", &r) == 0 &&
         r.codigo == 200 && r.tam > 0, "http-pagina");
    t_ok(nsfetch_get("http://10.0.2.2:18080/noencontrado", &r) == 0 &&
         r.codigo == 404, "http-404");
}

static void prueba_redir(void) {
    nsfetch_res_t r;
    u_print("t_fetch: redir...\n");
    t_ok(nsfetch_get("http://10.0.2.2:18080/redir302", &r) == 0 &&
         r.codigo == 200 && r.tam == 13 &&
         !u_memcmp(r.cuerpo, "Hello, world!", 13), "redir-sigue");
}

static void prueba_https(void) {
    nsfetch_res_t r;
    u_print("t_fetch: https...\n");
    t_ok(nsfetch_get("https://10.0.2.2:18443/pagina", &r) == 0 &&
         r.codigo == 200 && r.tam > 0 &&
         contiene(r.cuerpo, r.tam, "TLS ok"), "https-ok");
    t_ok(r.suite != 0, "https-suite");
    t_ok(nsfetch_get("https://10.0.2.2:18443/hola", &r) == 0 &&
         r.codigo == 200 && contiene(r.cuerpo, r.tam, "eco tls:"),
         "https-eco");
}

static void prueba_errores(void) {
    nsfetch_res_t r;
    u_print("t_fetch: errores...\n");
    t_ok(nsfetch_get("http://10.0.2.2:9/x", &r) == NSFETCH_E_RED,
         "transporte-cerrado");
    t_ok(nsfetch_get("://mal", &r) == NSFETCH_E_URL, "url-mala");
    t_ok(nsfetch_get("ftp://x/", &r) == NSFETCH_E_URL, "url-esquema");
}

int main(void) {
    u_print("t_fetch: bateria fetch NetSurf (Hito 2)...\n");
    prueba_http();
    prueba_redir();
    prueba_https();
    prueba_errores();
    {
        char nb[12];
        int q = 0;
        u_print("t_fetch: ");
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
