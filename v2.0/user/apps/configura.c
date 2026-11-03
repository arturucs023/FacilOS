#include "ugfx.h"

void __main(void) {
}

/* configura (etapa 6, Fase N): panel de solo lectura con datos reales
 * (pantalla, reloj, memoria, sistema). Sin controles ficticios: todo lo
 * mostrado sale de una interfaz real; lo no configurable se etiqueta.
 * Arg = pid del servidor. */

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0, pw = 0, ph = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    if (ug_abre(&v, srv, 400, 280, "configura")) return 98;
    ug_pide_info(&v, &pw, &ph);
    for (;;) {
        int t = ug_evento(&v, &ev, 1), y;
        unsigned int m[8], tj[6];
        if (t == UG_EV_SALIR) return 0;
        if (t != UG_EV_DRAW) continue;
        ug_comienza(&v);
        ug_limpia(&v, T_VENT_FONDO);
        y = 6;
        ug_texto(&v, 8, y, "pantalla", C_AMARILLO);
        y += 18;
        {
            char b[40];
            int i = 0, k = 0;
            char nb[12];
            const char *s = "modo=";
            while (s[k]) b[i++] = s[k++];
            u_itoa(pw, nb);
            k = 0;
            while (nb[k] && i < 20) b[i++] = nb[k++];
            b[i++] = 'x';
            k = 0;
            u_itoa(ph, nb);
            while (nb[k] && i < 30) b[i++] = nb[k++];
            b[i] = 0;
            ug_texto(&v, 8, y, b, C_BLANCO);
            y += 16;
        }
        ug_texto(&v, 8, y, "XRGB 32bpp (modo arranque)", C_GRIS);
        y += 22;
        ug_texto(&v, 8, y, "reloj CMOS", C_AMARILLO);
        y += 18;
        if (!u_reloj(tj)) {
            char b[32];
            int i = 0, k;
            for (k = 0; k < 6; k++) {
                unsigned x = tj[k];
                if (k) b[i++] = k < 3 ? '/' : k == 3 ? ' ' : ':';
                if (!k) {
                    b[i++] = (char)('0' + (x / 1000) % 10);
                    b[i++] = (char)('0' + (x / 100) % 10);
                }
                b[i++] = (char)('0' + (x / 10) % 10);
                b[i++] = (char)('0' + x % 10);
            }
            b[i] = 0;
            ug_texto(&v, 8, y, b, C_BLANCO);
            y += 16;
        }
        ug_texto(&v, 8, y, "(cambio de hora: no soportado)", C_GRIS);
        y += 22;
        ug_texto(&v, 8, y, "memoria", C_AMARILLO);
        y += 18;
        if (!u_info_memoria2(m)) {
            char b[40];
            int i = 0, k = 0;
            char nb[12];
            const char *s = "libres=";
            while (s[k]) b[i++] = s[k++];
            u_itoa((int)m[0], nb);
            k = 0;
            while (nb[k] && i < 34) b[i++] = nb[k++];
            b[i] = 0;
            ug_texto(&v, 8, y, b, C_BLANCO);
            y += 16;
        }
        ug_texto(&v, 8, y, "apariencia: fija en v1", C_GRIS);
        y += 16;
        (void)y;
        ug_fin(&v);
    }
}
