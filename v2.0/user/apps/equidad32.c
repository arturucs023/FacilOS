#include "ulib.h"

void __main(void) {
}

/* equidad32 (v2.0): tres quemadores CPU-bound simultaneos deben repartirse
 * la CPU por igual (Round Robin). Comprueba max(ticks)/min(ticks) < 1.5 y
 * que todos terminan. Devuelve nº de fallos. */

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
    int p[3];
    unsigned int s[3][5];
    int i, rc[3];
    u_print("equidad32 v2.0\n");
    for (i = 0; i < 3; i++) {
        p[i] = u_crear("quema32", "200");
        if (p[i] <= 0) {
            u_print("no se pudo crear quemador\n");
            return 99;
        }
        u_print("quemador pid=");
        u_print_num(p[i]);
        u_print("\n");
    }
    /* mientras queman, muestrea dos veces con pausa */
    u_dormir(1000);
    for (i = 0; i < 3; i++) u_proc_stat(p[i], s[i]);
    u_print("ticks intermedios: ");
    for (i = 0; i < 3; i++) {
        u_print_num((int)s[i][1]);
        u_print(" ");
    }
    u_print("\n");
    u_dormir(1500);
    for (i = 0; i < 3; i++) u_proc_stat(p[i], s[i]);
    {
        unsigned int mn = s[0][1], mx = s[0][1];
        for (i = 1; i < 3; i++) {
            if (s[i][1] < mn) mn = s[i][1];
            if (s[i][1] > mx) mx = s[i][1];
        }
        u_print("ticks finales: ");
        for (i = 0; i < 3; i++) {
            u_print_num((int)s[i][1]);
            u_print(" ");
        }
        u_print("\n  estados: ");
        for (i = 0; i < 3; i++) {
            u_print_num((int)s[i][0]);
            u_print(" ");
        }
        u_print("\n");
        ok(mn > 0, "todos-avanzan");
        /* max/min < 1.5  <=>  2*max < 3*min */
        ok(mn > 0 && 2 * mx < 3 * mn, "equidad-1.5");
    }
    for (i = 0; i < 3; i++) rc[i] = u_esperar(p[i]);
    u_print("rc=");
    for (i = 0; i < 3; i++) {
        u_print_num(rc[i]);
        u_print(" ");
    }
    u_print("\n");
    ok(rc[0] >= 0 && rc[1] >= 0 && rc[2] >= 0, "todos-terminan");
    u_printf("equidad32: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}
