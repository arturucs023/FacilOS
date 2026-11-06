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
        /* Via negativa: enviar/recibir deben fallar limpio. */
        ok(u_red_trama_enviar(tx, 42) == -1, "tx-sin-adaptador");
        ok(u_red_trama_recibir(rx, sizeof(rx), 100) == -1,
           "rx-sin-adaptador");
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
