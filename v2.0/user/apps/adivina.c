#include "ulib.h"

void __main(void) {
}

/* adivina: numero 1-100, pistas mayor/menor, cuenta intentos.
   v2.4-terminal: desde ventana (--gui=PID) la E/S viaja por IPC
   con la terminal (visible); desde consola usa la tecla cruda. */

static int GUI_PADRE = 0;

static void di(const char *s) {
    u_print(s);
    if (GUI_PADRE > 0) u_consola_out(GUI_PADRE, s);
}

static void di_num(int v) {
    char nb[12];
    u_itoa(v, nb);
    di(nb);
}

static void lee(char *lin, int max) {
    if (GUI_PADRE > 0) u_consola_linea(GUI_PADRE, lin, max);
    else u_lee_linea(lin, max);
}

int main(void) {
    const char *a = u_args();
    char lin[16];
    lin[0] = 0;
    while (*a == ' ') a++;
    if (!u_strncmp(a, "--gui=", 6)) {
        int p = 0;
        a += 6;
        while (*a >= '0' && *a <= '9') {
            p = p * 10 + (*a - '0');
            a++;
        }
        GUI_PADRE = p > 0 ? p : 0;
    }
    di("Adivina un numero del 1 al 100 (q sale).\n");
    for (;;) {
        int secreto = 1 + u_ticks() % 100;
        int n = 0;
        for (;;) {
            int v;
            di("tu numero: ");
            lee(lin, 16);
            if (lin[0] == 'q' || lin[0] == 'Q' || lin[0] == 0) return 0;
            v = u_atoi(lin);
            n++;
            if (v < 1 || v > 100) {
                di("del 1 al 100.\n");
            } else if (v < secreto) {
                di("mayor...\n");
            } else if (v > secreto) {
                di("menor...\n");
            } else {
                di("HAS GANADO en ");
                di_num(n);
                di(" intentos.\n");
                break;
            }
        }
        di("otra? (s/n): ");
        lee(lin, 16);
        if (lin[0] != 's' && lin[0] != 'S') return 0;
    }
}
