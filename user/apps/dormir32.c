#include "ulib.h"

void __main(void) {
}

/* dormir32 (v1.9): duerme 5 x 600 ms sin espera activa, avisando por
 * fichero cuando se despierta, y termina con rc 0. Si el planificador
 * quemara CPU en lugar de dormir, el proceso que lo crea notaria que los
 * otros no avanzan. */

int main(void) {
    for (int i = 0; i < 5; i++) {
        UFILE *f = u_fopen("cntD", "w");
        if (f) {
            char b[16];
            u_itoa(i, b);
            u_fwrite(b, u_strlen(b), f);
            u_fclose(f);
        }
        u_dormir(600);
    }
    return 0;
}
