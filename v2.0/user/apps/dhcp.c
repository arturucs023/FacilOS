#include "ulib.h"

void __main(void) {
}

/* v2.3-red F3: cliente DHCP (Ring3, trama cruda). No usa la pila
   UDP del nucleo (F4): construye Ethernet+IPv4+UDP+DHCP el solo
   con syscalls 52/53/54 y aplica con red_configurar (55).
   obtener: discover -> offer -> request -> ack.
   renovar: request broadcast INIT-REBOOT con la IP actual.
   liberar: descubre el servidor, release unicast, limpia local. */

static red_if_t rif;
static unsigned char inb[1600];

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

static unsigned short cks(const unsigned char *h, int n) {
    unsigned int s = 0;
    int i;
    for (i = 0; i + 1 < n; i += 2)
        s += ((unsigned int)h[i] << 8) | h[i + 1];
    if (i < n) s += (unsigned int)h[i] << 8;
    while (s >> 16) s = (s & 0xFFFFu) + (s >> 16);
    return (unsigned short)(~s & 0xFFFFu);
}

/* Suma IP seudocabecera UDP. */
static unsigned short cks_udp(const unsigned char *src4,
                              const unsigned char *dst4,
                              const unsigned char *u, int n) {
    unsigned int s = 0;
    int i;
    s += ((unsigned int)src4[0] << 8) | src4[1];
    s += ((unsigned int)src4[2] << 8) | src4[3];
    s += ((unsigned int)dst4[0] << 8) | dst4[1];
    s += ((unsigned int)dst4[2] << 8) | dst4[3];
    s += 17u;
    s += (unsigned int)n;
    for (i = 0; i + 1 < n; i += 2)
        s += ((unsigned int)u[i] << 8) | u[i + 1];
    if (i < n) s += (unsigned int)u[i] << 8;
    while (s >> 16) s = (s & 0xFFFFu) + (s >> 16);
    s = ~s & 0xFFFFu;
    return (unsigned short)(s ? s : 0xFFFFu);
}

static int armar_ip_Udp(unsigned char *m, const unsigned char *mac,
                        unsigned int src, unsigned int dst,
                        const unsigned char *carga, int ncarga) {
    int i, tot = 20 + 8 + ncarga;
    for (i = 0; i < 6; i++) {
        m[i] = 0xFF;
        m[i + 6] = mac[i];
    }
    m[12] = 0x08;
    m[13] = 0x00;
    m[14] = 0x45;
    m[15] = 0;
    m[16] = (unsigned char)((tot >> 8) & 0xFFu);
    m[17] = (unsigned char)(tot & 0xFFu);
    m[18] = 0x12;
    m[19] = 0x34;
    m[20] = 0;
    m[21] = 0;
    m[22] = 64;
    m[23] = 17;
    m[24] = 0;
    m[25] = 0;
    m[26] = (unsigned char)(src & 0xFFu);
    m[27] = (unsigned char)((src >> 8) & 0xFFu);
    m[28] = (unsigned char)((src >> 16) & 0xFFu);
    m[29] = (unsigned char)((src >> 24) & 0xFFu);
    m[30] = (unsigned char)(dst & 0xFFu);
    m[31] = (unsigned char)((dst >> 8) & 0xFFu);
    m[32] = (unsigned char)((dst >> 16) & 0xFFu);
    m[33] = (unsigned char)((dst >> 24) & 0xFFu);
    {
        unsigned short c = cks(m + 14, 20);
        m[24] = 0;
        m[25] = 0;
        m[24] = (unsigned char)((c >> 8) & 0xFFu);
        m[25] = (unsigned char)(c & 0xFFu);
    }
    m[34] = 0;
    m[35] = 68;
    m[36] = 0;
    m[37] = 67;
    {
        int ul = 8 + ncarga;
        m[38] = (unsigned char)((ul >> 8) & 0xFFu);
        m[39] = (unsigned char)(ul & 0xFFu);
        m[40] = 0;
        m[41] = 0;
        for (i = 0; i < ncarga; i++) m[42 + i] = carga[i];
        {
            unsigned short c = cks_udp(m + 26, m + 30, m + 34, ul);
            m[40] = (unsigned char)((c >> 8) & 0xFFu);
            m[41] = (unsigned char)(c & 0xFFu);
        }
    }
    return 14 + tot;
}

/* Rellena la cabecera BOOTP (28 B) + sname/file + magic.
   DHCP. d = inicio del area DHCP (tras eth+IP+UDP). */
static int armar_dhcp(unsigned char *d, unsigned char op,
                      unsigned int xid, unsigned int ci,
                      unsigned int req, unsigned int srv) {
    int i, t;
    d[0] = 1;
    d[1] = 1;
    d[2] = 6;
    d[3] = 0;
    d[4] = (unsigned char)((xid >> 24) & 0xFFu);
    d[5] = (unsigned char)((xid >> 16) & 0xFFu);
    d[6] = (unsigned char)((xid >> 8) & 0xFFu);
    d[7] = (unsigned char)(xid & 0xFFu);
    d[8] = 0;
    d[9] = 0;
    d[10] = 0x80;
    d[11] = 0;
    d[12] = (unsigned char)(ci & 0xFFu);
    d[13] = (unsigned char)((ci >> 8) & 0xFFu);
    d[14] = (unsigned char)((ci >> 16) & 0xFFu);
    d[15] = (unsigned char)((ci >> 24) & 0xFFu);
    for (i = 16; i < 28; i++) d[i] = 0;
    for (i = 28; i < 44; i++) d[i] = 0;   /* chaddr: se rellena fuera */
    for (i = 44; i < 44 + 64; i++) d[i] = 0;
    for (i = 108; i < 108 + 128; i++) d[i] = 0;
    d[238] = 0;
    d[239] = 0;
    d[236] = 99;
    d[237] = 130;
    d[238] = 83;
    d[239] = 99;
    t = 240;
    d[t++] = 53;
    d[t++] = 1;
    d[t++] = op;
    if (req) {
        d[t++] = 50;
        d[t++] = 4;
        d[t++] = (unsigned char)(req & 0xFFu);
        d[t++] = (unsigned char)((req >> 8) & 0xFFu);
        d[t++] = (unsigned char)((req >> 16) & 0xFFu);
        d[t++] = (unsigned char)((req >> 24) & 0xFFu);
    }
    if (srv) {
        d[t++] = 54;
        d[t++] = 4;
        d[t++] = (unsigned char)(srv & 0xFFu);
        d[t++] = (unsigned char)((srv >> 8) & 0xFFu);
        d[t++] = (unsigned char)((srv >> 16) & 0xFFu);
        d[t++] = (unsigned char)((srv >> 24) & 0xFFu);
    }
    d[t++] = 55;
    d[t++] = 3;
    d[t++] = 1;
    d[t++] = 3;
    d[t++] = 6;
    d[t++] = 255;
    while (t < 300) d[t++] = 0;
    return t < 300 ? 300 : t;
}

static unsigned int lee_u32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

/* F6-B D1: tiempo DHCP (opcion 51) en orden de red (big-endian).
   Las direcciones usan lee_u32 (orden del anfitrion en FacilOS32);
   el tiempo NO: 00 00 00 3C = 60 s (LE daria 1006632960). */

static unsigned int lee_be32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

/* Espera un mensaje DHCP del tipo pedido; extrae oferta. */
static int espera_dhcp(unsigned char tipo, unsigned int xid,
                       unsigned int *yip, unsigned int *mask,
                       unsigned int *gw, unsigned int *dns,
                       unsigned int *srv, unsigned int *lease,
                       int espera_ms) {
    unsigned int fin = ahora() + (unsigned int)espera_ms;
    while ((int)(ahora() - fin) < 0) {
        int n = u_red_trama_recibir(inb, 1518, 100);
        int o, fin_o;
        if (n < 14 + 20 + 8 + 240) continue;
        if (inb[12] != 0x08 || inb[13] != 0x00) continue;
        if (inb[23] != 17) continue;
        if (inb[36] != 0 || inb[37] != 68) continue;
        if (inb[42] != 2) continue;              /* BOOTREPLY */
        if (inb[46] != (unsigned char)((xid >> 24) & 0xFFu) ||
            inb[47] != (unsigned char)((xid >> 16) & 0xFFu) ||
            inb[48] != (unsigned char)((xid >> 8) & 0xFFu) ||
            inb[49] != (unsigned char)(xid & 0xFFu))
            continue;
        if (inb[278] != 99 || inb[279] != 130 || inb[280] != 83 ||
            inb[281] != 99)
            continue;
        o = 282;
        fin_o = n;
        if (fin_o > (int)sizeof(inb)) fin_o = (int)sizeof(inb);
        {
            int mtipo = -1;
            unsigned int rip = lee_u32(inb + 58);
            unsigned int m = 0, g = 0, d = 0, s = 0, l = 0;
            while (o + 1 < fin_o) {
                int cod = inb[o];
                int ln;
                if (cod == 255) break;
                if (cod == 0) {
                    o++;
                    continue;
                }
                if (o + 1 >= fin_o) break;
                ln = inb[o + 1];
                if (o + 2 + ln > fin_o) break;
                if (cod == 53 && ln >= 1) mtipo = inb[o + 2];
                else if (cod == 1 && ln == 4)
                    m = lee_u32(inb + o + 2);
                else if (cod == 3 && ln >= 4)
                    g = lee_u32(inb + o + 2);
                else if (cod == 6 && ln >= 4)
                    d = lee_u32(inb + o + 2);
                else if (cod == 54 && ln == 4)
                    s = lee_u32(inb + o + 2);
                else if (cod == 51 && ln == 4)
                    l = lee_be32(inb + o + 2);
                /* F6-B: 58/59 (renovacion/religa) se ignoran a
                   proposito: sin temporizador de renovacion en
                   esta fase; la concesion es informativa. */
                o += 2 + ln;
            }
            if (mtipo != tipo || !rip) continue;
            *yip = rip;
            if (mask) *mask = m;
            if (gw) *gw = g;
            if (dns) *dns = d;
            if (srv) *srv = s;
            if (lease) *lease = l;
            return 0;
        }
    }
    return 1;
}

static void pon_modo(const char *s) {
    u_print("dhcp: ");
    u_print(s);
    u_print("\n");
}

int main(void) {
    const char *a = u_args();
    char modo[16];
    int i, modo_renovar = 0, modo_liberar = 0;
    unsigned int xid;
    unsigned int mi_ip = 0;
    unsigned int yip = 0, mask = 0, gw = 0, dns = 0, srv = 0;
    unsigned int lease = 0;
    int intento;
    while (*a == ' ') a++;
    i = 0;
    while (*a && *a != ' ' && i < 15) modo[i++] = *a++;
    modo[i] = 0;
    if (i > 0) {
        if (!u_strcmp(modo, "renovar")) modo_renovar = 1;
        else if (!u_strcmp(modo, "liberar")) modo_liberar = 1;
        else {
            u_print("Uso: dhcp [renovar|liberar]\n");
            return 2;
        }
    }
    for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
    if (u_red_interfaz(&rif) || !rif.presente) {
        /* Sin adaptador pero con diagnostico del probe (etapa
           virtio, NICs PCI vistas, primer clase-red en mac[]):
           se muestra en hex para pedirlo al usuario. */
        static const char hx[] = "0123456789abcdef";
        char b[64];
        int t = 0, q;
        const char *pre = "dhcp: sin adaptador (dbg et=";
        while (pre[t]) {
            b[t] = pre[t];
            t++;
        }
        b[t++] = hx[(rif.rsv[1] >> 4) & 15];
        b[t++] = hx[rif.rsv[1] & 15];
        b[t++] = hx[(rif.rsv[0] >> 4) & 15];
        b[t++] = hx[rif.rsv[0] & 15];
        {
            const char *m2 = " nics=";
            q = 0;
            while (m2[q]) b[t++] = m2[q++];
        }
        b[t++] = hx[(rif.rsv[2] >> 4) & 15];
        b[t++] = hx[rif.rsv[2] & 15];
        {
            const char *m2 = " red=";
            q = 0;
            while (m2[q]) b[t++] = m2[q++];
        }
        for (q = 0; q < 6; q++) {
            b[t++] = hx[(rif.mac[q] >> 4) & 15];
            b[t++] = hx[rif.mac[q] & 15];
        }
        b[t++] = '\n';
        b[t] = 0;
        u_print(b);
        return 1;
    }
    xid = ((unsigned int)u_mi_pid() * 7919u + ahora()) ^ 0x12340000u;
    if (modo_liberar) {
        /* Aprende el servidor y libera; limpia local siempre. */
        unsigned char pet[360];
        int n;
        red_cfg_t c;
        int j;
        for (j = 0; j < 16; j++) ((unsigned char *)&c)[j] = 0;
        if (u_red_configurar_leer(&c)) return 1;
        mi_ip = c.ip;
        if (!mi_ip) {
            u_print("dhcp: nada que liberar\n");
            return 0;
        }
        pon_modo("liberando...");
        n = armar_dhcp(pet + 42, 1, xid, 0, 0, 0);
        for (i = 0; i < 6; i++) pet[42 + 28 + i] = rif.mac[i];
        {
            unsigned char marco[600];
            int t = armar_ip_Udp(marco, rif.mac, 0,
                                 0xFFFFFFFFu, pet + 42, n);
            u_red_trama_enviar(marco, t);
        }
        if (espera_dhcp(2, xid, &yip, &mask, &gw, &dns, &srv, &lease,
                        2000) == 0 && srv) {
            unsigned char rel[360];
            int t;
            n = armar_dhcp(rel + 42, 7, xid + 1, mi_ip, 0, srv);
            for (i = 0; i < 6; i++) rel[42 + 28 + i] = rif.mac[i];
            t = armar_ip_Udp(rel, rif.mac, mi_ip, srv, rel + 42, n);
            u_red_trama_enviar(rel, t);
            (void)t;
        }
        u_red_fijar(1, 0);
        u_red_fijar(2, 0);
        u_red_fijar(3, 0);
        u_red_fijar(4, 0);
        u_print("dhcp: configuracion liberada\n");
        return 0;
    }
    if (modo_renovar) {
        /* Lee la actual y la repide (INIT-REBOOT). */
        red_cfg_t c;
        int j;
        for (j = 0; j < 16; j++) ((unsigned char *)&c)[j] = 0;
        if (u_red_configurar_leer(&c) || !c.ip) {
            u_print("dhcp: sin IP actual; obteniendo...\n");
        } else {
            mi_ip = c.ip;
            yip = c.ip;
        }
    }
    for (intento = 0; intento < 3; intento++) {
        unsigned char pet[360];
        int n, tr;
        unsigned int req = modo_renovar ? yip : 0;
        u_printf("dhcp: intento %d %s...\n", intento + 1,
                 modo_renovar ? "renovando" : "descubriendo");
        n = armar_dhcp(pet + 42, 1, xid, 0, req, 0);
        for (i = 0; i < 6; i++) pet[42 + 28 + i] = rif.mac[i];
        {
            unsigned char marco[600];
            int t = armar_ip_Udp(marco, rif.mac, 0, 0xFFFFFFFFu,
                                 pet + 42, n);
            u_print("dhcp: enviando...\n");
            tr = u_red_trama_enviar(marco, t);
            u_print(tr >= 0 ? "dhcp: Tx ok\n" : "dhcp: fallo de envio\n");
            if (tr < 0) return 1;
        }
        if (espera_dhcp(2, xid, &yip, &mask, &gw, &dns, &srv, &lease,
                        4000) == 0 && yip)
            break;
        yip = 0;
    }
    if (!yip) {
        u_print("dhcp: sin respuesta (revisa la red)\n");
        return 1;
    }
    u_print("dhcp: oferta de ");
    pon_ip(srv ? srv : gw);
    u_print(" para ");
    pon_ip(yip);
    u_print("\n");
    for (intento = 0; intento < 3; intento++) {
        unsigned char pet[360];
        int n;
        unsigned int ack_ip = 0, ack_m = 0, ack_g = 0, ack_d = 0;
        unsigned int ack_s = 0, ack_l = 0;
        if (intento == 0) pon_modo("solicitando...");
        n = armar_dhcp(pet + 42, 3, xid, 0, yip, srv);
        for (i = 0; i < 6; i++) pet[42 + 28 + i] = rif.mac[i];
        {
            unsigned char marco[600];
            int t = armar_ip_Udp(marco, rif.mac, 0, 0xFFFFFFFFu,
                                 pet + 42, n);
            if (u_red_trama_enviar(marco, t) < 0) {
                u_print("dhcp: fallo de envio\n");
                return 1;
            }
        }
        if (espera_dhcp(5, xid, &ack_ip, &ack_m, &ack_g, &ack_d, &ack_s,
                        &ack_l, 4000) == 0 &&
            ack_ip == yip) {
            mask = ack_m;
            if (ack_g) gw = ack_g;
            if (ack_d) dns = ack_d;
            lease = ack_l;
            break;
        }
        yip = 0;
    }
    if (!yip) {
        u_print("dhcp: sin confirmacion (revisa la red)\n");
        return 1;
    }
    u_red_fijar(1, yip);
    if (mask) u_red_fijar(2, mask);
    if (gw) u_red_fijar(3, gw);
    if (dns) u_red_fijar(4, dns);
    u_print("dhcp: concedida ");
    pon_ip(yip);
    u_print(" mascara ");
    pon_ip(mask);
    u_print(" puerta ");
    pon_ip(gw);
    u_print(" dns ");
    pon_ip(dns);
    u_print("\n");
    {
        char b[48];
        int t = 0, q = 0;
        const char *s = "concesion (s): ";
        while (s[q] && t < 40) b[t++] = s[q++];
        {
            char nb[12];
            int k = 0;
            u_utoa(lease, nb, 10);
            while (nb[k] && t < 46) b[t++] = nb[k++];
        }
        b[t] = 0;
        u_print(b);
        u_print("\n");
    }
    {
        UFILE *f = u_fopen("/dhcsal.txt", "w");
        if (f) {
            char b[256];
            int t = 0;
            const char *s = "DHCP concedida\nIP: ";
            while (*s && t < 240) b[t++] = *s++;
            {
                unsigned int v = yip;
                int o;
                for (o = 0; o < 4; o++) {
                    unsigned int a = (v >> (o * 8)) & 0xFFu;
                    char nb[12];
                    int q2 = 0;
                    u_utoa(a, nb, 10);
                    while (nb[q2] && t < 240) b[t++] = nb[q2++];
                    if (o < 3) b[t++] = '.';
                }
            }
            b[t++] = '\n';
            u_fwrite(b, t, f);
            u_fclose(f);
        }
    }
    return 0;
}
