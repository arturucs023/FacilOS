#include "ulib.h"

void __main(void) {
}

/* mem32 (v2.0): contabilidad de memoria de usuario y del nucleo.
 * - coherencia info clasica (totales == libres + usados),
 * - crecimiento con el primer uso del monton y pico > 0,
 * - reutilizacion sin crecimiento en el segundo ciclo,
 * - sbrk enorme rechazado,
 * - anomalias del nucleo (doble-free, fugas, incoherencias) a cero.
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
    unsigned int v[5], w0[8], w1[8], w2[8];
    int i;
    u_print("mem32 v2.0\n");
    ok(u_info_memoria(v) == 0, "info-ok");
    ok(v[0] == v[1] + v[2], "info-coherente");
    ok(u_info_memoria2(w0) == 0, "info2-ok");
    ok(w0[0] + w0[1] == v[0], "info2-coherente");
    u_print("libres=");
    u_print_num((int)w0[0]);
    u_print(" usados_usr=");
    u_print_num((int)w0[2]);
    u_print(" pico=");
    u_print_num((int)w0[3]);
    u_print("\n");
    /* primer ciclo: 200 bloques variados */
    {
        void *p[200];
        for (i = 0; i < 200; i++) {
            p[i] = u_malloc(8 + (i * 13) % 120);
            if (!p[i]) break;
            u_memset(p[i], i & 255, 8 + (i * 13) % 120);
        }
        ok(i == 200, "malloc200");
        ok(u_info_memoria2(w1) == 0, "info2-ciclo1");
        ok(w1[2] > w0[2], "crece-uso");
        ok(w1[3] >= w1[2] && w1[3] > 0, "pico-ok");
        for (i = 0; i < 200; i++) u_free(p[i]);
        /* segundo ciclo: debe reutilizar sin pedir mas marcos */
        for (i = 0; i < 200; i++) {
            p[i] = u_malloc(8 + (i * 13) % 120);
            if (!p[i]) break;
        }
        ok(i == 200, "malloc200-2");
        ok(u_info_memoria2(w2) == 0, "info2-ciclo2");
        ok(w2[2] == w1[2], "reuso-sin-crecer");
        for (i = 0; i < 200; i++) u_free(p[i]);
    }
    /* sbrk enorme: rechazado */
    ok(sys_memoria(64 * 1024 * 1024) < 0, "sbrk-enorme-no");
    /* anomalias del nucleo a cero */
    ok(u_info_memoria2(w2) == 0, "info2-fin");
    ok(w2[4] == 0, "sin-doble-free");
    ok(w2[5] == 0, "sin-anomalias");
    u_printf("mem32: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}
