#include "ulib.h"

void __main(void) {
}

/* v2.3-red F4-F6D: cliente DNS (Ring3). Resolucion completa en
   ulib (u_dns_resuelve3): cache, UDP, CNAME (<=8), TCP si TC.
   Este programa es la interfaz: un dominio, muestra e informa. */

static unsigned int ahora(void) {
    int t = u_ticks();
    return t < 0 ? 0u : (unsigned int)t;
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

int main(void) {
    const char *a = u_args();
    char dom[256];
    int i, nd = 0;
    red_cfg_t cfg;
    red_if_t rif;
    unsigned char preg[300];
    u_dns_res_t r;
    unsigned int t0, ms = 0;
    int k;
    while (*a == ' ') a++;
    while (*a && *a != ' ' && nd < 255) dom[nd++] = *a++;
    dom[nd] = 0;
    while (*a == ' ') a++;
    if (!nd || *a) {
        u_print("Uso: dns <dominio>\n");
        return 2;
    }
    if (u_dns_armar(dom, 0, preg, (int)sizeof(preg)) < 0) {
        u_print("dns: dominio invalido (usa letras, digitos y puntos)\n");
        return 2;
    }
    for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
    if (u_red_interfaz(&rif) || !rif.presente) {
        u_print("dns: sin adaptador\n");
        return 5;
    }
    for (i = 0; i < 16; i++) ((unsigned char *)&cfg)[i] = 0;
    if (u_red_configurar_leer(&cfg) || !cfg.ip) {
        u_print("dns: sin IP (configura una IP o DHCP primero)\n");
        return 5;
    }
    if (!cfg.dns) {
        u_print("dns: sin servidor DNS (DHCP o configurar_ip dns)\n");
        return 5;
    }
    t0 = ahora();
    k = u_dns_resuelve3(dom, 0, 0, &r, 30000);
    ms = (ahora() - t0) * 10u;
    if (k == -2) {
        u_print("dns: dominio invalido (usa letras, digitos y puntos)\n");
        return 2;
    }
    if (k == -3) {
        u_print("dns: no existe (NXDOMAIN)\n");
        return 3;
    }
    if (k == -5) {
        u_print("dns: sin respuesta (revisa la red)\n");
        return 1;
    }
    if (k == -4) {
        u_print("dns: error del servidor (codigo ");
        pon_num(r.rcode >= 0 ? (unsigned int)r.rcode : 2u);
        u_print(")\n");
        return 4;
    }
    if (k == -6) {
        u_print("dns: truncada y TCP fallo (revisa la red)\n");
        return 4;
    }
    if (k == -7) {
        u_print("dns: alias con ciclo o demasiado largo\n");
        return 4;
    }
    if (k == -8) {
        u_print("dns: sin registros A\n");
        return 4;
    }
    if (k) {
        u_print("dns: sin respuesta (revisa la red)\n");
        return 1;
    }
    u_print("dns: ");
    u_print(dom);
    u_print(" -> ");
    pon_ip(r.ip);
    if (r.nrec > 1) {
        u_print(" (+");
        pon_num((unsigned int)(r.nrec - 1));
        u_print(" mas)");
    }
    u_print(" ttl ");
    pon_num(r.ttl);
    u_print(r.de_cache ? " cache " : " red ");
    u_print(r.via_tcp ? "TCP " : "UDP ");
    u_print("en ");
    pon_num(ms);
    u_print(" ms\n");
    if (r.cname[0]) {
        u_print("dns: alias: ");
        u_print(r.cname);
        u_print("\n");
    }
    {
        UFILE *f = u_fopen("/dnssal.txt", "w");
        if (f) {
            char b[96];
            int t = 0, o;
            const char *s = "DNS resuelta\nDominio: ";
            while (*s && t < 90) b[t++] = *s++;
            u_fwrite(b, t, f);
            u_fwrite(dom, nd, f);
            t = 0;
            s = "\nServidor: ";
            while (*s && t < 90) b[t++] = *s++;
            {
                unsigned int v = cfg.dns;
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
            s = "IP: ";
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
            s = "Tipo: A  TTL: ";
            while (*s && t < 90) b[t++] = *s++;
            {
                char nb[12];
                int q2 = 0;
                u_utoa(r.ttl, nb, 10);
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
            t = 0;
            s = r.de_cache ? "Origen: cache\n" : "Origen: red\n";
            while (*s && t < 90) b[t++] = *s++;
            u_fwrite(b, t, f);
            t = 0;
            s = r.via_tcp ? "Transporte: TCP\n" : "Transporte: UDP\n";
            while (*s && t < 90) b[t++] = *s++;
            u_fwrite(b, t, f);
            if (r.cname[0]) {
                t = 0;
                s = "Cadena: ";
                while (*s && t < 90) b[t++] = *s++;
                u_fwrite(b, t, f);
                {
                    int q3 = 0;
                    while (r.cname[q3] && q3 < 240) q3++;
                    u_fwrite(r.cname, q3, f);
                }
                t = 0;
                s = "\n";
                while (*s && t < 90) b[t++] = *s++;
                u_fwrite(b, t, f);
            }
            u_fclose(f);
        }
    }
    return 0;
}
