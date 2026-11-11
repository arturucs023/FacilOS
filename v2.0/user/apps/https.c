#include "ulib.h"
#include "nsfetch.h"

void __main(void) {
}

/* https (Hito 2: cliente HTTPS/1.0 Ring3 sobre nsfetch + BearSSL).
   Movido el motor a nsfetch.c (sin duplicar); este programa es la
   interfaz de linea: parsea args, llama y presenta el resultado.
   Cambios menores de mensajes respecto al Hito 1 (ver NOTAS-NS-HITO1):
   los fallos de transporte se consolidan en "fallo de red/TLS". */

static void pon_txt(const char *s) {
    u_print(s);
}

static void pon_num(unsigned int v) {
    char nb[12];
    int k = 0;
    u_utoa(v, nb, 10);
    while (nb[k]) {
        char b[2];
        b[0] = nb[k++];
        b[1] = 0;
        u_print(b);
    }
}

int main(void) {
    const char *a = u_args();
    char url[U_HTTP_MAX_URL + 1];
    u_http_url_t u;
    nsfetch_res_t r;
    int nd = 0, i, rc;
    while (*a == ' ') a++;
    while (*a && *a != ' ' && nd < U_HTTP_MAX_URL) url[nd++] = *a++;
    url[nd] = 0;
    while (*a == ' ') a++;
    if (!nd || *a) {
        u_print("Uso: https https://dominio[:puerto][/ruta]\n");
        return 2;
    }
    if (u_http_url(url, &u) || !u.esquema) {
        u_print("https: URL invalida (solo https://dominio[:puerto][/ruta])\n");
        return 2;
    }
    rc = nsfetch_get(url, &r);
    if (rc == NSFETCH_E_URL) {
        u_print("https: URL invalida (solo https://dominio[:puerto][/ruta])\n");
        return 2;
    }
    if (rc == NSFETCH_E_DNS) {
        u_print("https: DNS fallo\n");
        return 4;
    }
    if (rc == NSFETCH_E_CUERPO) {
        u_print("https: cuerpo demasiado grande\n");
        return 4;
    }
    if (rc == NSFETCH_E_REDIR) {
        u_print("https: demasiadas redirecciones\n");
        return 1;
    }
    if (rc != NSFETCH_OK) {
        pon_txt("https: fallo de red/TLS");
        if (r.detalle) {
            pon_txt(" (");
            pon_num((unsigned int)(r.detalle < 0 ? -r.detalle
                                                : r.detalle));
            pon_txt(")");
        }
        pon_txt("\n");
        return 1;
    }
    {
        char h[12];
        int j;
        pon_txt("https: TLS ok suite 0x");
        u_utoa((unsigned int)r.suite, h, 16);
        for (j = 0; h[j]; j++) {
            char b[2];
            b[0] = h[j];
            b[1] = 0;
            pon_txt(b);
        }
        pon_txt(" bytes ");
        pon_num((unsigned int)(r.tam < 0 ? 0 : r.tam));
        pon_txt("\n");
    }
    if (r.tam > 0) {
        int txt = 1;
        for (i = 0; i < r.tam; i++) {
            unsigned char c = (unsigned char)r.cuerpo[i];
            if (c == 9 || c == 10 || c == 13) continue;
            if (c < 32 || c > 126) {
                txt = 0;
                break;
            }
        }
        if (txt) {
            int k;
            for (k = 0; k < r.tam; k++) {
                char b[2];
                b[0] = r.cuerpo[k];
                b[1] = 0;
                pon_txt(b);
            }
            if (r.cuerpo[r.tam - 1] != '\n') pon_txt("\n");
        } else {
            pon_txt("(binario)\n");
        }
    }
    return 0;
}
