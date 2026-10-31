#include "ulib.h"

void __main(void) {
}

/* editor16: como editor32 pero sin tope de 511 (bufer dinamico,
   ficheros multisector). Anexa al final; ESC g=guardar, ESC s=salir. */

static char *buf = 0;
static int tam = 0, cap = 0;

static int reserva(int n) {
    char *nb;
    if (n <= cap) return 1;
    while (cap < n) cap = cap ? cap * 2 : 1024;
    if (cap > 65535) cap = 65535;
    if (n > cap) return 0;
    nb = u_realloc(buf, cap);
    if (!nb) return 0;
    buf = nb;
    return 1;
}

static void ponc(int c) {
    char s[1];
    s[0] = (char)c;
    sys_imprimir(s, 1);
}

int main(void) {
    const char *a = u_args();
    char nom[32];
    int i = 0;
    UFILE *f;
    while (*a == ' ') a++;
    while (*a && *a != ' ' && i < 31) nom[i++] = *a++;
    nom[i] = 0;
    if (!nom[0]) {
        u_print("uso: editor16 archivo\n");
        return 1;
    }
    f = u_fopen(nom, "r");
    if (f) {
        int t = u_fsize(f);
        if (t < 0) t = 0;
        if (!reserva(t + 16)) {
            u_print("Sin memoria.\n");
            u_fclose(f);
            return 2;
        }
        if (t && u_fread(buf, t, f) != t) {
            u_print("Error al leer.\n");
            u_fclose(f);
            return 2;
        }
        tam = t;
        u_fclose(f);
        sys_imprimir(buf, tam);
        if (tam) u_print("\n");
    } else {
        if (!reserva(1024)) {
            u_print("Sin memoria.\n");
            return 2;
        }
        tam = 0;
    }
    u_printf("[%d bytes] ESC g=guardar s=salir\n", tam);
    for (;;) {
        int t = u_tecla();
        if (t == 27) {
            int c = u_tecla();
            if (c == 'g') {
                UFILE *w = u_fopen(nom, "w");
                if (!w) {
                    u_print("err abrir\n");
                    return 2;
                }
                if (tam && u_fwrite(buf, tam, w) != tam) {
                    u_print("err escribir\n");
                    u_fclose(w);
                    return 2;
                }
                u_fclose(w);
                u_printf("ok %d bytes\n", tam);
                return 0;
            }
            if (c == 's') {
                u_print("Cancelado.\n");
                return 0;
            }
        } else if (t == 8) {
            if (tam > 0) {
                tam--;
                u_print("\b \b");
            }
        } else if (t == 13) {
            if (!reserva(tam + 2)) {
                u_print("Lleno.\n");
                return 3;
            }
            buf[tam++] = '\n';
            ponc('\n');
        } else if (t >= 32 && t < 127) {
            if (!reserva(tam + 2)) {
                u_print("Lleno.\n");
                return 3;
            }
            buf[tam++] = (char)t;
            ponc(t);
        }
    }
}
