#include "red_ip.h"
#include "red.h"
#include "red_arp.h"
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
    /* F2: valida y cuenta; el consumo real llega en F3+. */
    if (tam < 14 + 20) {
        ip_rx_err++;
        return;
    }
    if (red_ip_valida(m + 14, tam - 14)) {
        ip_rx_err++;
        return;
    }
    ip_rx_ok++;
}

void red_ip_kat(void) {
    /* Vector estilo RFC 791 (TTL 64, proto 17,
       192.168.0.1 -> 192.168.0.199, len 115): checksum B861. */
    static const uint8_t bueno[20] = {
        0x45, 0x00, 0x00, 0x73, 0x00, 0x00, 0x40, 0x00,
        0x40, 0x11, 0xB8, 0x61, 0xC0, 0xA8, 0x00, 0x01,
        0xC0, 0xA8, 0x00, 0xC7
    };
    uint8_t h[20];
    int i, ok = 1;
    for (i = 0; i < 20; i++) h[i] = bueno[i];
    h[10] = 0;
    h[11] = 0;
    if (red_ip_cksum(h, 20) != 0xB861u) ok = 0;
    if (red_ip_valida(bueno, 20)) ok = 0;
    /* Rechazos: checksum roto, TTL 0, IHL<5, total<ihl. */
    for (i = 0; i < 20; i++) h[i] = bueno[i];
    h[12] ^= 0xFF;
    if (!red_ip_valida(h, 20)) ok = 0;
    for (i = 0; i < 20; i++) h[i] = bueno[i];
    h[8] = 0;   /* TTL agotado (checksum recalculado para aislarlo) */
    h[10] = 0;
    h[11] = 0;
    {
        uint16_t c = red_ip_cksum(h, 20);
        h[10] = (uint8_t)((c >> 8) & 0xFFu);
        h[11] = (uint8_t)(c & 0xFFu);
    }
    if (!red_ip_valida(h, 20)) ok = 0;
    for (i = 0; i < 20; i++) h[i] = bueno[i];
    h[0] = 0x44;
    if (!red_ip_valida(h, 20)) ok = 0;
    for (i = 0; i < 20; i++) h[i] = bueno[i];
    h[2] = 0;
    h[3] = 10;
    if (!red_ip_valida(h, 20)) ok = 0;
    kprintf("red: KAT ip %s\r\n", ok ? "OK" : "FALLO");
}
