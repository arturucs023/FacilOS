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

int main(void) {
    int r, i, n;
    int hallada = 0;
    prueba_dns_parser();   /* sin red: codec determinista */
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
