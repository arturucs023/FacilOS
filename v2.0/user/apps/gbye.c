#include "ulib.h"

void __main(void) {
}

/* gbye (etapa 6, Fase O/R): cliente que muere SIN BYE (salida sucia).
 * Registra, confirma el primer DRAW y termina de golpe. El servidor
 * debe liberar su ventana (barre_muertos) sin colgarse. Arg = srv pid.
 * Uso: desde terminal `ejecutar gbye <srv>` (pid visible en sysinfo). */

int main(void) {
    const char *a = u_args();
    int srv = 0, id = -1;
    char b[64];
    int de = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    if (u_ipc_enviar(srv, "REG 200 100 gbye", 16)) return 98;
    for (;;) {
        int r = u_ipc_recibir(b, 64, &de);
        int k;
        if (r < 0) return 97;
        if (de != srv) continue;
        if (r >= 2 && b[0] == 'I' && b[1] == 'D') {
            k = 2;
            while (k < r && (b[k] < '0' || b[k] > '9')) k++;
            id = u_atoi(b + k);
        } else if (r >= 4 && b[0] == 'D' && b[1] == 'R') {
            u_texto(0, 0, "", C_NEGRO);
            u_rectangulo(0, 0, 200, 100, C_MAGENTA);
            u_presentar(0, 0, 200, 100);
            u_ipc_enviar(srv, "ACK", 4);
            sys_salir(0);   /* sucio: sin BYE */
        } else if (r >= 4 && b[0] == 'Q' && b[1] == 'U') {
            return 0;
        }
        (void)id;
    }
}
