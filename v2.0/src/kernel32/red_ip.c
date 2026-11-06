#include "red_ip.h"
#include "red.h"
#include "red_arp.h"
#include "red_icmp.h"
#include "red_udp.h"
#include "red_tcp.h"
#include "drv_red.h"
#include "libc.h"

/* v2.3-red F2: IPv4. Sin reensamblado ni reenvio (F6+). */

static uint32_t id_seq = 0;
static uint32_t ip_rx_ok = 0, ip_rx_err = 0;

uint16_t red_ip_cksum(const uint8_t *h, int n) {
    uint32_t s = 0;
    int i;
    for (i = 0; i + 1 < n; i += 2)
        s += ((uint32_t)h[i] << 8) | h[i + 1];
    if (i < n) s += (uint32_t)h[i] << 8;
    while (s >> 16) s = (s & 0xFFFFu) + (s >> 16);
    return (uint16_t)(~s & 0xFFFFu);
}

int red_ip_valida(const uint8_t *h, int n) {
    int ihl;
    int tot;
    if (n < 20) return -1;
    if ((h[0] >> 4) != 4) return -1;
    ihl = (h[0] & 0x0F) * 4;
    if (ihl < 20 || ihl > n) return -1;
    tot = (h[2] << 8) | h[3];
    if (tot < ihl || tot > n) return -1;
    if (red_ip_cksum(h, ihl) != 0) return -1;
    if (h[8] == 0) return -1;   /* TTL agotado */
    return 0;
}

int red_ip_enviar(uint32_t dst, uint8_t proto,
                  const uint8_t *carga, int ncarga, uint8_t ttl) {
    uint8_t marco[1514];
    uint8_t cab[20];
    uint8_t mac_dst[6], mac_src[6];
    uint32_t gw, mio, sig;
    int r, i;
    if (!red_hay()) return -1;
    if (!dst || !carga || ncarga < 0 || ncarga > IP_MTU_CARGA)
        return -2;
    mio = red_cfg_ip();
    if (!mio) return -2;
    {
        int rr;
        uint32_t gw = 0;
        rr = red_ruta_para(dst, &gw);
        if (rr < 0) return -3;   /* sin ruta */
        sig = gw ? gw : dst;
    }
    r = red_arp_resolver(sig, 3000, mac_dst);
    if (r) return -4;
    if (red_mac(mac_src)) return -1;
    for (i = 0; i < 6; i++) {
        marco[i] = mac_dst[i];
        marco[i + 6] = mac_src[i];
    }
    marco[12] = 0x08;
    marco[13] = 0x00;
    cab[0] = 0x45;
    cab[1] = 0;
    cab[2] = (uint8_t)(((20 + ncarga) >> 8) & 0xFFu);
    cab[3] = (uint8_t)((20 + ncarga) & 0xFFu);
    id_seq++;
    cab[4] = (uint8_t)((id_seq >> 8) & 0xFFu);
    cab[5] = (uint8_t)(id_seq & 0xFFu);
    cab[6] = 0;
    cab[7] = 0;
    cab[8] = ttl ? ttl : IP_TTL_DEF;
    cab[9] = proto;
    cab[10] = 0;
    cab[11] = 0;
    cab[12] = (uint8_t)(mio & 0xFFu);
    cab[13] = (uint8_t)((mio >> 8) & 0xFFu);
    cab[14] = (uint8_t)((mio >> 16) & 0xFFu);
    cab[15] = (uint8_t)((mio >> 24) & 0xFFu);
    cab[16] = (uint8_t)(dst & 0xFFu);
    cab[17] = (uint8_t)((dst >> 8) & 0xFFu);
    cab[18] = (uint8_t)((dst >> 16) & 0xFFu);
    cab[19] = (uint8_t)((dst >> 24) & 0xFFu);
    {
        uint16_t c = red_ip_cksum(cab, 20);
        cab[10] = (uint8_t)((c >> 8) & 0xFFu);
        cab[11] = (uint8_t)(c & 0xFFu);
    }
    for (i = 0; i < 20; i++) marco[14 + i] = cab[i];
    for (i = 0; i < ncarga; i++) marco[34 + i] = carga[i];
    r = red_tx(marco, 34 + ncarga);
    if (r < 0) return -5;
    return ncarga;
}

void red_ip_entrada(const uint8_t *m, int tam) {
    /* F2 valida y cuenta; F3 despacha ICMP (el resto sigue al
       deposito sin consumidor hasta F4/F5). */
    if (tam < 14 + 20) {
        ip_rx_err++;
        return;
    }
    if (red_ip_valida(m + 14, tam - 14)) {
        ip_rx_err++;
        return;
    }
    ip_rx_ok++;
    if (m[14 + 9] == 1) red_icmp_entrada(m, tam);
    else if (m[14 + 9] == 17) red_udp_entrada(m, tam);
    else if (m[14 + 9] == 6) red_tcp_entrada(m, tam);
}

void red_ip_kat(void) {
    /* Cabecera patron (TTL 64, proto 17, 192.168.0.1 -> .199,
       longitud total 115): checksum conocido B861. */
    static const uint8_t patron[20] = {
        0x45, 0x00, 0x00, 0x73, 0x00, 0x00, 0x40, 0x00,
        0x40, 0x11, 0x00, 0x00, 0xC0, 0xA8, 0x00, 0x01,
        0xC0, 0xA8, 0x00, 0xC7
    };
    uint8_t h[20];
    uint8_t pkt[115];
    int i, ok = 1;
    for (i = 0; i < 20; i++) h[i] = patron[i];
    /* 1. Vector conocido. */
    if (red_ip_cksum(h, 20) != 0xB861u) ok = 0;
    /* 2. Paquete completo valido (115 B). */
    for (i = 0; i < 20; i++) pkt[i] = patron[i];
    pkt[10] = 0xB8;
    pkt[11] = 0x61;
    for (i = 20; i < 115; i++) pkt[i] = 0;
    if (red_ip_valida(pkt, 115)) ok = 0;
    /* 3a. Cabecera corrupta -> se rechaza. */
    pkt[12] ^= 0xFF;
    if (!red_ip_valida(pkt, 115)) ok = 0;
    pkt[12] ^= 0xFF;
    /* 3b. TTL 0 (checksum recalculado para aislarlo) -> se rechaza. */
    for (i = 0; i < 20; i++) pkt[i] = patron[i];
    pkt[8] = 0;
    pkt[10] = 0;
    pkt[11] = 0;
    {
        uint16_t c = red_ip_cksum(pkt, 20);
        pkt[10] = (uint8_t)((c >> 8) & 0xFFu);
        pkt[11] = (uint8_t)(c & 0xFFu);
    }
    for (i = 20; i < 115; i++) pkt[i] = 0;
    if (!red_ip_valida(pkt, 115)) ok = 0;
    /* 3c. Total menor que cabecera -> se rechaza. */
    for (i = 0; i < 20; i++) pkt[i] = patron[i];
    pkt[2] = 0;
    pkt[3] = 10;
    for (i = 20; i < 115; i++) pkt[i] = 0;
    if (!red_ip_valida(pkt, 115)) ok = 0;
    /* 3d. Buffer mas corto que lo declarado -> se rechaza. */
    for (i = 0; i < 20; i++) pkt[i] = patron[i];
    pkt[10] = 0xB8;
    pkt[11] = 0x61;
    if (!red_ip_valida(pkt, 20)) ok = 0;
    kprintf("red: KAT ip %s\r\n", ok ? "OK" : "FALLO");
}
