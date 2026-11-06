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

int main(void) {
    int r, i, n;
    int hallada = 0;
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
