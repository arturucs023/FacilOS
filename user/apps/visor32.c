#include "sys.h"

void __main(void) {
}

static void imprime(const char *s) {
    int n = 0;
    while (s[n]) n++;
    sys_imprimir(s, n);
}

int main(void) {
    const char *a = (const char *)0x2F0000;
    while (*a == ' ') a++;
    char nom[32];
    int i = 0;
    while (*a && *a != ' ' && i < 31) nom[i++] = *a++;
    nom[i] = 0;
    if (!nom[0]) {
        imprime("uso: visor32 archivo\n");
        return 1;
    }
    int fd = sys_abrir(nom);
    if (fd < 0) {
        imprime("No existe.\n");
        return 2;
    }
    char buf[129];
    int n;
    while ((n = sys_leer(fd, buf, 128)) > 0) {
        buf[n] = 0;
        imprime(buf);
    }
    imprime("\n");
    sys_cerrar(fd);
    return 0;
}
