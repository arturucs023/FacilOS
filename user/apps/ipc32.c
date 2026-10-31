#include "ulib.h"

void __main(void) {
}

/* ipc32 (v2.0): 50 ping-pong con eco verificado (orden, contenido, emisor),
 * buzon lleno (-2) contra un quemador ocupado, y cierre limpio.
 * Devuelve nº de fallos. */

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

int main(void) {
    char b[64];
    int de = 0, i, h;
    u_print("ipc32 v2.0\n");
    h = u_crear("ipc_hijo", "");
    ok(h > 0, "crear-hijo");
    if (h <= 0) return 99;
    for (i = 0; i < 50; i++) {
        char m[16];
        int r, j = 0;
        m[j++] = 'p';
        m[j++] = '0' + (i / 10) % 10;
        m[j++] = '0' + i % 10;
        m[j] = 0;
        ok(u_ipc_enviar(h, m, 4) == 0, "enviar");
        r = u_ipc_recibir(b, 64, &de);
        ok(r == 4 && de == h && b[0] == 'p' && b[1] == m[1] && b[2] == m[2],
           "eco");
        if (r != 4 || de != h) break;
    }
    ok(i == 50, "cincuenta-ecos");
    /* buzon lleno: 16 entran, el 17 debe dar -2 (quemador no lee) */
    {
        int q = u_crear("quema32", "200");
        int r17 = -99, k;
        ok(q > 0, "crear-quemador");
        for (k = 0; k < 16; k++) {
            if (u_ipc_enviar(q, "x", 2)) break;
        }
        ok(k == 16, "dieciseis-cola");
        r17 = u_ipc_enviar(q, "x", 2);
        ok(r17 == -2, "lleno--2");
        ok(u_esperar(q) >= 0, "quemador-fin");
    }
    ok(u_ipc_enviar(h, "fin", 4) == 0, "enviar-fin");
    ok(u_esperar(h) == 50, "hijo-50");
    /* a proceso terminado: -1 */
    ok(u_ipc_enviar(h, "x", 2) == -1, "enviar-finado");
    ok(u_ipc_enviar(9999, "x", 2) == -1, "enviar-noexiste");
    ok(u_ipc_enviar(h, (const void *)0x100, 4) == -1, "enviar-puntero");
    u_printf("ipc32: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}
