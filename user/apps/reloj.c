#include "ulib.h"

void __main(void) {
}

/* reloj: tiempo desde el arranque (ticks 100 Hz). q/ESC sale. */

static void dos(char *b, int v) {
    b[0] = (char)('0' + (v / 10) % 10);
    b[1] = (char)('0' + v % 10);
    b[2] = 0;
}

int main(void) {
    char hh[3], mm[3], ss[3], hora[9];
    for (;;) {
        int t = u_ticks() / 100;
        int h = (t / 3600) % 100, m = (t / 60) % 60, s = t % 60;
        dos(hh, h);
        dos(mm, m);
        dos(ss, s);
        hora[0] = hh[0];
        hora[1] = hh[1];
        hora[2] = ':';
        hora[3] = mm[0];
        hora[4] = mm[1];
        hora[5] = ':';
        hora[6] = ss[0];
        hora[7] = ss[1];
        hora[8] = 0;
        u_destino(1);
        u_limpiar(C_NEGRO);
        u_rectangulo(230, 220, 340, 120, C_AZUL);
        u_texto(330, 240, hora, C_BLANCO);
        u_texto(310, 290, "desde el arranque", C_GRIS);
        u_texto(330, 310, "q: salir", C_CYAN);
        u_presentar(230, 220, 340, 120);
        u_dormir(200);
        if (u_hay_tecla()) {
            int k = u_tecla();
            if (k == 'q' || k == 'Q' || k == 27) return 0;
        }
    }
}
