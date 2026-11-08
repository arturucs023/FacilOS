#include "red_icmp.h"
#include "red.h"
#include "red_ip.h"
#include "red_arp.h"
#include "nim.h"
#include "drv_red.h"
#include "libc.h"
#include "irq.h"
#include "proc.h"

/* v2.3-red F3: eco ICMP. Una espera pendiente como maximo. */

static uint32_t c_ok = 0, c_err = 0;

static int hay_pend = 0;
static uint32_t pend_ip;
static uint16_t pend_ident, pend_seq;
static uint32_t pend_t0;
static int pend_ok = 0;
static uint16_t pend_rtt = 0;
static uint8_t pend_ttl = 0;

void red_icmp_init(void) {
    hay_pend = 0;
    pend_ok = 0;
}

/* Checksum sobre el mensaje ICMP (misma suma que IP). */
static uint16_t cks_icmp(const uint8_t *m, int n) {
    return red_ip_cksum(m, n);
}

void red_icmp_entrada(const uint8_t *m, int tam) {
    int hlen, ilen, i;
    const uint8_t *ip, *ic;
    uint16_t ident, seq;
    uint32_t src;
    if (tam < 14 + 20 + 8) { c_err++; return; }
    ip = m + 14;
    hlen = (ip[0] & 0x0F) * 4;
    if (hlen < 20 || 14 + hlen + 8 > tam) { c_err++; return; }
    ic = m + 14 + hlen;
    ilen = tam - 14 - hlen;
    if (red_ip_cksum(ic, ilen) != 0) { c_err++; return; }
    if (ic[0] == 0) {
        /* Respuesta: casa con la espera pendiente. */
        src = (uint32_t)ip[12] | ((uint32_t)ip[13] << 8) |
              ((uint32_t)ip[14] << 16) | ((uint32_t)ip[15] << 24);
        ident = (uint16_t)((ic[4] << 8) | ic[5]);
        seq = (uint16_t)((ic[6] << 8) | ic[7]);
        if (hay_pend && src == pend_ip && ident == pend_ident &&
            seq == pend_seq) {
            uint32_t dt = irq_ticks() - pend_t0;
            pend_rtt = (uint16_t)(dt * 10u > 65535u ? 65535u : dt * 10u);
            pend_ttl = ip[8];
            pend_ok = 1;
            c_ok++;
        }
        return;
    }
    if (ic[0] != 8 || ic[1] != 0) return;   /* solo eco en F3 */
    /* Solicitud a nosotros: responde. */
    {
        uint32_t mia = red_cfg_ip();
        uint32_t dst;
        uint8_t mac_src[6], mac_dst[6];
        uint8_t rep[14 + 20 + 64];
        int rl = 14 + hlen + ilen;
        if (rl > (int)sizeof(rep)) return;
        if (!mia) return;
        dst = (uint32_t)ip[16] | ((uint32_t)ip[17] << 8) |
              ((uint32_t)ip[18] << 16) | ((uint32_t)ip[19] << 24);
        if (dst != mia) return;
        for (i = 0; i < 6; i++) {
            mac_src[i] = m[6 + i];
            mac_dst[i] = m[i];
            rep[i] = mac_src[i];
            rep[i + 6] = mac_dst[i];
        }
        rep[12] = 0x08;
        rep[13] = 0x00;
        {
            uint8_t cab[20];
            for (i = 0; i < 20; i++) cab[i] = 0;
            cab[0] = 0x45;
            cab[2] = (uint8_t)(((hlen + ilen) >> 8) & 0xFFu);
            cab[3] = (uint8_t)((hlen + ilen) & 0xFFu);
            cab[8] = 64;
            cab[9] = 1;
            for (i = 0; i < 4; i++) cab[12 + i] = ip[16 + i];
            for (i = 0; i < 4; i++) cab[16 + i] = ip[12 + i];
            {
                uint16_t c = red_ip_cksum(cab, 20);
                cab[10] = (uint8_t)((c >> 8) & 0xFFu);
                cab[11] = (uint8_t)(c & 0xFFu);
            }
            for (i = 0; i < 20; i++) rep[14 + i] = cab[i];
        }
        for (i = 0; i < ilen; i++) rep[14 + 20 + i] = ic[i];
        rep[14 + 20] = 0;   /* type: respuesta */
        rep[14 + 21] = 0;
        rep[14 + 22] = 0;
        rep[14 + 23] = 0;
        {
            uint16_t c = red_ip_cksum(rep + 14 + 20, ilen);
            rep[14 + 22] = (uint8_t)((c >> 8) & 0xFFu);
            rep[14 + 23] = (uint8_t)(c & 0xFFu);
        }
        if (nim_tx(0, rep, rl) > 0) c_ok++;
        else c_err++;
    }
}

int red_icmp_ping(uint32_t dst, uint16_t ident, uint16_t seq,
                  uint8_t ttl, uint32_t espera_ms,
                  uint16_t *rtt_ms, uint8_t *pttl) {
    uint8_t msg[8 + 32];
    int i, n = 8 + 32;
    uint32_t fin;
    if (!dst) return -2;
    if (espera_ms > 10000) espera_ms = 10000;
    msg[0] = 8;
    msg[1] = 0;
    msg[2] = 0;
    msg[3] = 0;
    msg[4] = (uint8_t)((ident >> 8) & 0xFFu);
    msg[5] = (uint8_t)(ident & 0xFFu);
    msg[6] = (uint8_t)((seq >> 8) & 0xFFu);
    msg[7] = (uint8_t)(seq & 0xFFu);
    for (i = 8; i < n; i++) msg[i] = (uint8_t)(0xA0 + (i & 15));
    {
        uint16_t c = red_ip_cksum(msg, n);
        msg[2] = (uint8_t)((c >> 8) & 0xFFu);
        msg[3] = (uint8_t)(c & 0xFFu);
    }
    hay_pend = 1;
    pend_ip = dst;
    pend_ident = ident;
    pend_seq = seq;
    pend_t0 = irq_ticks();
    pend_ok = 0;
    pend_rtt = 0;
    {
        int k = red_ip_enviar(dst, 1, msg, n, ttl ? ttl : 64);
        if (k < 0) {
            hay_pend = 0;
            return k;   /* propaga: -1 sin adaptador, -3 sin ruta,
                           -4 ARP agotado, -5 fallo Tx */
        }
    }
    fin = irq_ticks() + espera_ms / 10u + 1u;
    for (;;) {
        red_bombea();
        if (pend_ok) {
            hay_pend = 0;
            if (rtt_ms) *rtt_ms = pend_rtt;
            if (pttl) *pttl = pend_ttl;
            return 1;
        }
        if ((int32_t)(irq_ticks() - fin) >= 0) {
            hay_pend = 0;
            return 0;
        }
        proc_actual->modo = 0;
        proc_bloquea_ticks(irq_ticks() + 2u);
    }
}

void red_icmp_contadores(uint32_t *ok, uint32_t *err) {
    if (ok) *ok = c_ok;
    if (err) *err = c_err;
}

void red_icmp_kat(void) {
    /* Eco request de 40 B con checksum conocido. */
    uint8_t m[40];
    int i, ok = 1;
    uint16_t c;
    for (i = 0; i < 40; i++) m[i] = 0;
    m[0] = 8;
    m[4] = 0x12;
    m[5] = 0x34;
    m[6] = 0x00;
    m[7] = 0x07;
    for (i = 8; i < 40; i++) m[i] = (uint8_t)(0xA0 + (i & 15));
    c = red_ip_cksum(m, 40);
    m[2] = (uint8_t)((c >> 8) & 0xFFu);
    m[3] = (uint8_t)(c & 0xFFu);
    if (red_ip_cksum(m, 40) != 0) ok = 0;
    m[10] ^= 0xFF;
    if (red_ip_cksum(m, 40) == 0) ok = 0;
    m[10] ^= 0xFF;
    {
        static uint8_t corto[14 + 20 + 4];
        uint32_t e0, e1;
        int i2;
        for (i2 = 0; i2 < (int)sizeof(corto); i2++) corto[i2] = 0;
        red_icmp_contadores(0, &e0);
        red_icmp_entrada(corto, (int)sizeof(corto));
        red_icmp_contadores(0, &e1);
        if (e1 != e0 + 1) ok = 0;
    }
    kprintf("red: KAT icmp %s\r\n", ok ? "OK" : "FALLO");
}
