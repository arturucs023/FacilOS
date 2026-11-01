#include "ulib.h"

void __main(void) {
}

/* adivina: numero 1-100, pistas mayor/menor, cuenta intentos. */

int main(void) {
    char lin[16];
    u_print("Adivina un numero del 1 al 100 (q sale).\n");
    for (;;) {
        int secreto = 1 + u_ticks() % 100;
        int n = 0;
        for (;;) {
            u_print("tu numero: ");
            u_lee_linea(lin, 16);
            if (lin[0] == 'q' || lin[0] == 'Q' || lin[0] == 0) return 0;
            {
                int v = u_atoi(lin);
                n++;
                if (v < 1 || v > 100) {
                    u_print("del 1 al 100.\n");
                } else if (v < secreto) {
                    u_print("mayor...\n");
                } else if (v > secreto) {
                    u_print("menor...\n");
                } else {
                    u_printf("HAS GANADO en %d intentos.\n", n);
                    break;
                }
            }
        }
        u_print("otra? (s/n): ");
        u_lee_linea(lin, 16);
        if (lin[0] != 's' && lin[0] != 'S') return 0;
    }
}
