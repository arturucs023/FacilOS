#include "ugfx.h"

void __main(void) {
}

/* configura (v2.1-p1): panel con datos reales + ajustes que aplican de
 * verdad: fondo (IPC FONDO al servidor, persiste en fondo.cfg) y modo
 * de video (syscall 45 -> CMOS 0x40, aplica al reiniciar; el arranque
 * lo valida y cae a 800x600 si no existe). Arg = pid del servidor. */

static int fondo_act = 0;
static int bot_antes = 0;
static char msg[48];

static const char *nombre_modo(int i) {
    if (i == 1) return "640x480";
    if (i == 2) return "1024x768";
    return "800x600";
}

static int modo_actual(int pw, int ph) {
    if (pw == 640 && ph == 480) return 1;
    if (pw == 1024 && ph == 768) return 2;
    return 0;
}

/* Lee fondo.cfg para mostrar el tema vigente (el servidor lo cargo). */
static void lee_fondo(void) {
    UFILE *f = u_fopen("fondo.cfg", "r");
    if (f) {
        char c = 0;
        if (u_fread(&c, 1, f) == 1 && c >= '0' && c <= '4')
            fondo_act = c - '0';
        u_fclose(f);
    }
}

static void envia_fondo(int srv) {
    char m[8];
    m[0] = 'F';
    m[1] = 'O';
    m[2] = 'N';
    m[3] = 'D';
    m[4] = 'O';
    m[5] = ' ';
    m[6] = (char)('0' + fondo_act);
    m[7] = 0;
    u_ipc_enviar(srv, m, 8);
}

static void dibuja(ug_win_t *v, int srv, int pw, int ph, int pref) {
    int y;
    unsigned int m[8], tj[6];
    ug_comienza(v);
    ug_limpia(v, T_VENT_FONDO);
    y = 6;
    ug_texto(v, 8, y, "pantalla", C_AMARILLO);
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
        ug_texto(v, 8, y, b, C_BLANCO);
        y += 16;
    }
    ug_texto(v, 8, y, "XRGB 32bpp (modo arranque)", C_GRIS);
    y += 20;
    ug_texto(v, 8, y, "resolucion (aplica al reiniciar):", C_AMARILLO);
    y += 18;
    {
        int i;
        for (i = 0; i < 3; i++) {
            char b[32];
            int j = 0, k = 0;
            const char *s = nombre_modo(i);
            while (s[k]) b[j++] = s[k++];
            if (i == modo_actual(pw, ph)) {
                const char *s2 = "  [actual]";
                k = 0;
                while (s2[k]) b[j++] = s2[k++];
            } else if (i == pref) {
                const char *s2 = "  [elegido]";
                k = 0;
                while (s2[k]) b[j++] = s2[k++];
            }
            b[j] = 0;
            ug_texto(v, 12, y, b,
                     i == pref ? C_VERDE_CLARO : C_BLANCO);
            y += 18;
        }
    }
    ug_texto(v, 8, y, "reloj CMOS", C_AMARILLO);
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
        ug_texto(v, 8, y, b, C_BLANCO);
        y += 16;
    }
    ug_texto(v, 8, y, "(cambio de hora: no soportado)", C_GRIS);
    y += 20;
    ug_texto(v, 8, y, "memoria", C_AMARILLO);
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
        ug_texto(v, 8, y, b, C_BLANCO);
        y += 16;
    }
    ug_texto(v, 8, y, "fondo:", C_AMARILLO);
    y += 18;
    ug_boton(v, 8, y, 220, 22, t_fondo_nombre(fondo_act),
             UG_BOTON_NORMAL);
    y += 26;
    if (msg[0]) {
        ug_texto(v, 8, y, msg, C_VERDE_CLARO);
        y += 16;
    }
    (void)y;
    (void)srv;
    ug_fin(v);
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0, pw = 0, ph = 0, pref = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    if (ug_abre(&v, srv, 440, 380, "configura")) return 98;
    ug_pide_info(&v, &pw, &ph);
    ug_tick(&v, 5);   /* v2.1-p1: fecha/hora en vivo */
    lee_fondo();
    {
        int r = sys_video_cmos(0, 0);
        if (r >= 0 && r <= 2) pref = r;
    }
    msg[0] = 0;
    dibuja(&v, srv, pw, ph, pref);
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        if (t == UG_EV_SALIR) return 0;
        if (t == UG_EV_RATON) {
            int lx = ev.rx - v.x, ly = ev.ry - (v.y + UG_CHROME);
            int i;
            /* filas de resolucion (dibuja: y=80+i*18, h=16) */
            for (i = 0; i < 3; i++) {
                int yy = 80 + i * 18;
                if (ug_dentro(lx, ly, 12, yy, 220, 16) &&
                    ug_clic(bot_antes, ev.rbot)) {
                    if (sys_video_cmos(1, i) == 0) {
                        pref = i;
                        u_strcpy(msg, "guardado: reinicia");
                    } else {
                        u_strcpy(msg, "no se guarda");
                    }
                    dibuja(&v, srv, pw, ph, pref);
                    break;
                }
            }
            /* boton fondo (dibuja: y=240, h=22) */
            {
                int yf = 240;
                if (ug_dentro(lx, ly, 8, yf, 220, 22) &&
                    ug_clic(bot_antes, ev.rbot)) {
                    fondo_act = (fondo_act + 1) % 5;
                    envia_fondo(srv);
                    u_strcpy(msg, "fondo cambiado");
                    dibuja(&v, srv, pw, ph, pref);
                }
            }
            bot_antes = ev.rbot;
        } else if (t == UG_EV_DRAW) {
            dibuja(&v, srv, pw, ph, pref);
        }
    }
}
