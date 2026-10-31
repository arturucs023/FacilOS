#include "sys.h"

void __main(void) {
}

/* len grande: el nucleo corta en NUL (caso 1). Sin strlen. */
static void num(int v) {
    char b[12];
    int n = 0;
    if (!v) b[n++] = '0';
    else {
        char r[12];
        int i = 0;
        while (v && i < 11) {
            r[i++] = (char)('0' + v % 10);
            v /= 10;
        }
        while (i) b[n++] = r[--i];
    }
    sys_imprimir(b, n);
}

int main(void) {
    sys_imprimir("b32\n", 99);
    int t0 = sys_ticks();
    int n = 0;
    for (int i = 0; i < 100; i++) {
        int p = sys_memoria(64);
        if (p <= 0) break;
        char *q = (char *)p;
        for (int j = 0; j < 64; j++) q[j] = (char)i;
        n++;
    }
    int t1 = sys_ticks();
    sys_imprimir("mem x100: ", 99);
    num(t1 - t0);
    sys_imprimir(" t n=", 99);
    num(n);
    sys_imprimir("\n50 sc: ", 99);
    t0 = sys_ticks();
    for (int i = 0; i < 50; i++) sys_imprimir(".", 1);
    t1 = sys_ticks();
    num(t1 - t0);
    sys_imprimir(" t\nfin\n", 99);
    return 0;
}
