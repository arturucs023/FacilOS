#include "red_udp.h"
#include "red.h"
#include "red_ip.h"
#include "red_arp.h"
#include "drv_red.h"
#include "libc.h"
#include "irq.h"

/* v2.3-red F4: UDP propio. Colas circulares por liga con dueno pid;
   el productor (entrada) nunca sobrescribe pendientes: con cola
   llena rechaza lo nuevo. El consumidor saca en orden FIFO. */

typedef struct {
    uint8_t datos[UDP_COLA_DATOS];
    uint16_t len;      /* 0 = hueco libre */
    uint32_t src;
    uint16_t sport;
} udp_msg_t;

typedef struct {
    uint16_t puerto;   /* 0 = liga libre */
    uint32_t pid;      /* dueno */
    uint32_t uso;      /* ultimo uso (ticks): desalojo del mas viejo */
    udp_msg_t cola[UDP_COLA];
    int ini;           /* indice del mas antiguo pendiente */
    int n;             /* pendientes (0..UDP_COLA) */
} udp_liga_t;

static udp_liga_t ligas[UDP_LIGAS];
static uint32_t c_ok = 0, c_err = 0;
static uint16_t ultimo_efem = UDP_EFEM_INI;

void red_udp_init(void) {
    int i, j;
    for (i = 0; i < UDP_LIGAS; i++) {
        ligas[i].puerto = 0;
        ligas[i].pid = 0;
        ligas[i].uso = 0;
        ligas[i].ini = 0;
        ligas[i].n = 0;
        for (j = 0; j < UDP_COLA; j++) ligas[i].cola[j].len = 0;
    }
    ultimo_efem = UDP_EFEM_INI;
}

static int busca_liga(uint16_t puerto) {
    int i;
    for (i = 0; i < UDP_LIGAS; i++)
        if (ligas[i].puerto == puerto) return i;
    return -1;
}

/* Suma con pseudo-cabecera IPv4 (origen+destino en orden de red,
   cero+proto+longitud UDP) y valida o calcula el checksum. */
static uint16_t cks_udp(const uint8_t *src4, const uint8_t *dst4,
                        const uint8_t *u, int n) {
    uint32_t s = 0;
    int i;
    s += ((uint32_t)src4[0] << 8) | src4[1];
    s += ((uint32_t)src4[2] << 8) | src4[3];
    s += ((uint32_t)dst4[0] << 8) | dst4[1];
    s += ((uint32_t)dst4[2] << 8) | dst4[3];
    s += 17u;
    s += (uint32_t)n;
    for (i = 0; i + 1 < n; i += 2)
        s += ((uint32_t)u[i] << 8) | u[i + 1];
    if (i < n) s += (uint32_t)u[i] << 8;
    while (s >> 16) s = (s & 0xFFFFu) + (s >> 16);
    return (uint16_t)(~s & 0xFFFFu);
}

int red_udp_enviar(uint32_t dst, uint16_t dpuerto, uint16_t spuerto,
                   const uint8_t *datos, int n) {
    uint8_t d[UDP_CAB + UDP_TX_MAX];
    uint8_t cab_ip[20];
    uint32_t mio;
    int i;
    if (!dst || !dpuerto || !datos || n < 0 || n > UDP_TX_MAX)
        return -2;
    if (!red_hay()) return -1;
    mio = red_cfg_ip();
    if (!mio) return -2;
    d[0] = (uint8_t)((spuerto >> 8) & 0xFFu);
    d[1] = (uint8_t)(spuerto & 0xFFu);
    d[2] = (uint8_t)((dpuerto >> 8) & 0xFFu);
    d[3] = (uint8_t)(dpuerto & 0xFFu);
    d[4] = (uint8_t)(((8 + n) >> 8) & 0xFFu);
    d[5] = (uint8_t)((8 + n) & 0xFFu);
    d[6] = 0;
    d[7] = 0;
    for (i = 0; i < n; i++) d[8 + i] = datos[i];
    /* Pseudo-cabecera en orden de red (byte bajo = primer octeto,
       igual que en el cable). */
    cab_ip[0] = (uint8_t)(mio & 0xFFu);
    cab_ip[1] = (uint8_t)((mio >> 8) & 0xFFu);
    cab_ip[2] = (uint8_t)((mio >> 16) & 0xFFu);
    cab_ip[3] = (uint8_t)((mio >> 24) & 0xFFu);
    cab_ip[4] = (uint8_t)(dst & 0xFFu);
    cab_ip[5] = (uint8_t)((dst >> 8) & 0xFFu);
    cab_ip[6] = (uint8_t)((dst >> 16) & 0xFFu);
    cab_ip[7] = (uint8_t)((dst >> 24) & 0xFFu);
    {
        uint16_t c = cks_udp(cab_ip, cab_ip + 4, d, 8 + n);
        if (!c) c = 0xFFFFu;   /* 0 en IPv4 = "sin checksum" */
        d[6] = (uint8_t)((c >> 8) & 0xFFu);
        d[7] = (uint8_t)(c & 0xFFu);
    }
    {
        int k = red_ip_enviar(dst, 17, d, 8 + n, 64);
        if (k < 0) return k;
        return n;   /* lo pedido (la cabecera la pone el nucleo) */
    }
}

int red_udp_ligar(uint16_t puerto, uint32_t pid) {
    int i, libre = -1, vict = -1;
    uint32_t ant = 0;
    int primero = 1;
    /* Sin exigencia de adaptador: la tabla es logica pura (el KAT
       la ejercita sin hardware); la syscall 58 exige adaptador. */
    if (!pid) return -2;
    if (puerto) {
        i = busca_liga(puerto);
        if (i >= 0) {
            if (ligas[i].pid == pid) {
                ligas[i].uso = irq_ticks();
                return puerto;   /* idempotente */
            }
            return -6;           /* en uso por otro proceso */
        }
    } else {
        /* Efimero: recorre 49152..65535 sin colisiones. */
        int k;
        for (k = 0; k <= 65535 - UDP_EFEM_INI; k++) {
            ultimo_efem++;
            if (ultimo_efem < UDP_EFEM_INI) ultimo_efem = UDP_EFEM_INI;
            if (busca_liga(ultimo_efem) < 0) break;
        }
        if (busca_liga(ultimo_efem) >= 0) return -6;  /* sin hueco */
        puerto = ultimo_efem;
    }
    for (i = 0; i < UDP_LIGAS; i++) {
        if (!ligas[i].puerto) {
            libre = i;
            break;
        }
    }
    if (libre >= 0) {
        ligas[libre].puerto = puerto;
        ligas[libre].pid = pid;
        ligas[libre].uso = irq_ticks();
        ligas[libre].ini = 0;
        ligas[libre].n = 0;
        return puerto;
    }
    /* Tabla llena: desaloja la liga con cola VACIA mas antigua
       (preferencia al propio pid). Con pendientes no se toca. */
    for (i = 0; i < UDP_LIGAS; i++) {
        if (ligas[i].n) continue;
        if (ligas[i].pid == pid) {
            vict = i;
            break;
        }
    }
    if (vict < 0) {
        for (i = 0; i < UDP_LIGAS; i++) {
            if (ligas[i].n) continue;
            if (primero || ligas[i].uso < ant) {
                ant = ligas[i].uso;
                vict = i;
                primero = 0;
            }
        }
    }
    if (vict < 0) return -6;   /* todas con recepcion pendiente */
    ligas[vict].puerto = puerto;
    ligas[vict].pid = pid;
    ligas[vict].uso = irq_ticks();
    ligas[vict].ini = 0;
    ligas[vict].n = 0;
    return puerto;
}

int red_udp_sacar(uint16_t puerto, uint32_t pid, uint32_t *src,
                  uint16_t *sport, uint8_t *dst, int max) {
    int i;
    udp_msg_t *m;
    int copia;
    /* Sin exigencia de adaptador (ver ligar); la syscall 60 la pone. */
    if (!puerto || !dst || max < 8) return -2;
    i = busca_liga(puerto);
    if (i < 0) return -2;      /* puerto sin receptor */
    if (ligas[i].pid != pid) return -6;
    if (!ligas[i].n) return 0;
    m = &ligas[i].cola[ligas[i].ini];
    copia = m->len;
    if (copia > UDP_COLA_DATOS) copia = UDP_COLA_DATOS;
    if (copia > max - 8) copia = max - 8;
    if (src) *src = m->src;
    if (sport) *sport = m->sport;
    {
        int k;
        for (k = 0; k < copia; k++) dst[8 + k] = m->datos[k];
    }
    dst[0] = (uint8_t)(m->src & 0xFFu);
    dst[1] = (uint8_t)((m->src >> 8) & 0xFFu);
    dst[2] = (uint8_t)((m->src >> 16) & 0xFFu);
    dst[3] = (uint8_t)((m->src >> 24) & 0xFFu);
    dst[4] = (uint8_t)((m->sport >> 8) & 0xFFu);
    dst[5] = (uint8_t)(m->sport & 0xFFu);
    dst[6] = (uint8_t)((m->len >> 8) & 0xFFu);
    dst[7] = (uint8_t)(m->len & 0xFFu);
    m->len = 0;
    ligas[i].ini = (ligas[i].ini + 1) % UDP_COLA;
    ligas[i].n--;
    ligas[i].uso = irq_ticks();
    return 8 + copia;
}

int red_udp_limpia_proceso(uint32_t pid) {
    int i, n = 0, j;
    if (!pid) return 0;
    for (i = 0; i < UDP_LIGAS; i++) {
        if (ligas[i].puerto && ligas[i].pid == pid) {
            ligas[i].puerto = 0;
            ligas[i].pid = 0;
            ligas[i].ini = 0;
            ligas[i].n = 0;
            for (j = 0; j < UDP_COLA; j++) ligas[i].cola[j].len = 0;
            n++;
        }
    }
    return n;
}

void red_udp_entrada(const uint8_t *m, int tam) {
    int hlen, ulen, n;
    const uint8_t *ip, *u;
    uint16_t dp;
    int i;
    if (tam < 14 + 20 + 8) {
        c_err++;
        return;
    }
    ip = m + 14;
    hlen = (ip[0] & 0x0F) * 4;
    if (hlen < 20 || 14 + hlen + 8 > tam) {
        c_err++;
        return;
    }
    u = m + 14 + hlen;
    n = tam - 14 - hlen;
    ulen = (u[4] << 8) | u[5];
    /* Longitud coherente: 8 <= campo <= resto del datagrama. */
    if (ulen < 8 || ulen > n) {
        c_err++;
        return;
    }
    /* Checksum: campo 0 = sin checksum (se acepta); si no, la suma
       con pseudo-cabecera debe dar cero. */
    if (u[6] || u[7]) {
        uint8_t seudo[12];
        uint32_t s = 0;
        int k;
        for (k = 0; k < 4; k++) {
            seudo[k] = ip[12 + k];
            seudo[4 + k] = ip[16 + k];
        }
        seudo[8] = 0;
        seudo[9] = 17;
        seudo[10] = u[4];
        seudo[11] = u[5];
        s = red_ip_cksum(seudo, 12);
        s += red_ip_cksum(u, ulen);
        while (s >> 16) s = (s & 0xFFFFu) + (s >> 16);
        if ((uint16_t)(~s & 0xFFFFu) != 0) {
            c_err++;
            return;
        }
    }
    dp = (uint16_t)((u[2] << 8) | u[3]);
    i = busca_liga(dp);
    if (i < 0) {
        c_err++;   /* puerto sin receptor */
        return;
    }
    {
        int carga = ulen - 8;
        udp_msg_t *slot;
        if (carga > UDP_COLA_DATOS) {
            c_err++;   /* mayor que la cola: se descarta entero */
            return;
        }
        if (ligas[i].n >= UDP_COLA) {
            c_err++;   /* cola llena: se rechaza lo nuevo */
            return;
        }
        slot = &ligas[i].cola[(ligas[i].ini + ligas[i].n) % UDP_COLA];
        {
            int k;
            for (k = 0; k < carga; k++) slot->datos[k] = u[8 + k];
        }
        slot->len = (uint16_t)carga;
        slot->src = (uint32_t)ip[12] | ((uint32_t)ip[13] << 8) |
                    ((uint32_t)ip[14] << 16) | ((uint32_t)ip[15] << 24);
        slot->sport = (uint16_t)((u[0] << 8) | u[1]);
        ligas[i].n++;
        ligas[i].uso = irq_ticks();
        c_ok++;
    }
}

void red_udp_contadores(uint32_t *ok, uint32_t *err) {
    if (ok) *ok = c_ok;
    if (err) *err = c_err;
}

/* Trama UDP sintetica: eth(14) + ip(20) + udp(8) + carga. */
static int arma_udp(uint8_t *m, uint32_t src, uint16_t sp,
                    uint16_t dp, const uint8_t *carga, int n,
                    int cks_malo, int sin_cks, int len_mal) {
    int i, ulen = 8 + n;
    uint16_t c;
    for (i = 0; i < 14; i++) m[i] = 0;
    m[12] = 0x08;
    m[13] = 0x00;
    m[14] = 0x45;
    m[15] = 0;
    m[16] = (uint8_t)(((20 + ulen) >> 8) & 0xFFu);
    m[17] = (uint8_t)((20 + ulen) & 0xFFu);
    for (i = 18; i < 34; i++) m[i] = 0;
    m[14 + 8] = 64;
    m[14 + 9] = 17;
    m[14 + 12] = (uint8_t)(src & 0xFFu);
    m[14 + 13] = (uint8_t)((src >> 8) & 0xFFu);
    m[14 + 14] = (uint8_t)((src >> 16) & 0xFFu);
    m[14 + 15] = (uint8_t)((src >> 24) & 0xFFu);
    m[14 + 16] = 10;
    m[14 + 17] = 0;
    m[14 + 18] = 2;
    m[14 + 19] = 15;
    c = red_ip_cksum(m + 14, 20);
    m[14 + 10] = (uint8_t)((c >> 8) & 0xFFu);
    m[14 + 11] = (uint8_t)(c & 0xFFu);
    m[34] = (uint8_t)((sp >> 8) & 0xFFu);
    m[35] = (uint8_t)(sp & 0xFFu);
    m[36] = (uint8_t)((dp >> 8) & 0xFFu);
    m[37] = (uint8_t)(dp & 0xFFu);
    m[38] = (uint8_t)((ulen >> 8) & 0xFFu);
    m[39] = (uint8_t)(ulen & 0xFFu);
    m[40] = 0;
    m[41] = 0;
    for (i = 0; i < n; i++) m[42 + i] = carga[i];
    if (!sin_cks) {
        uint8_t seudo[12];
        int k;
        for (k = 0; k < 4; k++) {
            seudo[k] = m[14 + 12 + k];
            seudo[4 + k] = m[14 + 16 + k];
        }
        seudo[8] = 0;
        seudo[9] = 17;
        seudo[10] = m[38];
        seudo[11] = m[39];
        c = cks_udp(seudo, seudo + 4, m + 34, ulen);
        if (!c) c = 0xFFFFu;
        if (cks_malo) c ^= 0xFFFFu;
        m[40] = (uint8_t)((c >> 8) & 0xFFu);
        m[41] = (uint8_t)(c & 0xFFu);
    }
    if (len_mal) {
        m[38] = 0xFF;
        m[39] = 0xFF;
    }
    return 14 + 20 + ulen;
}

void red_udp_kat(void) {
    static uint8_t m[1600];
    uint8_t carga[16];
    uint8_t sal[8 + 64];
    uint32_t src, e0, e1;
    uint16_t sport;
    int i, ok = 1;
    for (i = 0; i < 16; i++) carga[i] = (uint8_t)(0xC0 + i);
    /* 1. Valido con checksum: encola y saca en orden. */
    if (red_udp_ligar(61000, 9999) != 61000) ok = 0;
    if (red_udp_ligar(61000, 9999) != 61000) ok = 0;  /* idempotente */
    if (red_udp_ligar(61000, 7777) != -6) ok = 0;     /* otro pid */
    arma_udp(m, 0x0202000Au, 53, 61000, carga, 16, 0, 0, 0);
    red_udp_entrada(m, 14 + 20 + 8 + 16);
    red_udp_contadores(&e0, 0);
    if (e0 != 1) ok = 0;
    if (red_udp_sacar(61000, 7777, 0, 0, sal, sizeof(sal)) != -6) ok = 0;
    if (red_udp_sacar(61000, 9999, &src, &sport, sal,
                      (int)sizeof(sal)) != 8 + 16)
        ok = 0;
    if (src != 0x0202000Au || sport != 53) ok = 0;
    if (sal[6] != 0 || sal[7] != 16) ok = 0;
    for (i = 0; i < 16; i++)
        if (sal[8 + i] != carga[i]) ok = 0;
    if (red_udp_sacar(61000, 9999, 0, 0, sal, sizeof(sal)) != 0) ok = 0;
    /* 2. Checksum incorrecto / cero / longitudes. */
    red_udp_contadores(0, &e0);
    arma_udp(m, 0x0202000Au, 53, 61000, carga, 16, 1, 0, 0);
    red_udp_entrada(m, 14 + 20 + 8 + 16);
    arma_udp(m, 0x0202000Au, 53, 61000, carga, 16, 0, 1, 0);
    red_udp_entrada(m, 14 + 20 + 8 + 16);
    arma_udp(m, 0x0202000Au, 53, 61000, carga, 16, 0, 0, 1);
    red_udp_entrada(m, 14 + 20 + 8 + 16);
    red_udp_entrada(m, 10);
    red_udp_contadores(0, &e1);
    if (e1 != e0 + 3) ok = 0;   /* malo + campo-len + corta */
    /* la de checksum cero si entro: sacarla para dejar cola limpia */
    red_udp_sacar(61000, 9999, 0, 0, sal, sizeof(sal));
    /* 3. Sin receptor y cola llena (rechaza lo nuevo, no corrompe). */
    arma_udp(m, 0x0202000Au, 53, 62000, carga, 4, 0, 0, 0);
    red_udp_contadores(0, &e0);
    red_udp_entrada(m, 14 + 20 + 8 + 4);
    red_udp_contadores(0, &e1);
    if (e1 != e0 + 1) ok = 0;
    for (i = 0; i < UDP_COLA; i++) {
        arma_udp(m, 0x0202000Au, 53, 61000, carga, 4, 0, 0, 0);
        red_udp_entrada(m, 14 + 20 + 8 + 4);
    }
    red_udp_contadores(&e0, &e1);
    arma_udp(m, 0x0202000Au, 53, 61000, carga, 4, 0, 0, 0);
    red_udp_entrada(m, 14 + 20 + 8 + 4);
    {
        uint32_t o1, r1;
        red_udp_contadores(&o1, &r1);
        if (o1 != e0 || r1 != e1 + 1) ok = 0;  /* 5o descartado */
    }
    /* orden FIFO de los 4 y el 5o descartado */
    for (i = 0; i < UDP_COLA; i++) {
        if (red_udp_sacar(61000, 9999, 0, 0, sal, sizeof(sal)) != 8 + 4)
            ok = 0;
    }
    if (red_udp_sacar(61000, 9999, 0, 0, sal, sizeof(sal)) != 0) ok = 0;
    red_udp_contadores(0, &e1);
    /* 4. Desalojo: tabla llena con pendientes no se toca. */
    if (red_udp_ligar(61001, 9999) != 61001) ok = 0;
    if (red_udp_ligar(61002, 9999) != 61002) ok = 0;
    if (red_udp_ligar(61003, 9999) != 61003) ok = 0;
    for (i = 0; i < UDP_LIGAS; i++) {
        arma_udp(m, 0x0202000Au, 53, (uint16_t)(61000 + i), carga, 4,
                 0, 0, 0);
        red_udp_entrada(m, 14 + 20 + 8 + 4);
    }
    if (red_udp_ligar(61004, 9999) != -6) ok = 0;  /* todas ocupadas */
    red_udp_sacar(61000, 9999, 0, 0, sal, sizeof(sal));
    if (red_udp_ligar(61004, 9999) != 61004) ok = 0;  /* vacia: entra */
    if (red_udp_sacar(61000, 9999, 0, 0, sal, sizeof(sal)) != -2) ok = 0;
    /* limpieza del KAT */
    red_udp_limpia_proceso(9999);
    if (red_udp_ligar(0, 9999) < UDP_EFEM_INI) ok = 0;
    red_udp_limpia_proceso(9999);
    kprintf("red: KAT udp %s\r\n", ok ? "OK" : "FALLO");
}
