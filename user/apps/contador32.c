#include "ulib.h"

void __main(void) {
}

/* contador32 (v1.9): proceso de prueba del planificador.
 *   args: una letra (A, B, X...) que se usa de nombre de fichero.
 * Avanza 1 cada 100 ms durante 3 s escribiendo la cuenta en cnt<letra>,
 * y termina solo con rc = letra. Al no tocar el teclado nunca bloquea:
 * el progreso se ve porque el padre lee el fichero. */

int main(void) {
    const char *a = u_args();
    char letra = (a[0] && a[0] != ' ') ? a[0] : 'A';
    int pasos = 30;
    while (*a == ' ') a++;
    while (*a && *a != ' ') a++;
    while (*a == ' ') a++;
    if (*a >= '0' && *a <= '9') pasos = u_atoi(a);
    if (pasos < 1) pasos = 1;
    if (pasos > 200) pasos = 200;
    char fich[16];
    u_strcpy(fich, "cnt");
    fich[3] = letra;
    fich[4] = 0;
    for (int i = 0; i < pasos; i++) {
        char b[16];
        UFILE *f = u_fopen(fich, "w");
        if (f) {
            u_itoa(i, b);
            u_fwrite(b, u_strlen(b), f);
            u_fclose(f);
        }
        u_dormir(100);
    }
    return letra;
}
