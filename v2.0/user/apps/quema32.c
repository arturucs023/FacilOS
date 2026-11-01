#include "ulib.h"

void __main(void) {
}

/* quema32 (v2.0): carga CPU pura sin bloquear (fuerza preempción).
 * args: ticks a quemar. Termina con rc = (acumulador & 127). */

int main(void) {
    const char *a = u_args();
    int n = 0;
    volatile unsigned int acc = 0;
    while (*a == ' ') a++;
    n = u_atoi(a);
    if (n < 10) n = 10;
    if (n > 2000) n = 2000;
    {
        int t0 = u_ticks();
        while (u_ticks() - t0 < n) {
            for (int i = 0; i < 500; i++) acc += (unsigned int)(i * 7 + 3);
        }
    }
    return (int)(acc & 127);
}
