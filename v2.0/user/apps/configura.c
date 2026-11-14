#include "ugfx.h"

void __main(void) {
}

/* configura (video en vivo): panel por secciones con datos reales
 * y ajustes que aplican de verdad: pantalla (resolucion con cambio
 * inmediato sin reiniciar + CMOS 0x40 para persistir; el arranque la
 * valida y cae a 800x600 si no existe), sistema (reloj, memoria) y
 * apariencia (fondo: IPC FONDO al servidor, persiste en fondo.cfg).
 * Teclado (arriba/abajo + Enter, atajos 1-6) y raton. Arg = pid del
 * servidor. */

static int fondo_act = 0;
static int bot_antes = 0;
static char msg[48];
/* Cursor de teclado (-1 = sin mover aun; sigue al modo vigente). */
static int ksel = -1;
/* Geometria dibujada (la usan el clic y el teclado; la fija dibuja()
   en cada repintado: antes iba fija 80/240 y el dibujo real es
   78/238: fallaba el borde). */
static int y_res0 = 78, y_btnfondo = 238;

#define N_MODOS 6
/* Orden del CMOS 0x40 (ver arranque): 0=800x600, 1=640x480,
   2=1024x768, 3=1920x1080, 4=1360x768, 5=1600x900. Anchos multiplo
   de 8 (el BGA recorta: 1366 entraria como 1360). */
static const int modos_w[N_MODOS] = { 800, 640, 1024, 1920, 1360, 1600 };
static const int modos_h[N_MODOS] = { 600, 480, 768, 1080, 768, 900 };

static const char *nombre_modo(int i) {
    if (i < 0 || i >= N_MODOS) return "?";
    if (i == 1) return "640x480";
    if (i == 2) return "1024x768";
    if (i == 3) return "1920x1080";
    if (i == 4) return "1360x768";
    if (i == 5) return "1600x900";
    return "800x600";
}

/* Indice del modo vigente segun sus dimensiones (0 = 800x600). */
static int modo_actual(int pw, int ph) {
    int i;
    for (i = 0; i < N_MODOS; i++)
        if (modos_w[i] == pw && modos_h[i] == ph) return i;
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

/* Una seccion: cabecera amarilla + subrayado, con aire. */
static void seccion(ug_win_t *v, int *y, const char *t) {
    ug_texto(v, 8, *y, t, C_AMARILLO);
    ug_linea(v, 8, *y + 17, v->w - 8, *y + 17, C_GRIS_OSCURO);
    *y += 24;
}

static void dibuja(ug_win_t *v, int srv, int act, int pref) {
    int y;
    unsigned int m[8], tj[6];
    ug_comienza(v);
    ug_limpia(v, T_VENT_FONDO);
    y = 6;
    /* ---- seccion pantalla ---- */
    seccion(v, &y, "pantalla");
    {
        char b[40];
        int i = 0, k = 0;
        char nb[12];
        const char *s = "modo=";
        while (s[k]) b[i++] = s[k++];
        u_itoa(modos_w[act], nb);
        k = 0;
        while (nb[k] && i < 20) b[i++] = nb[k++];
        b[i++] = 'x';
        k = 0;
        u_itoa(modos_h[act], nb);
        while (nb[k] && i < 30) b[i++] = nb[k++];
        b[i] = 0;
        ug_texto(v, 8, y, b, C_BLANCO);
        y += 16;
    }
    ug_texto(v, 8, y, "XRGB 32bpp (cambio inmediato)", C_GRIS);
    y += 18;
    ug_texto(v, 8, y, "resolucion (teclas 1-6 o clic):", C_BLANCO);
    y += 18;
    y_res0 = y;
    {
        int i;
        for (i = 0; i < N_MODOS; i++) {
            char b[32];
            int j = 0, k = 0;
            const char *s = nombre_modo(i);
            b[j++] = (char)('0' + i + 1);
            b[j++] = '.';
            b[j++] = ' ';
            if (i == ksel) {
                b[j++] = '>';
                b[j++] = ' ';
            }
            while (s[k]) b[j++] = s[k++];
            if (i == act) {
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
                     i == pref ? C_VERDE_CLARO :
                     i == ksel ? C_AMARILLO : C_BLANCO);
            y += 17;
        }
    }
    /* ---- seccion sistema ---- */
    seccion(v, &y, "sistema");
    if (!u_reloj(tj)) {
        char b[48];
        int i = 0, k;
        const char *s = "reloj: ";
        while (s[i]) { b[i] = s[i]; i++; }
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
    y += 18;
    if (!u_info_memoria2(m)) {
        char b[48];
        int i = 0, k = 0;
        char nb[12];
        const char *s = "memoria: libres=";
        while (s[k]) b[i++] = s[k++];
        u_itoa((int)m[0], nb);
        k = 0;
        while (nb[k] && i < 42) b[i++] = nb[k++];
        b[i] = 0;
        ug_texto(v, 8, y, b, C_BLANCO);
        y += 16;
    }
    /* ---- seccion apariencia ---- */
    seccion(v, &y, "apariencia");
    ug_texto(v, 8, y, "fondo:", C_BLANCO);
    y += 18;
    y_btnfondo = y;
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

/* Avisa al servidor de que la pantalla cambio (reencaja todo). */
static void envia_video(int srv) {
    u_ipc_enviar(srv, "VIDEO", 6);
}

/* Lee el modo vigente del nucleo (indice a modos_w/h). */
static int lee_actual(void) {
    unsigned int v[4];
    if (u_video_info(v)) return 1;
    return modo_actual((int)v[0], (int)v[1]);
}

/* Aplica el modo i (raton o teclado): cambio en vivo + CMOS + aviso.
   Devuelve 1 si rediseno (siempre: el mensaje pudo cambiar). */
static void aplica_modo(int i, int *act, int *pref, int srv) {
    int rc;
    if (i < 0 || i >= N_MODOS) return;
    rc = u_video_modo(modos_w[i], modos_h[i]);
    if (rc == 0) {
        *act = i;
        *pref = i;
        ksel = i;
        sys_video_cmos(1, i);
        envia_video(srv);
        u_strcpy(msg, "aplicado");
    } else if (rc == -2) {
        u_strcpy(msg, "sin memoria");
    } else {
        u_strcpy(msg, "no soportado");
    }
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0, pw = 0, ph = 0, pref = 0, act = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    if (ug_abre(&v, srv, 440, 400, "configura")) return 98;
    ug_pide_info(&v, &pw, &ph);
    ug_tick(&v, 5);   /* fecha/hora en vivo */
    lee_fondo();
    act = lee_actual();
    {
        int r = sys_video_cmos(0, 0);
        if (r >= 0 && r <= 5) pref = r;
        else pref = act;
    }
    msg[0] = 0;
    /* Primer pintado solo con posicion valida (el DRAW de INF la deja en
       v.x/v.y). Antes se pintaba en (0,0) con tamano de pantalla y el
       presentar ensuciaba el escritorio: el fantasma de la captura. */
    if (v.x != 0 || v.y != 0)
        dibuja(&v, srv, act, pref);
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        if (t == UG_EV_SALIR) return 0;
        if (t == UG_EV_RATON) {
            int lx = ev.rx - v.x, ly = ev.ry - (v.y + UG_CHROME);
            int i;
            /* filas de resolucion (las fija dibuja en y_res0+i*17) */
            for (i = 0; i < N_MODOS; i++) {
                int yy = y_res0 + i * 17;
                if (ug_dentro(lx, ly, 12, yy, 220, 17) &&
                    ug_clic(bot_antes, ev.rbot)) {
                    ksel = i;
                    aplica_modo(i, &act, &pref, srv);
                    dibuja(&v, srv, act, pref);
                    break;
                }
            }
            /* boton fondo (lo fija dibuja en y_btnfondo) */
            {
                int yf = y_btnfondo;
                if (ug_dentro(lx, ly, 8, yf, 220, 22) &&
                    ug_clic(bot_antes, ev.rbot)) {
                    fondo_act = (fondo_act + 1) % 5;
                    envia_fondo(srv);
                    u_strcpy(msg, "fondo cambiado");
                    dibuja(&v, srv, act, pref);
                }
            }
            bot_antes = ev.rbot;
        } else if (t == UG_EV_TECLA) {
            /* Teclado: arriba/abajo mueve el cursor, Enter aplica
               (sin raton). */
            int k = ev.tecla;
            if (k == TEC_ARRIBA_U || k == TEC_ABAJO_U) {
                if (ksel < 0) ksel = act;
                else if (k == TEC_ARRIBA_U)
                    ksel = ksel <= 0 ? N_MODOS - 1 : ksel - 1;
                else
                    ksel = (ksel + 1) % N_MODOS;
                msg[0] = 0;
                dibuja(&v, srv, act, pref);
            } else if (k == 13 && ksel >= 0) {
                aplica_modo(ksel, &act, &pref, srv);
                dibuja(&v, srv, act, pref);
            } else if (k >= '1' && k <= '0' + N_MODOS) {
                /* Atajo directo a cada modo (1-6). */
                ksel = k - '1';
                aplica_modo(ksel, &act, &pref, srv);
                dibuja(&v, srv, act, pref);
            }
        } else if (t == UG_EV_DRAW) {
            act = lee_actual();
            dibuja(&v, srv, act, pref);
        }
    }
}
