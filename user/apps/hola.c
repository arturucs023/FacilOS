#include "sys.h"

void __main(void) {
}

int main(void) {
    const char *m = "Hola desde programa F32X.\n";
    int len = 0;
    while (m[len]) len++;
    sys_imprimir(m, len);
    int cs;
    __asm__ volatile ("mov %%cs,%0" : "=r"(cs));
    if ((cs & 3) == 3) sys_imprimir("anillo 3\n", 9);
    else sys_imprimir("anillo 0\n", 9);
    sys_imprimir("ticks=", 6);
    int t = sys_ticks();
    char b[12];
    int n = 0;
    if (!t) b[n++] = '0';
    else {
        char rv[12];
        int ri = 0;
        while (t && ri < 11) { rv[ri++] = (char)('0' + t % 10); t /= 10; }
        while (ri) b[n++] = rv[--ri];
    }
    b[n++] = '\n';
    sys_imprimir(b, n);
    return 42;
}
