#include "ulib.h"

void __main(void) {
}

/* planificador32 (v1.9): comprueba que hay multitarea REAL.
 *
 * Crea dos procesos contadores independientes (contador32) y uno que
 * duerme (dormir32), y mientras siguen vivos va refrescando la tabla de
 * procesos y el estado de los contadores leidos de un fichero.
 *
 *   1 = crea contadores, 2 = crea el que duerme, 3 = matar/recrear
 *   r = reinicia la prueba, ESC = salir
 *
 * Cada contador escribe su cuenta en un fichero propio y lo cierra, de
 * modo que el padre ve el avance sin necesidad de compartir memoria.
 */

static void uso(void) {
    u_print("\r\nplanificador32 v1.9\r\n");
    u_print("  1 crear dos contadores   2 crear uno que duerme\r\n");
    u_print("  3 repetir 20 veces (crear y esperar)\r\n");
    u_print("  r reiniciar  ESC salir\r\n");
}

static int crea(const char *prog, const char *args) {
    int pid = u_crear(prog, args);
    u_print("crear ");
    u_print(prog);
    if (args[0]) { u_print(" "); u_print(args); }
    u_print(" -> ");
    u_print_num(pid);
    u_print("\r\n");
    return pid;
}

static int lee_cont(const char *fich) {
    UFILE *f = u_fopen(fich, "r");
    char b[32];
    int v = -1;
    if (!f) return -1;
    int n = u_fread(b, 15, f);
    if (n > 0) { b[n] = 0; v = u_atoi(b); }
    u_fclose(f);
    return v;
}

int main(void) {
    int p1 = 0, p2 = 0, p3 = 0;
    uso();
    for (;;) {
        int t = u_tecla();
        if (t == 27) break;
        if (t == 'r') {
            p1 = p2 = p3 = 0;
            u_print("reiniciado\r\n");
            continue;
        }
        if (t == '1') {
            p1 = crea("contador32", "A");
            p2 = crea("contador32", "B");
        } else if (t == '2') {
            p3 = crea("dormir32", "");
        } else if (t == '3') {
            int rc = 0;
            for (int i = 0; i < 20; i++) {
                int pid = u_crear("contador32", "X");
                if (pid < 0) { rc = pid; break; }
                rc = u_esperar(pid);
            }
            u_print("20 procesos creados y esperados, ultimo rc=");
            u_print_num(rc);
            u_print("\r\n");
        } else {
            continue;
        }
        /* Muestra el avance mientras los hijos siguen vivos */
        for (int k = 0; k < 6; k++) {
            u_dormir(250);
            int a = lee_cont("cntA"), b = lee_cont("cntB");
            uproc_t p[12];
            int n = u_listar(p, 12);
            u_print("  A=");
            u_print_num(a);
            u_print(" B=");
            u_print_num(b);
            u_print("  procesos:");
            for (int i = 0; i < n; i++) {
                u_print(" ");
                u_print(p[i].nombre);
                u_print("=");
                u_print(u_estado_nombre(p[i].estado));
            }
            u_print("\r\n");
        }
        if (p1 > 0) { u_print("esperando p1 rc="); u_print_num(u_esperar(p1)); u_print("\r\n"); }
        if (p2 > 0) { u_print("esperando p2 rc="); u_print_num(u_esperar(p2)); u_print("\r\n"); }
        if (p3 > 0) { u_print("esperando p3 rc="); u_print_num(u_esperar(p3)); u_print("\r\n"); }
        p1 = p2 = p3 = 0;
    }
    u_print("planificador32 terminado\r\n");
    return 0;
}
