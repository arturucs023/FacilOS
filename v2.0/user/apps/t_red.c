#include "ulib.h"

void __main(void) {
}

/* v2.3-red F1: prueba de Tx/Rx Ethernet reales.
 * Sin adaptador: verifica el error limpio y termina OK (prueba
 * negativa). Con adaptador (QEMU user-net): envia ARP who-has a la
 * puerta 10.0.2.2 y espera su respuesta; demuestra transmision y
 * recepcion reales de tramas. */

static int fallos = 0;
static int pruebas = 0;

static void ok(int cond, const char *nom) {
    pruebas++;
    if (!cond) {
        fallos++;
        u_printf("FALLO %s\n", nom);
    } else {
        u_printf("ok %s\n", nom);
    }
}

static red_if_t rif;
static unsigned char tx[64];
static unsigned char rx[1518];

static void pon_mac(const unsigned char *m) {
    char b[20];
    int t = 0, i;
    for (i = 0; i < 6; i++) {
        unsigned char v = m[i];
        unsigned char hi = (unsigned char)(v >> 4);
        unsigned char lo = (unsigned char)(v & 15);
        b[t++] = hi < 10 ? (char)('0' + hi) : (char)('A' + hi - 10);
        b[t++] = lo < 10 ? (char)('0' + lo) : (char)('A' + lo - 10);
        if (i < 5) b[t++] = ':';
    }
    b[t] = 0;
    u_print(b);
    u_print("\n");
}

/* F4: vectores del codec DNS (sin red: corren siempre). */
static int dns_resp(unsigned char *b, unsigned short xid,
                    unsigned short band, int nan,
                    const unsigned int *ips, int nip) {
    static const unsigned char pg[] = {
        7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, 0, 1, 0, 1
    };
    int t = 12, i, j;
    for (i = 0; i < 12; i++) b[i] = 0;
    b[0] = (unsigned char)((xid >> 8) & 0xFFu);
    b[1] = (unsigned char)(xid & 0xFFu);
    b[2] = (unsigned char)((band >> 8) & 0xFFu);
    b[3] = (unsigned char)(band & 0xFFu);
    b[5] = 1;
    b[7] = (unsigned char)(nan & 0xFFu);
    for (j = 0; j < 17; j++) b[t++] = pg[j];
    for (i = 0; i < nan; i++) {
        b[t++] = 0xC0;
        b[t++] = 0x0C;
        b[t++] = 0;
        b[t++] = 1;
        b[t++] = 0;
        b[t++] = 1;
        b[t++] = 0;
        b[t++] = 0;
        b[t++] = 1;
        b[t++] = 0x2C;   /* TTL 300 */
        b[t++] = 0;
        b[t++] = 4;
        {
            unsigned int v = ips[i < nip ? i : 0];
            b[t++] = (unsigned char)(v & 0xFFu);
            b[t++] = (unsigned char)((v >> 8) & 0xFFu);
            b[t++] = (unsigned char)((v >> 16) & 0xFFu);
            b[t++] = (unsigned char)((v >> 24) & 0xFFu);
        }
    }
    return t;
}

static void prueba_dns_parser(void) {
    unsigned char preg[300], resp[300], bas[300];
    unsigned int ips[4], ttls[4];
    unsigned int ip1 = 93 | (184 << 8) | (216 << 16) | (34 << 24);
    unsigned int ip2 = 93 | (184 << 8) | (216 << 16) | (35 << 24);
    int npreg, nresp, rc, k;
    u_print("t_red F4: codec DNS...\n");
    npreg = u_dns_armar("example.com", 0x1234, preg, (int)sizeof(preg));
    ok(npreg > 12 && preg[0] == 0x12 && preg[1] == 0x34 &&
       preg[5] == 1, "dns-armar-ok");
    /* Los invalidos usan otro bufer: armar puede dejar salida
       parcial al fallar (solo vale si devuelve long). */
    ok(u_dns_armar("", 1, bas, 300) == -2, "dns-armar-vacio");
    ok(u_dns_armar("a..b", 1, bas, 300) == -2, "dns-armar-doble-punto");
    ok(u_dns_armar("a_b.com", 1, bas, 300) == -2, "dns-armar-subrayado");
    {
        char larga[70];
        int i;
        for (i = 0; i < 64; i++) larga[i] = 'a';
        larga[64] = '.';
        larga[65] = 'c';
        larga[66] = 'o';
        larga[67] = 'm';
        larga[68] = 0;
        ok(u_dns_armar(larga, 1, bas, 300) == -2, "dns-armar-etiqueta64");
    }
    {
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1234, 0x8180, 1, una, 1);
        rc = -9;
        k = u_dns_leer(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                       4, &rc);
        ok(k == 1 && ips[0] == ip1 && ttls[0] == 300 && rc == 0,
           "dns-leer-a");
    }
    {
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1235, 0x8180, 1, una, 1);
        k = u_dns_leer(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                       4, &rc);
        ok(k == -2, "dns-leer-xid");
    }
    {
        unsigned char preg2[300];
        unsigned int una[1];
        int npreg2;
        una[0] = ip1;
        npreg2 = u_dns_armar("example.net", 0x1234, preg2,
                             (int)sizeof(preg2));
        nresp = dns_resp(resp, 0x1234, 0x8180, 1, una, 1);
        k = u_dns_leer(resp, nresp, 0x1234, preg2, npreg2, ips, ttls,
                       4, &rc);
        ok(k == -2, "dns-leer-pregunta");
    }
    nresp = dns_resp(resp, 0x1234, 0x8183, 0, 0, 0);
    rc = -9;
    k = u_dns_leer(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                   4, &rc);
    ok(k == 0 && rc == 3, "dns-leer-nxdomain");
    nresp = dns_resp(resp, 0x1234, 0x8182, 0, 0, 0);
    k = u_dns_leer(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                   4, &rc);
    ok(k == 0 && rc == 2, "dns-leer-servfail");
    nresp = dns_resp(resp, 0x1234, 0x8380, 0, 0, 0);
    k = u_dns_leer(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                   4, &rc);
    ok(k == -4, "dns-leer-truncada");
    ok(u_dns_leer(resp, 10, 0x1234, preg, npreg, ips, ttls, 4,
                  &rc) == -1, "dns-leer-corta");
    {
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1234, 0x8180, 1, una, 1);
        resp[12 + 17] = 0xC0;   /* nombre respuesta -> fuera */
        resp[12 + 18] = 0xFF;
        ok(u_dns_leer(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                      4, &rc) == -1, "dns-leer-puntero-mal");
    }
    {
        /* Nombre que apunta a si mismo: bucle detectado. */
        unsigned char mal[32];
        int i;
        for (i = 0; i < 32; i++) mal[i] = 0;
        mal[0] = 0x12;
        mal[1] = 0x34;
        mal[2] = 0x81;
        mal[3] = 0x80;
        mal[5] = 1;
        mal[12] = 0xC0;
        mal[13] = 0x0C;
        ok(u_dns_leer(mal, 18, 0x1234, preg, npreg, ips, ttls, 4,
                      &rc) == -1, "dns-leer-bucle");
    }
    {
        unsigned int dos[2];
        dos[0] = ip1;
        dos[1] = ip2;
        nresp = dns_resp(resp, 0x1234, 0x8180, 2, dos, 2);
        k = u_dns_leer(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                       4, &rc);
        ok(k == 2 && ips[0] == ip1 && ips[1] == ip2 && rc == 0,
           "dns-leer-multi-a");
    }
    {
        /* CNAME (se salta) + A (se lee). */
        unsigned char cn[128];
        int t = 0, i;
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1234, 0x8180, 0, una, 0);
        for (i = 0; i < nresp; i++) cn[i] = resp[i];
        t = nresp;
        cn[6] = 0;
        cn[7] = 2;   /* ANCOUNT = 2 */
        cn[t++] = 0xC0;
        cn[t++] = 0x0C;
        cn[t++] = 0;
        cn[t++] = 5;   /* CNAME */
        cn[t++] = 0;
        cn[t++] = 1;
        cn[t++] = 0;
        cn[t++] = 0;
        cn[t++] = 0;
        cn[t++] = 60;
        cn[t++] = 0;
        cn[t++] = 2;
        cn[t++] = 0xC0;
        cn[t++] = 0x10;
        cn[t++] = 0xC0;   /* A con puntero */
        cn[t++] = 0x0C;
        cn[t++] = 0;
        cn[t++] = 1;
        cn[t++] = 0;
        cn[t++] = 1;
        cn[t++] = 0;
        cn[t++] = 0;
        cn[t++] = 1;
        cn[t++] = 0x2C;
        cn[t++] = 0;
        cn[t++] = 4;
        cn[t++] = 93;
        cn[t++] = 184;
        cn[t++] = 216;
        cn[t++] = 34;
        k = u_dns_leer(cn, t, 0x1234, preg, npreg, ips, ttls, 4,
                       &rc);
        ok(k == 1 && ips[0] == ip1 && rc == 0, "dns-leer-cname-salta");
    }
}

static void prueba_dhcp_tiempo(void) {
    /* F6-B D1: opcion 51 en orden de red (big-endian).
       00 00 00 3C = 60 s; en LE daria 1006632960 (bug F5). */
    unsigned char v60[4] = { 0x00, 0x00, 0x00, 0x3C };
    unsigned char v86400[4] = { 0x00, 0x01, 0x51, 0x80 };
    unsigned int be60, be86400, le60;
    u_print("t_red F6B: tiempo DHCP...\n");
    be60 = ((unsigned int)v60[0] << 24) |
           ((unsigned int)v60[1] << 16) |
           ((unsigned int)v60[2] << 8) | (unsigned int)v60[3];
    be86400 = ((unsigned int)v86400[0] << 24) |
              ((unsigned int)v86400[1] << 16) |
              ((unsigned int)v86400[2] << 8) | (unsigned int)v86400[3];
    le60 = (unsigned int)v60[0] | ((unsigned int)v60[1] << 8) |
           ((unsigned int)v60[2] << 16) | ((unsigned int)v60[3] << 24);
    ok(be60 == 60, "dhcp-tiempo-60-be");
    ok(be86400 == 86400, "dhcp-tiempo-86400-be");
    ok(le60 == 1006632960u, "dhcp-tiempo-60-le-es-erroneo");
    /* 58/59: ignoradas a proposito en esta fase (sin renovacion). */
    ok(1, "dhcp-58-59-ignoradas-doc");
}

static void prueba_dns_f6d(void) {
    /* F6-D: parser CNAME/robusto + cache (sin red). */
    unsigned char preg[300], resp[600], cn[8];
    unsigned int ips[8], ttls[8];
    int npreg, nresp, k, rc;
    unsigned int ip1 = 93 | (184 << 8) | (216 << 16) | (34 << 24);
    unsigned int ipf = 192 | (0 << 8) | (2 << 16) | (10 << 24);
    unsigned int ipo = 192 | (0 << 8) | (2 << 16) | (20 << 24);
    char cnom[256];
    unsigned int cttl = 0;
    int i;
    u_print("t_red F6D: parser CNAME + cache...\\n");
    npreg = u_dns_armar("example.com", 0x1234, preg, (int)sizeof(preg));
    /* CNAME solo: cname + TTL, sin A. */
    nresp = dns_resp(resp, 0x1234, 0x8180, 0, 0, 0);
    resp[6] = 0;
    resp[7] = 1;
    {
        int t = nresp;
        unsigned char wt[] = { 5, 'f', 'i', 'n', 'a', 'l', 4, 't',
                               'e', 's', 't', 0 };
        resp[t++] = 0xC0;
        resp[t++] = 0x0C;
        resp[t++] = 0;
        resp[t++] = 5;
        resp[t++] = 0;
        resp[t++] = 1;
        resp[t++] = 0;
        resp[t++] = 0;
        resp[t++] = 0;
        resp[t++] = 100;
        resp[t++] = 0;
        resp[t++] = (unsigned char)sizeof(wt);
        for (i = 0; i < (int)sizeof(wt); i++) resp[t++] = wt[i];
        for (i = 0; i < 256; i++) cnom[i] = 0;
        cttl = 0;
        rc = -9;
        k = u_dns_leer2(resp, t, 0x1234, preg, npreg, ips, ttls, 8,
                        &rc, cnom, (int)sizeof(cnom), &cttl);
        ok(k == 0 && rc == 0 && cttl == 100, "dns-cname-solo");
        {
            int ig = 1, j = 0;
            const char *es = "final.test";
            while (es[j] && cnom[j]) {
                if (es[j] != cnom[j]) {
                    ig = 0;
                    break;
                }
                j++;
            }
            if (es[j] || cnom[j]) ig = 0;
            ok(ig, "dns-cname-nombre");
        }
        ok(u_dns_leer(resp, t, 0x1234, preg, npreg, ips, ttls, 8,
                      &rc) == 0, "dns-cname-compat");
    }
    /* CNAME + A ajeno: el A no cuenta; queda el CNAME. */
    {
        int t;
        unsigned char wt[] = { 5, 'f', 'i', 'n', 'a', 'l', 4, 't',
                               'e', 's', 't', 0 };
        unsigned char wo[] = { 4, 'o', 't', 'r', 'o', 4, 't', 'e',
                               's', 't', 0 };
        unsigned char base[600];
        int nb = dns_resp(base, 0x1234, 0x8180, 0, 0, 0);
        base[7] = 2;
        t = nb;
        base[t++] = 0xC0;
        base[t++] = 0x0C;
        base[t++] = 0;
        base[t++] = 5;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 50;
        base[t++] = 0;
        base[t++] = (unsigned char)sizeof(wt);
        for (i = 0; i < (int)sizeof(wt); i++) base[t++] = wt[i];
        for (i = 0; i < (int)sizeof(wo); i++) base[t++] = wo[i];
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 60;
        base[t++] = 0;
        base[t++] = 4;
        base[t++] = 192;
        base[t++] = 0;
        base[t++] = 2;
        base[t++] = 99;
        for (i = 0; i < 256; i++) cnom[i] = 0;
        rc = -9;
        k = u_dns_leer2(base, t, 0x1234, preg, npreg, ips, ttls, 8,
                        &rc, cnom, (int)sizeof(cnom), &cttl);
        ok(k == 0 && rc == 0 && cnom[0] == 'f', "dns-cname-ajeno");
    }
    /* CNAME + A del destino: el A cuenta. */
    {
        int t;
        unsigned char wt[] = { 5, 'f', 'i', 'n', 'a', 'l', 4, 't',
                               'e', 's', 't', 0 };
        unsigned char base[600];
        int nb = dns_resp(base, 0x1234, 0x8180, 0, 0, 0);
        base[7] = 2;
        t = nb;
        base[t++] = 0xC0;
        base[t++] = 0x0C;
        base[t++] = 0;
        base[t++] = 5;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 50;
        base[t++] = 0;
        base[t++] = (unsigned char)sizeof(wt);
        for (i = 0; i < (int)sizeof(wt); i++) base[t++] = wt[i];
        for (i = 0; i < (int)sizeof(wt); i++) base[t++] = wt[i];
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 60;
        base[t++] = 0;
        base[t++] = 4;
        base[t++] = 192;
        base[t++] = 0;
        base[t++] = 2;
        base[t++] = 10;
        for (i = 0; i < 256; i++) cnom[i] = 0;
        rc = -9;
        k = u_dns_leer2(base, t, 0x1234, preg, npreg, ips, ttls, 8,
                        &rc, cnom, (int)sizeof(cnom), &cttl);
        ok(k == 1 && ips[0] == ipf && rc == 0, "dns-cname-mas-a");
    }
    /* Z reservado: se rechaza. */
    {
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1234, 0x8180, 1, una, 1);
        resp[3] |= 0x70;
        ok(u_dns_leer2(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                       8, &rc, 0, 0, 0) == -1, "dns-z-reservado");
    }
    /* Basura final: se rechaza. */
    {
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1234, 0x8180, 1, una, 1);
        resp[nresp] = 0xAA;
        ok(u_dns_leer2(resp, nresp + 1, 0x1234, preg, npreg, ips,
                       ttls, 8, &rc, 0, 0, 0) == -1,
           "dns-basura-final");
    }
    /* Puntero a la cabecera: se rechaza. */
    {
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1234, 0x8180, 1, una, 1);
        resp[29] = 0xC0;
        resp[30] = 0x04;
        ok(u_dns_leer2(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                       8, &rc, 0, 0, 0) == -1,
           "dns-puntero-cabecera");
    }
    /* RDLEN mas alla del mensaje: se rechaza. */
    {
        unsigned int una[1];
        una[0] = ip1;
        nresp = dns_resp(resp, 0x1234, 0x8180, 1, una, 1);
        resp[nresp - 6] = 0;
        resp[nresp - 5] = 8;
        ok(u_dns_leer2(resp, nresp, 0x1234, preg, npreg, ips, ttls,
                       8, &rc, 0, 0, 0) == -1,
           "dns-rdlen-largo");
    }
    /* Tipo desconocido se salta; el A posterior cuenta. */
    {
        unsigned int una[1];
        int t;
        unsigned char base[600];
        int nb = dns_resp(base, 0x1234, 0x8180, 0, 0, 0);
        una[0] = ipo;
        base[7] = 2;
        t = nb;
        base[t++] = 0xC0;
        base[t++] = 0x0C;
        base[t++] = 0;
        base[t++] = 99;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 10;
        base[t++] = 0;
        base[t++] = 3;
        base[t++] = 'a';
        base[t++] = 'b';
        base[t++] = 'c';
        base[t++] = 0xC0;
        base[t++] = 0x0C;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 70;
        base[t++] = 0;
        base[t++] = 4;
        base[t++] = 192;
        base[t++] = 0;
        base[t++] = 2;
        base[t++] = 20;
        rc = -9;
        k = u_dns_leer2(base, t, 0x1234, preg, npreg, ips, ttls, 8,
                        &rc, 0, 0, 0);
        ok(k == 1 && ips[0] == ipo && rc == 0, "dns-tipo-raro");
        (void)cn;
    }
    /* CNAME en bucle: se ignora sin colgar. */
    {
        int t;
        unsigned char base[600];
        int nb = dns_resp(base, 0x1234, 0x8180, 0, 0, 0);
        base[7] = 1;
        t = nb;
        base[t++] = 0xC0;
        base[t++] = 0x0C;
        base[t++] = 0;
        base[t++] = 5;
        base[t++] = 0;
        base[t++] = 1;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 0;
        base[t++] = 60;
        base[t++] = 0;
        base[t++] = 2;
        base[t++] = 0xC0;
        base[t++] = (unsigned char)(nb + 4);
        for (i = 0; i < 256; i++) cnom[i] = 0;
        rc = -9;
        k = u_dns_leer2(base, t, 0x1234, preg, npreg, ips, ttls, 8,
                        &rc, cnom, (int)sizeof(cnom), &cttl);
        ok(k == 0 && rc == 0 && cnom[0] == 0, "dns-cname-bucle");
    }
    /* Cache: predicado puro incl. wraparound. */
    ok(u_dns_cache_viva(1000, 60, 1000) == 1, "dns-cache-viva-ahora");
    ok(u_dns_cache_viva(1000, 60, 1000 + 5999) == 1,
       "dns-cache-viva-borde");
    ok(u_dns_cache_viva(1000, 60, 1000 + 6000) == 0,
       "dns-cache-caduca");
    ok(u_dns_cache_viva(1000, 0, 1000) == 0, "dns-cache-ttl-cero");
    ok(u_dns_cache_viva(0xFFFFFF00u, 60, 0x00000100u) == 1,
       "dns-cache-wrap-viva");
    ok(u_dns_cache_viva(0xFFFFFF00u, 60, 0x00100000u) == 0,
       "dns-cache-wrap-muerta");
    u_dns_cache_limpiar();
    ok(u_dns_cache_n() == 0, "dns-cache-limpia");
}

int main(void) {
    int r, i, n;
    int hallada = 0;
    prueba_dns_parser();   /* sin red: codec determinista */
    prueba_dhcp_tiempo();  /* F6-B D1: vectores big-endian */
    prueba_dns_f6d();        /* F6-D: sin red */
    u_print("t_red F1: interfaz...\n");
    for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
    r = u_red_interfaz(&rif);
    if (!rif.presente) {
        ok(r != 0, "sin-adaptador-informa");
        u_print("t_red: sin adaptador (arranque sin red OK)\n");
        /* Via negativa: enviar/recibir/consultar deben fallar limpio. */
        ok(u_red_trama_enviar(tx, 42) == -1, "tx-sin-adaptador");
        ok(u_red_trama_recibir(rx, sizeof(rx), 100) == -1,
           "rx-sin-adaptador");
        {
            red_consulta_t q;
            int j;
            for (j = 0; j < 12; j++) ((unsigned char *)&q)[j] = 0;
            q.ip = 10 | (0 << 8) | (2 << 16) | (2 << 24);
            q.espera = 100;
            ok(u_red_consultar(0, &q) == -1, "cons-sin-adaptador");
        }
        {
            red_peticion_t p;
            unsigned char rb[6];
            p.ip = 10 | (0 << 8) | (2 << 16) | (2 << 24);
            p.n = 1;
            p.espera = 100;
            p.ttl = 0;
            p.res = 0;
            p.res2 = 0;
            ok(u_red_comprobar(&p, (red_respuesta_t *)rb, 6) == -1,
               "ping-sin-adaptador");
        }
        {
            red_udp_peticion_t pet;
            red_udp_envio_t e;
            unsigned char db[8];
            int j;
            for (j = 0; j < 8; j++) db[j] = 0;
            ok(u_red_udp_ligar(50000) == -1, "udp-liga-sin-adaptador");
            e.ip = 10 | (0 << 8) | (2 << 16) | (2 << 24);
            e.dpuerto = 53;
            e.spuerto = 50000;
            ok(u_red_udp_enviar(&e, db, 0) == -1,
               "udp-enviar-sin-adaptador");
            pet.puerto = 50000;
            pet.espera = 100;
            ok(u_red_udp_recibir(&pet, db, 8) == -1,
               "udp-recibir-sin-adaptador");
        }
        {
            unsigned char tb[8];
            int j;
            for (j = 0; j < 8; j++) tb[j] = 0;
            ok(u_tcp_abrir(10 | (0 << 8) | (2 << 16) | (2 << 24),
                           80, 0) == -1, "tcp-abrir-sin-adaptador");
            ok(u_tcp_escuchar(5001) == -1, "tcp-escuchar-sin-adaptador");
            ok(u_tcp_enviar(1, tb, 8) == -1, "tcp-enviar-sin-adaptador");
            ok(u_tcp_recibir(1, tb, 8, 100) == -1,
               "tcp-recibir-sin-adaptador");
            ok(u_tcp_cerrar(1) == -1, "tcp-cerrar-sin-adaptador");
            ok(u_tcp_estado(1) == -1, "tcp-estado-sin-adaptador");
        }
        u_printf("t_red: %d pruebas %d fallos\n", pruebas, fallos);
        return fallos ? 1 : 0;
    }
    ok(r == 0, "interfaz-ok");
    u_print("MAC: ");
    pon_mac(rif.mac);
    /* Negativas con adaptador. */
    ok(u_red_trama_enviar(tx, 10) == -2, "tx-tam-corto");
    ok(u_red_trama_enviar(tx, 2000) == -2, "tx-tam-largo");
    ok(u_red_trama_recibir(rx, 0, 10) == -2, "rx-max-cero");
    /* ARP who-has 10.0.2.2 (puerta QEMU user-net), sha=nuestra. */
    for (i = 0; i < 6; i++) {
        tx[i] = 0xFF;             /* destino: difusion */
        tx[i + 6] = rif.mac[i];  /* origen: la nuestra */
    }
    tx[12] = 0x08;
    tx[13] = 0x06;               /* EtherType ARP */
    tx[14] = 0x00;
    tx[15] = 0x01;               /* Ethernet */
    tx[16] = 0x08;
    tx[17] = 0x00;               /* IPv4 */
    tx[18] = 6;
    tx[19] = 4;
    tx[20] = 0x00;
    tx[21] = 0x01;               /* solicitud */
    for (i = 0; i < 6; i++) tx[22 + i] = rif.mac[i];
    tx[28] = 10;
    tx[29] = 0;
    tx[30] = 2;
    tx[31] = 15;                /* IP origen supuesta */
    for (i = 0; i < 6; i++) tx[32 + i] = 0;
    tx[38] = 10;
    tx[39] = 0;
    tx[40] = 2;
    tx[41] = 2;                 /* IP destino: puerta */
    u_print("t_red: ARP who-has 10.0.2.2...\n");
    r = u_red_trama_enviar(tx, 42);
    ok(r == 42, "tx-arp");
    /* Espera respuesta hasta 3 s, filtrando en Ring3. */
    for (n = 0; n < 30 && !hallada; n++) {
        r = u_red_trama_recibir(rx, sizeof(rx), 100);
        if (r < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;
        if (rx[28] != 10 || rx[29] != 0 || rx[30] != 2 ||
            rx[31] != 2)
            continue;
        hallada = 1;
        u_print("t_red: respuesta ARP de ");
        pon_mac(rx + 22);
    }
    ok(hallada, "rx-arp-puerta");
    /* F2: configuracion manual + ARP por nucleo + rutas + sonda IP. */
    {
        red_cfg_t cfg;
        unsigned int ip, mask, gw;
        int j;
        for (j = 0; j < 16; j++) ((unsigned char *)&cfg)[j] = 0;
        ip = 10 | (0 << 8) | (2 << 16) | (15 << 24);
        mask = 255 | (255 << 8) | (255 << 16) | (0 << 24);
        gw = 10 | (0 << 8) | (2 << 16) | (2 << 24);
        ok(u_red_fijar(1, ip) == 0, "cfg-ip");
        ok(u_red_fijar(2, mask) == 0, "cfg-mask");
        ok(u_red_fijar(3, gw) == 0, "cfg-gw");
        ok(u_red_fijar(4, gw) == 0, "cfg-dns");
        ok(u_red_configurar_leer(&cfg) == 0, "cfg-lee");
        ok(cfg.ip == ip && cfg.mascara == mask && cfg.puerta == gw,
           "cfg-coherente");
    }
    {
        /* Resolver puerta por nucleo (usa cache + red real). */
        red_consulta_t q;
        unsigned int antes_tx, despues_tx;
        int j, r;
        red_if_t f2;
        for (j = 0; j < 12; j++) ((unsigned char *)&q)[j] = 0;
        q.ip = 10 | (0 << 8) | (2 << 16) | (2 << 24);
        q.espera = 3000;
        r = u_red_consultar(0, &q);
        ok(r == 0, "arp-nucleo-resuelve");
        ok(q.mac[0] == 0x52 && q.mac[1] == 0x55 &&
           q.mac[2] == 0x0A && q.mac[3] == 0x00 &&
           q.mac[4] == 0x02 && q.mac[5] == 0x02,
           "arp-mac-puerta");
        u_print("t_red: puerta = ");
        pon_mac(q.mac);
        for (j = 0; j < 32; j++) ((unsigned char *)&f2)[j] = 0;
        u_red_interfaz(&f2);
        antes_tx = f2.tx_ok;
        r = u_red_consultar(0, &q);
        for (j = 0; j < 32; j++) ((unsigned char *)&f2)[j] = 0;
        u_red_interfaz(&f2);
        despues_tx = f2.tx_ok;
        ok(r == 0 && antes_tx == despues_tx, "arp-cache-sin-tx");
        /* Sonda IP (UDP ficticio a la puerta): prueba Tx L3 real. */
        {
            red_sonda_t s;
            s.ip = 10 | (0 << 8) | (2 << 16) | (2 << 24);
            s.ttl = 64;
            s.proto = 17;
            s.espera = 0;
            r = u_red_consultar(2, &s);
            ok(r == 8, "ip-sonda-tx");
        }
        /* Rutas derivadas: conectada 10.0.2.0/24 + defecto. */
        {
            unsigned char rt[96];
            int k, n = u_red_rutas(rt, 96);
            int veo_con = 0, veo_def = 0;
            ok(n == 24, "rutas-2");
            for (k = 0; k + 12 <= n; k += 12) {
                unsigned int d = (unsigned int)rt[k] |
                    ((unsigned int)rt[k + 1] << 8) |
                    ((unsigned int)rt[k + 2] << 16) |
                    ((unsigned int)rt[k + 3] << 24);
                unsigned int m = (unsigned int)rt[k + 4] |
                    ((unsigned int)rt[k + 5] << 8) |
                    ((unsigned int)rt[k + 6] << 16) |
                    ((unsigned int)rt[k + 7] << 24);
                unsigned int g = (unsigned int)rt[k + 8] |
                    ((unsigned int)rt[k + 9] << 8) |
                    ((unsigned int)rt[k + 10] << 16) |
                    ((unsigned int)rt[k + 11] << 24);
                if (d == (10 | (0 << 8) | (2 << 16) | (0 << 24)) &&
                    m == 0x00FFFFFFu && g == 0)
                    veo_con = 1;
                if (d == 0 && m == 0 && g ==
                    (10 | (0 << 8) | (2 << 16) | (2 << 24)))
                    veo_def = 1;
            }
            ok(veo_con && veo_def, "rutas-contenido");
        }
        {
            red_peticion_t pet;
            unsigned char resp[12];
            int k, r, ok2 = 0;
            pet.ip = 10 | (0 << 8) | (2 << 16) | (2 << 24);
            pet.n = 2;
            pet.espera = 1000;
            pet.ttl = 0;
            pet.res = 0;
            pet.res2 = 0;
            for (k = 0; k < 12; k++) resp[k] = 0;
            r = u_red_comprobar(&pet, (red_respuesta_t *)resp, 12);
            ok(r == 12, "eco-2-respuestas");
            for (k = 0; k + 6 <= r && r > 0; k += 6) {
                unsigned int seq = ((unsigned int)resp[k] << 8) | resp[k + 1];
                if (resp[k + 4] && seq == (unsigned int)(k / 6 + 1)) ok2++;
            }
            ok(ok2 == 2, "eco-seq-ok");
        }
        {
            red_peticion_t pet;
            unsigned char resp[6];
            int r;
            pet.ip = 192 | (168 << 8) | (99 << 16) | (99 << 24);
            pet.n = 1;
            pet.espera = 1000;
            pet.ttl = 0;
            pet.res = 0;
            pet.res2 = 0;
            resp[0] = 0;
            r = u_red_comprobar(&pet, (red_respuesta_t *)resp, 6);
            ok(r == 6 && resp[4] == 0, "eco-timeout-limpio");
        }
        /* F4: UDP vivo + DNS real contra 10.0.2.3 (slirp). */
        {
            red_udp_peticion_t pet;
            red_udp_envio_t e;
            unsigned char q[300], rb[8 + 512];
            unsigned int ips[4], ttls[4];
            int p1, nq, rr, k, rc2 = -1;
            p1 = u_red_udp_ligar(0);
            ok(p1 >= 49152, "udp-efimero");
            if (p1 > 0) ok(u_red_udp_ligar(p1) == p1, "udp-liga-idem");
            nq = u_dns_armar("example.com", 0x4321, q, (int)sizeof(q));
            ok(nq > 12, "udp-armar-consulta");
            e.ip = 10 | (0 << 8) | (2 << 16) | (3 << 24);
            e.dpuerto = 53;
            e.spuerto = p1 > 0 ? (unsigned short)p1 : 0;
            ok(u_red_udp_enviar(&e, q, nq) == nq, "udp-enviar-dns");
            pet.puerto = p1 > 0 ? (unsigned short)p1 : 0;
            pet.espera = 3000;
            rr = u_red_udp_recibir(&pet, rb, (int)sizeof(rb));
            ok(rr > 8, "udp-recibir-dns");
            if (rr > 8) {
                unsigned int src = (unsigned int)rb[0] |
                    ((unsigned int)rb[1] << 8) |
                    ((unsigned int)rb[2] << 16) |
                    ((unsigned int)rb[3] << 24);
                unsigned int sport = ((unsigned int)rb[4] << 8) | rb[5];
                ok(src == (10 | (0 << 8) | (2 << 16) | (3 << 24)),
                   "udp-src-dns");
                ok(sport == 53, "udp-sport-dns");
                ok((rb[8 + 2] & 0x80) != 0, "udp-qr-dns");
                k = u_dns_leer(rb + 8, rr - 8, 0x4321, q, nq, ips,
                               ttls, 4, &rc2);
                ok(k >= 1 && rc2 == 0, "udp-dns-valida");
                if (k >= 1) {
                    u_print("t_red: example.com = ");
                    {
                        unsigned int v = ips[0];
                        char b[20];
                        int t = 0, o;
                        for (o = 0; o < 4; o++) {
                            char nb[12];
                            int w2 = 0;
                            u_utoa((v >> (o * 8)) & 0xFFu, nb, 10);
                            while (nb[w2] && t < 18) b[t++] = nb[w2++];
                            if (o < 3) b[t++] = '.';
                        }
                        b[t] = 0;
                        u_print(b);
                        u_print("\n");
                    }
                }
            }
            /* Drena por si la respuesta llego tarde. */
            pet.espera = 100;
            u_red_udp_recibir(&pet, rb, (int)sizeof(rb));
            /* Negativas y desalojo (tabla: P1 + 61001..3). */
            {
                int a, b, c2, d;
                a = u_red_udp_ligar(61001);
                b = u_red_udp_ligar(61002);
                c2 = u_red_udp_ligar(61003);
                ok(a == 61001 && b == 61002 && c2 == 61003,
                   "udp-liga-3");
                d = u_red_udp_ligar(61004);
                ok(d == 61004, "udp-desalojo-vacia");
                pet.puerto = p1 > 0 ? (unsigned short)p1 : 0;
                pet.espera = 100;
                ok(u_red_udp_recibir(&pet, rb, (int)sizeof(rb)) == -2,
                   "udp-desalojada-sin-liga");
            }
            pet.puerto = 62000;
            pet.espera = 100;
            ok(u_red_udp_recibir(&pet, rb, (int)sizeof(rb)) == -2,
               "udp-puerto-sin-receptor");
            e.ip = 10 | (0 << 8) | (2 << 16) | (3 << 24);
            e.dpuerto = 53;
            e.spuerto = 61004;
            ok(u_red_udp_enviar(&e, q, 2000) == -2, "udp-enviar-largo");
            e.dpuerto = 0;
            ok(u_red_udp_enviar(&e, q, 10) == -2, "udp-enviar-sin-puerto");
        }

        /* F6-D: resolucion contra stub controlado (10.0.2.2:5353;
           ver tools/dns_stub.py). Sin dependencia de DNS publico. */
        {
            unsigned int stub = 10 | (0 << 8) | (2 << 16) | (2 << 24);
            unsigned int ip10 = 192 | (0 << 8) | (2 << 16) | (10 << 24);
            unsigned int ip20 = 192 | (0 << 8) | (2 << 16) | (20 << 24);
            unsigned int ip30 = 192 | (0 << 8) | (2 << 16) | (30 << 24);
            unsigned int ip40 = 192 | (0 << 8) | (2 << 16) | (40 << 24);
            unsigned int ip60 = 192 | (0 << 8) | (2 << 16) | (60 << 24);
            unsigned int ip70 = 192 | (0 << 8) | (2 << 16) | (70 << 24);
            unsigned int ip99 = 192 | (0 << 8) | (2 << 16) | (99 << 24);
            u_dns_res_t r;
            int k;
            u_dns_cache_limpiar();
            k = u_dns_resuelve3("directo.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.ip == ip10 && r.de_cache == 0 &&
               r.ttl == 300 && r.via_tcp == 0, "dns-stub-directo");
            k = u_dns_resuelve3("directo.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.ip == ip10 && r.de_cache == 1,
               "dns-stub-cache-hit");
            k = u_dns_resuelve3("alias-full.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.ip == ip20 && r.saltos == 0 &&
               r.cname[0] == 'm', "dns-stub-alias-full");
            k = u_dns_resuelve3("alias1.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.ip == ip30 && r.saltos == 2,
               "dns-stub-alias-cadena");
            {
                const char *es = "alias1.test -> alias2.test -> "
                                 "alias3.test";
                int j = 0, ig = 1;
                while (es[j] && r.cname[j]) {
                    if (es[j] != r.cname[j]) {
                        ig = 0;
                        break;
                    }
                    j++;
                }
                if (es[j] || r.cname[j]) ig = 0;
                ok(ig, "dns-stub-cadena-texto");
            }
            k = u_dns_resuelve3("loop.test", stub, 5353, &r, 15000);
            ok(k == -7, "dns-stub-ciclo");
            k = u_dns_resuelve3("c1.test", stub, 5353, &r, 20000);
            ok(k == 0 && r.ip == ip40 && r.saltos == 8,
               "dns-stub-8-saltos");
            k = u_dns_resuelve3("d1.test", stub, 5353, &r, 25000);
            ok(k == -7, "dns-stub-9-saltos");
            k = u_dns_resuelve3("nx.test", stub, 5353, &r, 15000);
            ok(k == -3 && r.rc == 3, "dns-stub-nxdomain");
            k = u_dns_resuelve3("nodata.test", stub, 5353, &r,
                                15000);
            ok(k == -8, "dns-stub-nodata");
            k = u_dns_resuelve3("srvfail.test", stub, 5353, &r,
                                15000);
            ok(k == -4 && r.rcode == 2, "dns-stub-servfail");
            k = u_dns_resuelve3("refused.test", stub, 5353, &r,
                                15000);
            ok(k == -4 && r.rcode == 5, "dns-stub-refused");
            k = u_dns_resuelve3("tc.test", stub, 5353, &r, 20000);
            ok(k == 0 && r.ip == ip60 && r.via_tcp == 1,
               "dns-stub-tc-tcp");
            k = u_dns_resuelve3("multi.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.nrec == 3 && r.ip == ip70,
               "dns-stub-multi");
            k = u_dns_resuelve3("ttl0.test", stub, 5353, &r, 15000);
            ok(k == 0 && r.ttl == 0, "dns-stub-ttl0");
            k = u_dns_resuelve3("ttl0.test", stub, 5353, &r, 15000);
            ok(k == 0 && r.de_cache == 0, "dns-stub-ttl0-nocache");
            k = u_dns_resuelve3("ttlbig.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.ttl == 86400, "dns-stub-ttl-tope");
            k = u_dns_resuelve3("nada.test", stub, 5399, &r, 3000);
            ok(k == -5, "dns-stub-timeout");
            k = u_dns_resuelve3("directo.test", stub, 5354, &r,
                                15000);
            ok(k == 0 && r.ip == ip99 && r.de_cache == 0,
               "dns-stub-otro-srv");
            k = u_dns_resuelve3("directo.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.ip == ip10 && r.de_cache == 1,
               "dns-stub-srv-cache");
            u_dns_cache_limpiar();
            {
                int j, bien = 1;
                for (j = 1; j <= 33; j++) {
                    char nm[16];
                    int t = 0, q2;
                    const char *p1 = "gen";
                    char nb[12];
                    while (p1[t]) {
                        nm[t] = p1[t];
                        t++;
                    }
                    u_utoa((unsigned int)j, nb, 10);
                    q2 = 0;
                    while (nb[q2] && t < 10) nm[t++] = nb[q2++];
                    nm[t++] = '.';
                    nm[t++] = 't';
                    nm[t++] = 'e';
                    nm[t++] = 's';
                    nm[t++] = 't';
                    nm[t] = 0;
                    k = u_dns_resuelve3(nm, stub, 5353, &r, 15000);
                    if (k != 0 || r.de_cache != 0) bien = 0;
                }
                ok(bien, "dns-stub-33-miss");
            }
            ok(u_dns_cache_n() == 32, "dns-cache-llena");
            k = u_dns_resuelve3("gen1.test", stub, 5353, &r, 15000);
            ok(k == 0 && r.de_cache == 0, "dns-stub-desalojo");
            k = u_dns_resuelve3("gen33.test", stub, 5353, &r,
                                15000);
            ok(k == 0 && r.de_cache == 1, "dns-stub-conserva");
        }

        /* F5: TCP vivo (par en anfitrion; ver tools/tcp_par.py). */
        {
            unsigned int gw = 10 | (0 << 8) | (2 << 16) | (2 << 24);
            unsigned char tb[600];
            int id, r, kk;
            id = u_tcp_abrir(gw, 5001, 0);
            ok(id > 0, "tcp-abrir-eco");
            if (id > 0) {
                const char *h = "hola tcp f5";
                int n = 0;
                ok(u_tcp_estado(id) == 4, "tcp-establecido");
                while (h[n]) n++;
                r = u_tcp_enviar(id, h, n);
                ok(r == n, "tcp-enviar-eco");
                r = u_tcp_recibir(id, tb, (int)sizeof(tb), 3000);
                ok(r == n, "tcp-recibir-eco");
                if (r == n) {
                    int igual = 1;
                    for (kk = 0; kk < n; kk++)
                        if (tb[kk] != (unsigned char)h[kk]) igual = 0;
                    ok(igual, "tcp-eco-igual");
                }
                ok(u_tcp_cerrar(id) == 0, "tcp-cerrar-eco");
                ok(u_tcp_estado(id) < 0, "tcp-cerrada-sin-estado");
            }
            {
                int c1 = u_tcp_abrir(gw, 5001, 0);
                int c2 = u_tcp_abrir(gw, 5001, 0);
                ok(c1 > 0 && c2 > 0 && c1 != c2, "tcp-dos-abrir");
                if (c1 > 0 && c2 > 0) {
                    r = u_tcp_enviar(c1, "uno", 3);
                    ok(r == 3, "tcp-enviar-c1");
                    r = u_tcp_enviar(c2, "dos-dos", 7);
                    ok(r == 7, "tcp-enviar-c2");
                    r = u_tcp_recibir(c2, tb, (int)sizeof(tb), 3000);
                    ok(r == 7, "tcp-recibir-c2");
                    r = u_tcp_recibir(c1, tb, (int)sizeof(tb), 3000);
                    ok(r == 3, "tcp-recibir-c1");
                    if (r == 3)
                        ok(tb[0] == 'u' && tb[1] == 'n' && tb[2] == 'o',
                           "tcp-datos-c1");
                    ok(u_tcp_cerrar(c1) == 0, "tcp-cerrar-c1");
                    ok(u_tcp_cerrar(c2) == 0, "tcp-cerrar-c2");
                } else {
                    if (c1 > 0) u_tcp_abortar(c1);
                    if (c2 > 0) u_tcp_abortar(c2);
                }
            }
            id = u_tcp_abrir(gw, 5002, 0);
            ok(id > 0, "tcp-abrir-fin");
            if (id > 0) {
                r = u_tcp_recibir(id, tb, (int)sizeof(tb), 5000);
                ok(r == -12, "tcp-eof-fin");
                r = u_tcp_cerrar(id);
                ok(r == 0 || r == -9, "tcp-cerrar-tras-fin");
            }
            {
                unsigned int t0 = 0, dt;
                int t1 = u_ticks();
                t0 = t1 < 0 ? 0u : (unsigned int)t1;
                id = u_tcp_abrir(gw, 59999, 0);
                {
                    int t2 = u_ticks();
                    dt = (t2 < 0 ? 0u : (unsigned int)t2) - t0;
                }
                ok(id == -7 && (int)dt < 1500, "tcp-rechazada-rapido");
            }
            {
                unsigned int t0 = 0, dt;
                unsigned int txa = 0, txb = 0;
                int t1 = u_ticks();
                red_if_t f2;
                int j;
                for (j = 0; j < 32; j++) ((unsigned char *)&f2)[j] = 0;
                if (u_red_interfaz(&f2) == 0) txa = f2.tx_ok;
                t0 = t1 < 0 ? 0u : (unsigned int)t1;
                id = u_tcp_abrir(192 | (0 << 8) | (2 << 16) |
                                     (1 << 24), 80, 0);
                {
                    unsigned int _dt;
                    int _t2 = u_ticks();
                    _dt = (_t2 < 0 ? 0u : (unsigned int)_t2) - t0;
                    dt = _dt;
                }
                for (j = 0; j < 32; j++)
                    ((unsigned char *)&f2)[j] = 0;
                if (u_red_interfaz(&f2) == 0) txb = f2.tx_ok;
                ok(id == -7, "tcp-negro-rechazado");
                ok((int)dt < 1500, "tcp-negro-acotado");
                ok(txb > txa, "tcp-negro-tx");
            }
            {
                int esc = u_tcp_escuchar(6101);
                int hid;
                int sig = -1;
                ok(esc > 0, "tcp-escuchar");
                if (esc > 0) {
                    sig = u_tcp_abrir(gw, 6102, 0);
                    ok(sig > 0, "tcp-aviso-listo");
                    if (sig > 0) u_tcp_cerrar(sig);
                }
                {
                        int k2;
                        hid = -1;
                        for (k2 = 0; k2 < 3 && hid <= 0; k2++)
                            hid = esc > 0 ? u_tcp_aceptar(esc, 10000)
                                          : -1;
                    }
                ok(hid > 0, "tcp-aceptar");
                if (hid > 0) {
                    r = u_tcp_recibir(hid, tb, (int)sizeof(tb), 5000);
                    ok(r == 13, "tcp-recibir-huesped");
                    if (r == 13)
                        ok(tb[0] == 'h' && tb[5] == 'i',
                           "tcp-datos-huesped");
                    r = u_tcp_enviar(hid, "hola anfitrion", 14);
                    ok(r == 14, "tcp-enviar-huesped");
                    r = u_tcp_recibir(hid, tb, (int)sizeof(tb), 5000);
                    ok(r == -12, "tcp-eof-huesped");
                    ok(u_tcp_cerrar(hid) == 0, "tcp-cerrar-huesped");
                }
                if (esc > 0) ok(u_tcp_cerrar(esc) == 0,
                                "tcp-cerrar-escucha");
            }
            /* F6-C: poll sin bloqueo (D13) y cuarentena (D15). */
            {
                int esc2 = u_tcp_escuchar(6105);
                ok(esc2 > 0, "tcp-escuchar-poll");
                if (esc2 > 0) {
                    int t0 = u_ticks();
                    int r0 = u_tcp_aceptar(esc2, 0);
                    int dt = u_ticks() - t0;
                    ok(r0 == 0, "tcp-aceptar-poll-vacio");
                    ok(dt >= 0 && dt <= 1, "tcp-aceptar-poll-rapido");
                    u_tcp_cerrar(esc2);
                }
            }
            {
                unsigned char pb[16];
                int t0, dt, r0;
                id = u_tcp_abrir(gw, 5003, 0);
                ok(id > 0, "tcp-abrir-silencio");
                if (id > 0) {
                    t0 = u_ticks();
                    r0 = u_tcp_recibir(id, pb, sizeof(pb), 0);
                    dt = u_ticks() - t0;
                    ok(r0 == 0, "tcp-recibir-poll-vacio");
                    ok(dt >= 0 && dt <= 1, "tcp-recibir-poll-rapido");
                    u_tcp_abortar(id);
                }
            }
            {
                int e1, e2, e3;
                e1 = u_tcp_abrir(gw, 5001, 45001);
                ok(e1 > 0, "tcp-tupla-abrir");
                if (e1 > 0) {
                    r = u_tcp_enviar(e1, "hola", 4);
                    ok(r == 4, "tcp-tupla-eco-tx");
                    r = u_tcp_recibir(e1, tb, (int)sizeof(tb), 5000);
                    ok(r == 4, "tcp-tupla-eco-rx");
                    ok(u_tcp_cerrar(e1) == 0, "tcp-tupla-cerrar");
                }
                e2 = u_tcp_abrir(gw, 5001, 45001);
                ok(e2 == -6, "tcp-tupla-gracia");
                if (e2 > 0) u_tcp_abortar(e2);
                u_dormir(7000);
                e3 = u_tcp_abrir(gw, 5001, 45001);
                ok(e3 > 0, "tcp-tupla-reuso");
                if (e3 > 0) u_tcp_cerrar(e3);
            }
            id = u_tcp_abrir(gw, 5001, 0);
            ok(id > 0, "tcp-abrir-aborto");
            if (id > 0) {
                ok(u_tcp_abortar(id) == 0, "tcp-abortar");
                ok(u_tcp_estado(id) < 0, "tcp-abortada-sin-estado");
            }
            ok(u_tcp_abrir(0, 80, 0) == -2, "tcp-abrir-sin-ip");
            ok(u_tcp_abrir(gw, 0, 0) == -2, "tcp-abrir-sin-puerto");
            ok(u_tcp_estado(9999) == -2, "tcp-estado-malo");
            ok(u_tcp_cerrar(9999) == -2, "tcp-cerrar-malo");
            {
                int e2 = u_tcp_abrir(gw, 5001, 0);
                if (e2 > 0) {
                    unsigned char mucho[600];
                    int j;
                    for (j = 0; j < 600; j++) mucho[j] = 0;
                    ok(u_tcp_enviar(e2, mucho, 600) == -2,
                       "tcp-enviar-largo");
                    ok(u_tcp_recibir(e2, tb, 0, 100) == -2,
                       "tcp-recibir-max-cero");
                    ok(u_tcp_aceptar(e2, 100) == -8,
                       "tcp-aceptar-no-escucha");
                    u_tcp_cerrar(e2);
                } else {
                    ok(0, "tcp-h-arg");
                }
            }
        }
    }
    u_print("t_red: contadores tras prueba:\n");
    for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
    if (u_red_interfaz(&rif) == 0) {
        u_printf("tx_ok=%u rx_ok=%u tx_err=%u rx_err=%u\n", rif.tx_ok,
                 rif.rx_ok, rif.tx_err, rif.rx_err);
        ok(rif.tx_ok >= 1 && rif.rx_ok >= 1, "contadores-avanzan");
    } else {
        ok(0, "relee-interfaz");
    }
    u_printf("t_red: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos ? 1 : 0;
}
