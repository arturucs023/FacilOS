#include "ulib.h"

void __main(void) {
}

/* v2.3-red F4: cliente DNS (Ring3) sobre UDP propio (58-60).
   A/IN al servidor de la configuracion (DHCP o manual).
   Sin TCP, DNSSEC, DoH/DoT (fuera de F4). Sin cache en F4:
   cada consulta pregunta al servidor (ver docs/network/DNS.md). */

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
    red_udp_envio_t e;
    red_udp_peticion_t pet;
    unsigned char preg[300], resp[8 + 512];
    unsigned int ips[8], ttls[8];
    unsigned short sport = 0;
    unsigned int t0, ms = 0;
    int intento, rcode = -1, nrec = 0;
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
    {
        int p = u_red_udp_ligar(0);
        if (p <= 0) {
            u_print("dns: no se pudo ligar puerto\n");
            return 5;
        }
        sport = (unsigned short)p;
    }
    t0 = ahora();
    for (intento = 0; intento < 3 && !nrec; intento++) {
        unsigned short xid;
        unsigned int fin;
        int npreg;
        xid = (unsigned short)(((unsigned int)u_mi_pid() * 7919u +
                                ahora() + (unsigned int)intento * 131u) &
                               0xFFFFu);
        npreg = u_dns_armar(dom, xid, preg, (int)sizeof(preg));
        if (npreg < 0) return 2;
        e.ip = cfg.dns;
        e.dpuerto = 53;
        e.spuerto = sport;
        if (u_red_udp_enviar(&e, preg, npreg) < 0) {
            u_print("dns: fallo de envio\n");
            return 5;
        }
        fin = ahora() + 300u;
        while ((int)(ahora() - fin) < 0) {
            int r;
            pet.puerto = sport;
            pet.espera = 500;
            r = u_red_udp_recibir(&pet, resp, (int)sizeof(resp));
            if (r < 0) return 5;
            if (!r) continue;
            {
                int k = u_dns_leer(resp + 8, r - 8, xid, preg, npreg,
                                   ips, ttls, 8, &rcode);
                if (k == -2) continue;   /* ajena: sigue esperando */
                if (k == -4) {
                    u_print("dns: respuesta truncada (sin TCP en F4)\n");
                    return 4;
                }
                if (k < 0) continue;     /* ruido: sigue esperando */
                ms = (ahora() - t0) * 10u;
                if (rcode == 3) {
                    u_print("dns: no existe (NXDOMAIN)\n");
                    return 3;
                }
                if (rcode) {
                    u_print("dns: error del servidor (codigo ");
                    pon_num((unsigned int)rcode);
                    u_print(")\n");
                    return 4;
                }
                if (!k) {
                    u_print("dns: sin registros A\n");
                    return 4;
                }
                nrec = k;
            }
        }
    }
    if (!nrec) {
        u_print("dns: sin respuesta (revisa la red)\n");
        return 1;
    }
    u_print("dns: ");
    u_print(dom);
    u_print(" -> ");
    pon_ip(ips[0]);
    if (nrec > 1) {
        u_print(" (+");
        pon_num((unsigned int)(nrec - 1));
        u_print(" mas)");
    }
    u_print(" ttl ");
    pon_num(ttls[0]);
    u_print(" en ");
    pon_num(ms);
    u_print(" ms\n");
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
            /* IP del servidor en texto */
            {
                unsigned int v = cfg.dns;
                for (o = 0; o < 4; o++) {
                    unsigned int c = (v >> (o * 8)) & 0xFFu;
                    char nb[12];
                    int q = 0;
                    u_utoa(c, nb, 10);
                    while (nb[q] && t < 90) b[t++] = nb[q++];
                    if (o < 3) b[t++] = '.';
                }
            }
            b[t++] = '\n';
            u_fwrite(b, t, f);
            t = 0;
            s = "IP: ";
            while (*s && t < 90) b[t++] = *s++;
            {
                unsigned int v = ips[0];
                for (o = 0; o < 4; o++) {
                    unsigned int c = (v >> (o * 8)) & 0xFFu;
                    char nb[12];
                    int q = 0;
                    u_utoa(c, nb, 10);
                    while (nb[q] && t < 90) b[t++] = nb[q++];
                    if (o < 3) b[t++] = '.';
                }
            }
            if (nrec > 1) {
                const char *s2 = " (+";
                int q = 0;
                char nb[12];
                while (s2[q] && t < 90) b[t++] = s2[q++];
                u_utoa((unsigned int)(nrec - 1), nb, 10);
                q = 0;
                while (nb[q] && t < 90) b[t++] = nb[q++];
                b[t++] = ')';
            }
            b[t++] = '\n';
            u_fwrite(b, t, f);
            t = 0;
            s = "Tipo: A  TTL: ";
            while (*s && t < 90) b[t++] = *s++;
            {
                char nb[12];
                int q = 0;
                u_utoa(ttls[0], nb, 10);
                while (nb[q] && t < 90) b[t++] = nb[q++];
            }
            {
                const char *s2 = "  Tiempo: ";
                int q = 0;
                while (s2[q] && t < 90) b[t++] = s2[q++];
            }
            {
                char nb[12];
                int q = 0;
                u_utoa(ms, nb, 10);
                while (nb[q] && t < 90) b[t++] = nb[q++];
            }
            {
                const char *s2 = " ms\n";
                int q = 0;
                while (s2[q] && t < 90) b[t++] = s2[q++];
            }
            u_fwrite(b, t, f);
            u_fclose(f);
        }
    }
    return 0;
}
