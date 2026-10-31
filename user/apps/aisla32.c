#include "ulib.h"

void __main(void) {
}

/* aisla32 (v1.9): proceso auxiliar de la bateria de aislamiento.
 * Se crea con un argumento que decide que intento hace:
 *   kernel  -> escribe en 0x100000 (memoria del kernel)
 *   privado -> llena su buffer con 0x55 y lo comprueba
 *   fuera   -> lee 0x7F0000, fuera de su espacio mapeado
 *   cero    -> divide entre cero
 *   mala    -> instruccion invalida
 *  終     -> termina con rc = numero de argumento
 */

int main(void) {
    const char *a = u_args();
    while (*a == ' ') a++;
    if (a[0] == 'k') {
        volatile unsigned char *p = (volatile unsigned char *)0x100000;
        p[0] = 0x41;
        return 0;
    }
    if (a[0] == 'p') {
        static unsigned char buf[256];
        for (int i = 0; i < 256; i++) buf[i] = 0x55;
        u_dormir(50);
        for (int i = 0; i < 256; i++) if (buf[i] != 0x55) return 2;
        return 7;
    }
    if (a[0] == 'f') {
        volatile unsigned char *p = (volatile unsigned char *)0x7F0000;
        return p[0];
    }
    if (a[0] == 'c') {
        volatile int x = 10, y = 0;
        return x / y;
    }
    if (a[0] == 'm') {
        __asm__ volatile (".byte 0x0F, 0x0B");
        return 1;
    }
    return 0;
}
