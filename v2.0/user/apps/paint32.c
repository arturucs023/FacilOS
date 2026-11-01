#include "sys.h"

void __main(void) {
}

static void imprime(const char *s) {
    int n = 0;
    while (s[n]) n++;
    sys_imprimir(s, n);
}

int main(void) {
    sys_limpiar_pantalla(0);
    const char *t = "paint32: flec esp c x q";
    sys_texto(8, 4, t, 23, 15);
    int x = 400, y = 300, ci = 0, px = -1, py = -1;
    int cols[4] = { 4, 2, 9, 14 };
    for (;;) {
        int k = sys_leer_tecla();
        if (k == 'q') break;
        if (k == 'c') {
            ci = (ci + 1) % 4;
            continue;
        }
        if (k == 'x') {
            sys_limpiar_pantalla(0);
            sys_texto(8, 4, t, 23, 15);
            px = -1;
            continue;
        }
        if (k == 0x103) x -= 8;
        else if (k == 0x104) x += 8;
        else if (k == 0x101) y -= 8;
        else if (k == 0x102) y += 8;
        else if (k == ' ') {
            sys_pixel(x, y, cols[ci]);
            px = -1;
            continue;
        } else continue;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > 792) x = 792;
        if (y > 592) y = 592;
        if (px >= 0) sys_pixel(px, py, 0);
        sys_pixel(x, y, 15);
        px = x;
        py = y;
    }
    imprime("\n");
    return 0;
}
