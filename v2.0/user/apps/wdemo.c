#include "ulib.h"

void __main(void) {
}

/* wdemo (etapa 4, E2): cliente demo del servidor. Arg = pid del servidor.
 * Registra (w=400,h=200), dibuja contenido al DRAW y sale con QUIT. */

int main(void) {
    const char *a = u_args();
    int srv = 0, id = -1, x = 0, y = 0, tecla = -1;
    int rx = -1, ry = -1, rb = -1;
    char b[64];
    int de = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    if (u_ipc_enviar(srv, "REG 400 200", 12)) return 98;
    for (;;) {
        int r = u_ipc_recibir(b, 64, &de);
        if (r < 0) return 97;
        if (r >= 2 && b[0] == 'I' && b[1] == 'D') {
            int k = 3;
            while (k < r && (b[k] < '0' || b[k] > '9')) k++;
            id = u_atoi(b + k);
        } else if (r >= 4 && b[0] == 'D' && b[1] == 'R' && b[2] == 'A' &&
                   b[3] == 'W') {
            int k = 5, xx, yy;
            while (k < r && (b[k] < '0' || b[k] > '9') && b[k] != '-') k++;
            xx = u_atoi(b + k);
            while (k < r && ((b[k] >= '0' && b[k] <= '9') || b[k] == '-')) k++;
            while (k < r && (b[k] < '0' || b[k] > '9') && b[k] != '-') k++;
            yy = u_atoi(b + k);
            x = xx;
            y = yy;
            /* toma propiedad y dibuja contenido (lease) */
            u_texto(0, 0, "", C_NEGRO);
            u_rectangulo(x, y + 20, 400, 180, C_VERDE);
            u_texto(x + 16, y + 60, "ventana demo", C_BLANCO);
            u_texto(x + 16, y + 90, "id dibujado", C_BLANCO);
            if (rx >= 0) {
                char t[24];
                int ti = 0;
                t[ti++] = 'r';
                t[ti++] = ':';
                {
                    char nb[12];
                    int j = 0, v = rx;
                    if (!v) nb[j++] = '0';
                    else {
                        char rv[12];
                        int ri = 0;
                        while (v && ri < 11) {
                            rv[ri++] = (char)('0' + v % 10);
                            v /= 10;
                        }
                        while (ri) nb[j++] = rv[--ri];
                    }
                    nb[j] = 0;
                    j = 0;
                    while (nb[j] && ti < 20) t[ti++] = nb[j++];
                    t[ti++] = ',';
                    v = ry;
                    j = 0;
                    if (!v) nb[j++] = '0';
                    else {
                        char rv[12];
                        int ri = 0;
                        while (v && ri < 11) {
                            rv[ri++] = (char)('0' + v % 10);
                            v /= 10;
                        }
                        while (ri) nb[j++] = rv[--ri];
                    }
                    nb[j] = 0;
                    j = 0;
                    while (nb[j] && ti < 20) t[ti++] = nb[j++];
                    t[ti] = 0;
                }
                u_texto(x + 16, y + 150, t, C_BLANCO);
            }
            if (tecla >= 0) {
                char t[16];
                int ti = 0;
                t[ti++] = 't';
                t[ti++] = 'e';
                t[ti++] = 'c';
                t[ti++] = 'l';
                t[ti++] = 'a';
                t[ti++] = ':';
                t[ti++] = (tecla >= 32 && tecla < 127) ? (char)tecla : '?';
                t[ti] = 0;
                u_texto(x + 16, y + 120, t, C_BLANCO);
            }
            u_presentar(x, y, 400, 200);
            u_ipc_enviar(srv, "ACK", 4);
        } else if (r >= 4 && b[0] == 'Q' && b[1] == 'U' && b[2] == 'I' &&
                   b[3] == 'T') {
            u_ipc_enviar(srv, "BYE", 4);
            return 0;
        } else if (r >= 4 && b[0] == 'K' && b[1] == 'E' && b[2] == 'Y') {
            int k = 4;
            while (k < r && (b[k] < '0' || b[k] > '9')) k++;
            tecla = u_atoi(b + k);
        }
        else if (r >= 4 && b[0] == 'M' && b[1] == 'O' && b[2] == 'U') {
            int k = 4;
            while (k < r && (b[k] < '0' || b[k] > '9')) k++;
            rx = u_atoi(b + k);
            while (k < r && b[k] >= '0' && b[k] <= '9') k++;
            while (k < r && (b[k] < '0' || b[k] > '9')) k++;
            ry = u_atoi(b + k);
            while (k < r && b[k] >= '0' && b[k] <= '9') k++;
            while (k < r && (b[k] < '0' || b[k] > '9')) k++;
            rb = u_atoi(b + k);
        }
        (void)id;
        (void)x;
        (void)y;
    }
}
