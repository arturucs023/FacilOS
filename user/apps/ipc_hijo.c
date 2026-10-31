#include "ulib.h"

void __main(void) {
}

/* ipc_hijo (v2.0): responde con eco a cada mensaje; "fin" termina con el
 * numero de ecos. El emisor viene en cada recepcion. */

int main(void) {
    char b[64];
    int de = 0, n = 0;
    for (;;) {
        int r = u_ipc_recibir(b, 64, &de);
        if (r < 0) return 90 + n;
        if (r >= 3 && b[0] == 'f' && b[1] == 'i' && b[2] == 'n') return n;
        if (u_ipc_enviar(de, b, r)) return 80 + n;
        n++;
    }
}
