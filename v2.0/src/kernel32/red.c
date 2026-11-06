#include "red.h"
#include "drv_red.h"
#include "red_arp.h"
#include "red_ip.h"
#include "red_icmp.h"
#include "libc.h"

/* v2.3-red F2: configuracion, rutas derivadas, deposito crudo y
 * demultiplexor de entrada. */

static red_cfg_t cfg;
static int cfg_ok = 0;

/* Deposito de diagnostico: todo lo entrante deja copia (tap). */
#define TAP_N 4
#define TAP_MAX 1516
static uint8_t tap[TAP_N][TAP_MAX];
static int tap_len[TAP_N];
static int tap_ini = 0, tap_fin = 0, tap_n = 0;
static uint32_t tap_tiradas = 0;

void red_nucleo_init(void) {
    int i;
    cfg.ip = 0;
    cfg.mascara = 0;
    cfg.puerta = 0;
    cfg.dns = 0;
    cfg_ok = 1;
    red_arp_init();
    for (i = 0; i < TAP_N; i++) tap_len[i] = 0;
    red_ip_kat();
    red_icmp_init();
    red_icmp_kat();
}

int red_cfg_fijar(int campo, uint32_t v) {
    uint32_t antes;
    if (campo < 1 || campo > 4) return -2;
    antes = cfg.ip;
    if (campo == 1) cfg.ip = v;
    else if (campo == 2) cfg.mascara = v;
    else if (campo == 3) cfg.puerta = v;
    else cfg.dns = v;
    if (campo == 1 && v != 0 && v != antes) red_arp_anuncia();
    return 0;
}

void red_cfg_leer(red_cfg_t *c) {
    c->ip = cfg.ip;
    c->mascara = cfg.mascara;
    c->puerta = cfg.puerta;
    c->dns = cfg.dns;
}

uint32_t red_cfg_ip(void) { return cfg.ip; }

int red_rutas(uint8_t *dst, int max) {
    int n = 0;
    /* Conectada (entrega directa) si hay IP+mascara. */
    if (cfg.ip && cfg.mascara && max >= 12) {
        uint32_t *w = (uint32_t *)dst;
        w[0] = cfg.ip & cfg.mascara;
        w[1] = cfg.mascara;
        w[2] = 0;
        n = 12;
        dst += 12;
        max -= 12;
    }
    /* Defecto via puerta. */
    if (cfg.puerta && max >= 12) {
        uint32_t *w = (uint32_t *)dst;
        w[0] = 0;
        w[1] = 0;
        w[2] = cfg.puerta;
        n += 12;
    }
    return n;
}

/* Devuelve 0 si debe ir por puerta (gw en *gw), 1 si directa,
   -1 si no hay ruta. */
int red_ruta_para(uint32_t dst, uint32_t *gw) {
    if (cfg.ip && cfg.mascara &&
        (dst & cfg.mascara) == (cfg.ip & cfg.mascara)) {
        *gw = 0;
        return 1;
    }
    if (cfg.puerta) {
        *gw = cfg.puerta;
        return 0;
    }
    return -1;
}

static void tap_mete(const uint8_t *m, int tam) {
    int i;
    if (tam > TAP_MAX) tam = TAP_MAX;
    if (tap_n == TAP_N) {
        tap_ini = (tap_ini + 1) % TAP_N;
        tap_n--;
        tap_tiradas++;
    }
    for (i = 0; i < tam; i++) tap[tap_fin][i] = m[i];
    tap_len[tap_fin] = tam;
    tap_fin = (tap_fin + 1) % TAP_N;
    tap_n++;
}

int red_hay_crudo(void) { return tap_n > 0 ? 1 : 0; }

int red_saca_crudo(uint8_t *dst, int max) {
    int n, i;
    if (!tap_n) return 0;
    n = tap_len[tap_ini];
    if (n > max) n = max;
    for (i = 0; i < n; i++) dst[i] = tap[tap_ini][i];
    tap_ini = (tap_ini + 1) % TAP_N;
    tap_n--;
    return n;
}

/* Entrada: EtherType ARP->arp (aprende/responde), IPv4->valida
   (F2: cuenta y deja al deposito; consumo real en F3+), resto al
   deposito tal cual. Todo lo entrante deja copia de diagnostico. */
static uint8_t bomba[1518];

static void red_entrada(const uint8_t *m, int tam) {
    if (tam >= 14) {
        unsigned eth = ((unsigned)m[12] << 8) | m[13];
        if (eth == 0x0806) red_arp_entrada(m, tam);
        else if (eth == 0x0800) red_ip_entrada(m, tam);
    }
    tap_mete(m, tam);
}

void red_bombea(void) {
    int n;
    for (;;) {
        n = red_rx(bomba, sizeof(bomba));
        if (n <= 0) return;
        red_entrada(bomba, n);
    }
}
