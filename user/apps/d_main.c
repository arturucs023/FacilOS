#include "demo16.h"

void __main(void) {
}

static unsigned int sem = 12345;

unsigned int d_azar(void) {
    sem = sem * 1103515245u + 12345u;
    return (sem >> 16) & 0x7FFFu;
}

void d_azar_ini(unsigned int s) {
    sem = s;
}

void d_titulo(const char *s) {
    u_print("\n== ");
    u_print(s);
    u_print(" ==\n");
}

void d_pausa(void) {
    u_print("-- pulsa tecla --\n");
    u_tecla();
}

void d_info(const char *s) {
    u_print("   ");
    u_print(s);
    u_print("\n");
}

static void menu(void) {
    u_print("\ndemo16 v1.6\n");
    u_print("1 graficos  2 ficheros  3 memoria  4 banco  q salir\n");
    u_print("elige: ");
}

int main(void) {
    d_azar_ini(999);
    u_print("demo16: plataforma v1.6\n");
    for (;;) {
        char lin[16];
        menu();
        u_lee_linea(lin, 16);
        if (lin[0] == '1') escena_graficos();
        else if (lin[0] == '2') escena_ficheros();
        else if (lin[0] == '3') escena_memoria();
        else if (lin[0] == '4') escena_banco();
        else if (lin[0] == 'q' || lin[0] == 'Q') {
            u_print("fin demo16.\n");
            return 0;
        } else {
            u_print("usa 1-4 o q.\n");
        }
    }
}
