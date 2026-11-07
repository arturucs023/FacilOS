#include "ulib.h"

void __main(void) {
}

/* v2.3-red F7-B: cliente HTTP/1.0 (Ring3). Resolucion y TCP
   reutilizados tal cual (u_dns_resuelve3 + u_tcp_*); parser
   incremental en ulib. Solo GET + Connection: close. */

static unsigned char hcuerpo[U_HTTP_MAX_CPO + 1];

static unsigned int ahora(void) {
    int t = u_ticks();
    return t < 0 ? 0u : (unsigned int)t;
}

static void pon_txt(const char *s) {
    u_print(s);
}

static void pon_num(unsigned int v) {
    char nb[12];
    int k = 0;
    u_utoa(v, nb, 10);
    while (nb[k]) {
        char b[2];
        b[0] = nb[k];
        b[1] = 0;
        u_print(b);
        k++;
    }
}

static void pon_ip(unsigned int v) {
    char b[20];
    int t = 0, o;
    for (o = 0; o < 4; o++) {
        char nb[12];
        int k = 0, q = 0;
        unsigned int a = (v >> (o * 8)) & 0xFFu;
        if (a >= 100) nb[q++] = (char)('0' + a / 100);
        if (a >= 10) nb[q++] = (char)('0' + (a / 10) % 10);
        nb[q++] = (char)('0' + a % 10);
        nb[q] = 0;
        while (nb[k] && t < 18) b[t++] = nb[k++];
        if (o < 3) b[t++] = '.';
    }
    b[t] = 0;
    u_print(b);
}

static int es_texto(const unsigned char *b, int n) {
    int i;
    for (i = 0; i < n; i++) {
        unsigned char c = b[i];
        if (c == 9 || c == 10 || c == 13) continue;
        if (c < 32 || c > 126) return 0;
    }
    return 1;
}

int main(void) {
    const char *a = u_args();
    char url[U_HTTP_MAX_URL + 1];
    int i, nd = 0;
    red_cfg_t cfg;
    red_if_t rif;
    u_http_url_t u;
    u_http_res_t r;
    unsigned int t0, ms = 0;
    int k, nbody = 0;
    while (*a == ' ') a++;
    while (*a && *a != ' ' && nd < U_HTTP_MAX_URL) {
        url[nd++] = *a++;
    }
    url[nd] = 0;
    while (*a == ' ') a++;
    if (!nd || *a) {
        u_print("Uso: http http://dominio[/ruta]\n");
        return 2;
    }
    if (u_http_url(url, &u)) {
        u_print("http: URL invalida (solo http://dominio[:puerto][/ruta])\n");
        return 2;
    }
    for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
    if (u_red_interfaz(&rif) || !rif.presente) {
        u_print("http: sin adaptador\n");
        return 1;
    }
    for (i = 0; i < 16; i++) ((unsigned char *)&cfg)[i] = 0;
    if (u_red_configurar_leer(&cfg) || !cfg.ip) {
        u_print("http: sin IP (configura una IP o DHCP primero)\n");
        return 1;
    }
    t0 = ahora();
    k = u_http_trae(url, &r, hcuerpo, U_HTTP_MAX_CPO, 60000);
    ms = (ahora() - t0) * 10u;
    if (k == -2) {
        u_print("http: URL invalida (solo http://dominio[:puerto][/ruta])\n");
        return 2;
    }
    if (k == -3) {
        u_print("http: no existe (NXDOMAIN)\n");
        return 3;
    }
    if (k == -5) {
        u_print("http: sin respuesta (revisa la red)\n");
        return 1;
    }
    if (k == -4) {
        u_print("http: error DNS (sin respuesta valida)\n");
        return 4;
    }
    if (k == -6) {
        u_print("http: respuesta malformada (revisa el servidor)\n");
        return 4;
    }
    if (k == -7) {
        u_print("http: transferencia no soportada (solo HTTP/1.0 simple)\n");
        return 4;
    }
    if (k == -8) {
        u_print("http: cuerpo truncado o demasiado grande\n");
        return 4;
    }
    if (k) {
        u_print("http: sin respuesta (revisa la red)\n");
        return 1;
    }
    nbody = (int)r.cuerpo_n;
    if ((unsigned int)nbody > (unsigned int)U_HTTP_MAX_CPO)
        nbody = U_HTTP_MAX_CPO;
    hcuerpo[nbody] = 0;
    {
        char val[U_HTTP_VAL];
        pon_txt("http: ");
        pon_txt(url);
        pon_txt("\nIP: ");
        pon_ip(r.ip);
        pon_txt(" puerto ");
        pon_num(u.puerto);
        pon_txt("\n");
        pon_txt(r.version);
        pon_txt(" ");
        pon_num((unsigned int)r.codigo);
        pon_txt(" ");
        pon_txt(r.motivo);
        pon_txt("\n");
        if (u_http_cabecera(&r, "Content-Type", val,
                            (int)sizeof(val)) >= 0) {
            pon_txt("Content-Type: ");
            pon_txt(val);
            pon_txt("\n");
        }
        if (u_http_cabecera(&r, "Content-Length", val,
                            (int)sizeof(val)) >= 0) {
            pon_txt("Content-Length: ");
            pon_txt(val);
            pon_txt("\n");
        }
        if (u_http_cabecera(&r, "Connection", val,
                            (int)sizeof(val)) >= 0) {
            pon_txt("Connection: ");
            pon_txt(val);
            pon_txt("\n");
        }
        if (u_http_cabecera(&r, "Location", val,
                            (int)sizeof(val)) >= 0) {
            pon_txt("Location: ");
            pon_txt(val);
            pon_txt(" (sin seguir: F7-B no redirige)\n");
        }
        pon_txt("cuerpo: ");
        pon_num((unsigned int)nbody);
        pon_txt(" bytes en ");
        pon_num(ms);
        pon_txt(" ms");
        {
            unsigned int chk = 0;
            int j;
            for (j = 0; j < nbody; j++) chk += hcuerpo[j];
            pon_txt(" chk ");
            pon_num(chk);
        }
        pon_txt("\n");
        if (!nbody) {
            pon_txt("(cuerpo vacio)\n");
        } else if (es_texto(hcuerpo, nbody)) {
            u_print((const char *)hcuerpo);
            if (hcuerpo[nbody - 1] != '\n') u_print("\n");
        } else {
            pon_txt("[cuerpo binario: ");
            pon_num((unsigned int)nbody);
            pon_txt(" bytes, no mostrado]\n");
        }
    }
    {
        UFILE *f = u_fopen("/httpsal.txt", "w");
        if (f) {
            char b[96];
            int t = 0, o;
            const char *s = "HTTP respuesta\nURL: ";
            while (*s && t < 90) b[t++] = *s++;
            u_fwrite(b, t, f);
            {
                int q = 0;
                while (url[q]) q++;
                u_fwrite(url, q, f);
            }
            t = 0;
            s = "\nIP: ";
            while (*s && t < 90) b[t++] = *s++;
            {
                unsigned int v = r.ip;
                for (o = 0; o < 4; o++) {
                    unsigned int c = (v >> (o * 8)) & 0xFFu;
                    char nb[12];
                    int q2 = 0;
                    u_utoa(c, nb, 10);
                    while (nb[q2] && t < 90) b[t++] = nb[q2++];
                    if (o < 3) b[t++] = '.';
                }
            }
            b[t++] = '\n';
            u_fwrite(b, t, f);
            t = 0;
            s = "Estado: ";
            while (*s && t < 90) b[t++] = *s++;
            {
                int q2 = 0;
                while (r.version[q2] && t < 90)
                    b[t++] = r.version[q2++];
                if (t < 90) b[t++] = ' ';
                {
                    char nb[12];
                    int q3 = 0;
                    u_utoa((unsigned int)r.codigo, nb, 10);
                    while (nb[q3] && t < 90) b[t++] = nb[q3++];
                }
                if (t < 90) b[t++] = ' ';
                q2 = 0;
                while (r.motivo[q2] && t < 90)
                    b[t++] = r.motivo[q2++];
            }
            b[t++] = '\n';
            u_fwrite(b, t, f);
            t = 0;
            s = "Tamano cuerpo: ";
            while (*s && t < 90) b[t++] = *s++;
            {
                char nb[12];
                int q2 = 0;
                u_utoa((unsigned int)nbody, nb, 10);
                while (nb[q2] && t < 90) b[t++] = nb[q2++];
            }
            {
                const char *s2 = "  Tiempo: ";
                int q2 = 0;
                while (s2[q2] && t < 90) b[t++] = s2[q2++];
            }
            {
                char nb[12];
                int q2 = 0;
                u_utoa(ms, nb, 10);
                while (nb[q2] && t < 90) b[t++] = nb[q2++];
            }
            {
                const char *s2 = " ms\n";
                int q2 = 0;
                while (s2[q2] && t < 90) b[t++] = s2[q2++];
            }
            u_fwrite(b, t, f);
            if (nbody > 0) {
                int q2 = 0;
                t = 0;
                s = "Cuerpo:\n";
                while (*s && t < 90) b[t++] = *s++;
                u_fwrite(b, t, f);
                while (q2 < nbody) {
                    int trozo = nbody - q2;
                    if (trozo > 512) trozo = 512;
                    u_fwrite(hcuerpo + q2, trozo, f);
                    q2 += trozo;
                }
            }
            u_fclose(f);
        }
    }
    return 0;
}
