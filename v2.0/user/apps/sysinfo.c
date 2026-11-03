#include "ugfx.h"

void __main(void) {
}

/* sysinfo (etapa 6, Fase L): datos reales del sistema. Sin valores
   inventados: version (cadena del nucleo), memoria (syscall 36),
   procesos (syscall 30), pantalla (INF del servidor), raton (cola),
   reloj (syscall 44), entradas del FS (syscall 41). Arg = srv. */

static void linea_num(ug_win_t *v, int y, const char *t, unsigned int n,
                      int color) {
    char b[64];
    int i = 0, q = 0;
    char nb[12];
    while (t[i] && i < 40) {
        b[i] = t[i];
        i++;
    }
    u_itoa((int)n, nb);
    while (nb[q] && i < 60) b[i++] = nb[q++];
    b[i] = 0;
    ug_texto(v, 8, y, b, color);
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0, pw = 0, ph = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    if (ug_abre(&v, srv, 400, 300, "sysinfo")) return 98;
    ug_tick(&v, 1);   /* v2.1-p1: datos en vivo (reloj/memoria) */
    ug_pide_info(&v, &pw, &ph);
    for (;;) {
        int t = ug_evento(&v, &ev, 1), y = 6;
        unsigned int m[8], tj[6];
        uproc_t ps[24];
        int np, ne = 0, q = 0;
        udir_t d;
        if (t == UG_EV_SALIR) return 0;
        if (t != UG_EV_DRAW) continue;
        ug_comienza(&v);
        ug_limpia(&v, T_VENT_FONDO);
        ug_texto(&v, 8, y, "FacilOS32 version 2.0", C_AMARILLO);
        y += 20;
        ug_texto(&v, 8, y, "x86 32 bits (nucleo propio)", C_GRIS);
        y += 20;
        if (!u_info_memoria2(m)) {
            linea_num(&v, y, "marcos libres=", m[0], C_BLANCO);
            y += 16;
            linea_num(&v, y, "marcos usados=", m[1], C_BLANCO);
            y += 16;
            linea_num(&v, y, "pico usuario=", m[3], C_BLANCO);
            y += 16;
        }
        np = u_listar(ps, 24);
        if (np >= 0) {
            linea_num(&v, y, "procesos=", (unsigned)np, C_BLANCO);
            y += 16;
        }
        {
            char b[40];
            int i = 0;
            b[i++] = 'p';
            b[i++] = 'a';
            b[i++] = 'n';
            b[i++] = 't';
            b[i++] = 'a';
            b[i++] = 'l';
            b[i++] = 'l';
            b[i++] = 'a';
            b[i++] = '=';
            {
                char nb[12];
                int k = 0;
                u_itoa(pw, nb);
                while (nb[k] && i < 24) b[i++] = nb[k++];
                b[i++] = 'x';
                k = 0;
                u_itoa(ph, nb);
                while (nb[k] && i < 34) b[i++] = nb[k++];
                b[i] = 0;
            }
            ug_texto(&v, 8, y, b, C_BLANCO);
            y += 16;
        }
        {
            int hay = u_hay_raton();
            ug_texto(&v, 8, y,
                     hay > 0 ? "raton: PS/2 con datos" : "raton: PS/2 (IRQ12)",
                     C_BLANCO);
            y += 16;
        }
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
        while (u_dir_hijo(0, q, &d) >= 0 && q < 200) {
            if (d.tipo == 1 || d.tipo == 2) ne++;
            q++;
        }
        linea_num(&v, y, "entradas FS=", (unsigned)ne, C_BLANCO);
        y += 16;
        ug_texto(&v, 8, y, "(cierra con la X)", C_GRIS);
        (void)y;
        ug_fin(&v);
    }
}
