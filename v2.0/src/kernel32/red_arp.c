#include "red_arp.h"
#include "red.h"
#include "drv_red.h"
#include "libc.h"
#include "irq.h"
#include "proc.h"

/* v2.3-red F2: ARP. Aprende de toda solicitud/respuesta ajena;
   responde solicitudes para nuestra IP; resuelve con espera. */

#define ARP_N 16
#define ARP_CADUCA (60u * 100u)   /* 60 s en ticks de 100 Hz */

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint32_t ticks;
    int valida;
} arp_e_t;

static arp_e_t cache[ARP_N];

void red_arp_init(void) {
    int i;
    for (i = 0; i < ARP_N; i++) cache[i].valida = 0;
}

static int mac_propia(const uint8_t *m) {
    uint8_t yo[6];
    int i;
    if (red_mac(yo)) return 0;
    for (i = 0; i < 6; i++)
        if (m[i] != yo[i]) return 0;
    return 1;
}

static void aprende(uint32_t ip, const uint8_t *mac) {
    int i, viejo = 0;
    uint32_t ant = 0;
    if (!ip) return;
    for (i = 0; i < ARP_N; i++) {
        if (cache[i].valida && cache[i].ip == ip) {
            int k;
            for (k = 0; k < 6; k++) cache[i].mac[k] = mac[k];
            cache[i].ticks = irq_ticks();
            return;
        }
    }
    for (i = 0; i < ARP_N; i++) {
        if (!cache[i].valida) {
            int k;
            cache[i].ip = ip;
            for (k = 0; k < 6; k++) cache[i].mac[k] = mac[k];
            cache[i].ticks = irq_ticks();
            cache[i].valida = 1;
            return;
        }
        if (i == 0 || cache[i].ticks < ant) {
            ant = cache[i].ticks;
            viejo = i;
        }
    }
    {
        int k;
        cache[viejo].ip = ip;
        for (k = 0; k < 6; k++) cache[viejo].mac[k] = mac[k];
        cache[viejo].ticks = irq_ticks();
        cache[viejo].valida = 1;
    }
}

static int busca(uint32_t ip, uint8_t *mac) {
    int i;
    uint32_t ahora = irq_ticks();
    for (i = 0; i < ARP_N; i++) {
        if (cache[i].valida && cache[i].ip == ip) {
            if (ahora - cache[i].ticks >= ARP_CADUCA) {
                cache[i].valida = 0;
                return 0;
            }
            {
                int k;
                for (k = 0; k < 6; k++) mac[k] = cache[i].mac[k];
            }
            return 1;
        }
    }
    return 0;
}

/* Construye solicitud ARP (difusion) en buf (42 B). */
static void haz_peticion(uint8_t *b, const uint8_t *mac,
                         uint32_t spa, uint32_t tpa) {
    int i;
    for (i = 0; i < 6; i++) {
        b[i] = 0xFF;
        b[i + 6] = mac[i];
    }
    b[12] = 0x08;
    b[13] = 0x06;
    b[14] = 0x00;
    b[15] = 0x01;
    b[16] = 0x08;
    b[17] = 0x00;
    b[18] = 6;
    b[19] = 4;
    b[20] = 0x00;
    b[21] = 0x01;
    for (i = 0; i < 6; i++) b[22 + i] = mac[i];
    b[28] = (uint8_t)(spa & 0xFFu);
    b[29] = (uint8_t)((spa >> 8) & 0xFFu);
    b[30] = (uint8_t)((spa >> 16) & 0xFFu);
    b[31] = (uint8_t)((spa >> 24) & 0xFFu);
    for (i = 0; i < 6; i++) b[32 + i] = 0;
    b[38] = (uint8_t)(tpa & 0xFFu);
    b[39] = (uint8_t)((tpa >> 8) & 0xFFu);
    b[40] = (uint8_t)((tpa >> 16) & 0xFFu);
    b[41] = (uint8_t)((tpa >> 24) & 0xFFu);
}

void red_arp_entrada(const uint8_t *m, int tam) {
    uint16_t hw, pr;
    uint8_t hl, pl;
    uint16_t op;
    uint32_t spa, tpa;
    uint8_t yo[6];
    int i;
    hw = (uint16_t)((m[14] << 8) | m[15]);
    pr = (uint16_t)((m[16] << 8) | m[17]);
    hl = m[18];
    pl = m[19];
    if (hw != 1 || pr != 0x0800 || hl != 6 || pl != 4) return;
    op = (uint16_t)((m[20] << 8) | m[21]);
    /* Ignora tramas propias (evita autoaprendizaje en bucle). */
    for (i = 0; i < 6; i++) yo[i] = m[6 + i];
    if (mac_propia(yo)) return;
    spa = (uint32_t)m[28] | ((uint32_t)m[29] << 8) |
          ((uint32_t)m[30] << 16) | ((uint32_t)m[31] << 24);
    tpa = (uint32_t)m[38] | ((uint32_t)m[39] << 8) |
          ((uint32_t)m[40] << 16) | ((uint32_t)m[41] << 24);
    aprende(spa, m + 22);
    if (op == 1) {
        uint32_t mia = red_cfg_ip();
        /* Responde solo a solicitudes para nuestra IP. */
        if (mia && tpa == mia && red_hay()) {
            uint8_t r[42];
            uint8_t mac[6];
            if (red_mac(mac)) return;
            for (i = 0; i < 6; i++) {
                r[i] = m[22 + i];      /* destino: quien pregunta */
                r[i + 6] = mac[i];
            }
            r[12] = 0x08;
            r[13] = 0x06;
            r[14] = 0x00;
            r[15] = 0x01;
            r[16] = 0x08;
            r[17] = 0x00;
            r[18] = 6;
            r[19] = 4;
            r[20] = 0x00;
            r[21] = 0x02;              /* respuesta */
            for (i = 0; i < 6; i++) r[22 + i] = mac[i];
            r[28] = (uint8_t)(mia & 0xFFu);
            r[29] = (uint8_t)((mia >> 8) & 0xFFu);
            r[30] = (uint8_t)((mia >> 16) & 0xFFu);
            r[31] = (uint8_t)((mia >> 24) & 0xFFu);
            for (i = 0; i < 6; i++) r[32 + i] = m[22 + i];
            r[38] = (uint8_t)(spa & 0xFFu);
            r[39] = (uint8_t)((spa >> 8) & 0xFFu);
            r[40] = (uint8_t)((spa >> 16) & 0xFFu);
            r[41] = (uint8_t)((spa >> 24) & 0xFFu);
            red_tx(r, 42);
        }
    }
}

int red_arp_resolver(uint32_t ip, int espera_ms, uint8_t *mac) {
    uint8_t yo[6];
    uint8_t pet[42];
    uint32_t fin;
    int tr;
    if (!red_hay()) return -1;
    if (!mac || !ip) return -2;
    if (espera_ms < 0) espera_ms = 0;
    if (espera_ms > 10000) espera_ms = 10000;
    if (busca(ip, mac)) return 0;
    if (red_mac(yo)) return -1;
    haz_peticion(pet, yo, red_cfg_ip(), ip);
    tr = red_tx(pet, 42);
    if (tr != 42) return -1;
    fin = irq_ticks() + (uint32_t)espera_ms / 10u + 1u;
    {
        uint32_t ultimo_tx = irq_ticks();
        for (;;) {
            uint32_t ahora;
            red_bombea();
            if (busca(ip, mac)) return 0;   /* la respuesta ya esta */
            ahora = irq_ticks();
            if ((int32_t)(ahora - fin) >= 0) return 1;
            if ((int32_t)(ahora - ultimo_tx) >= 50) {
                /* Reintento cada 0,5 s (la primera puede perderse);
                   espera total sigue acotada por `fin`. */
                if (red_tx(pet, 42) == 42) ultimo_tx = ahora;
            }
            proc_actual->modo = 0;
            proc_bloquea_ticks(irq_ticks() + 2u);
        }
    }
}

int red_arp_mira(uint32_t ip, uint8_t *mac) {
    if (!ip || !mac) return 0;
    return busca(ip, mac);
}

void red_arp_anuncia(void) {
    uint8_t yo[6];
    uint8_t pet[42];
    uint32_t mia = red_cfg_ip();
    if (!mia || !red_hay()) return;
    if (red_mac(yo)) return;
    /* Gratuito estilo sonda: spa==tpa==nuestra IP. */
    haz_peticion(pet, yo, mia, mia);
    red_tx(pet, 42);
}

int red_arp_n(void) {
    int i, n = 0;
    for (i = 0; i < ARP_N; i++)
        if (cache[i].valida) n++;
    return n;
}
