#include "ugfx.h"

void __main(void) {
}

/* terminal (etapa 6, Fase J): terminal grafica sobre UGFX.
 * Builtins con APIs reales; `ejecutar` lanza F32X de verdad (rc real).
 * Sin tubarias: la salida del hijo va a consola y se repinta (ver SYNC).
 * Arg = pid del servidor. */

#define NLIN 120
#define NCOL 64
#define NENT 128
#define NHIST 10

static char pant[NLIN][NCOL];
static int nlin = 0;
static char ent[NENT];
static int pos = 0;
static char hist[NHIST][NENT];
static int nhist = 0, hh = 0;

static void pon(const char *s) {
    int i = 0;
    while (s[i] && nlin < NLIN) {
        int k = 0;
        while (s[i] && s[i] != '\n' && k < NCOL - 1) {
            pant[nlin][k++] = s[i++];
        }
        pant[nlin][k] = 0;
        nlin++;
        if (s[i] == '\n') i++;
    }
    if (nlin >= NLIN) {
        int d = nlin - NLIN + 1, j;
        for (j = 0; j < NLIN - 1; j++) {
            int k = 0;
            while (k < NCOL) {
                pant[j][k] = pant[j + d][k];
                k++;
            }
        }
        nlin = NLIN - 1;
    }
}

static void ayuda(void) {
    pon("comandos: ayuda version mem procs fecha");
    pon(" ver <arch> ejecutar <prog> [args]");
    pon(" limpiar salir");
    pon("nota: ejecuta programas F32X; sin builtins");
    pon("de shell (son del nucleo en modo texto).");
}

static void cmd_version(void) {
    pon("FacilOS32 version 2.0 (grafica UGFX v1)");
}

static void cmd_mem(void) {
    unsigned int v[8];
    char b[64];
    int t = 0, q;
    if (u_info_memoria2(v)) {
        pon("mem: error");
        return;
    }
    b[t++] = 'm';
    b[t++] = 'a';
    b[t++] = 'r';
    b[t++] = 'c';
    b[t++] = 'o';
    b[t++] = 's';
    b[t++] = ' ';
    b[t++] = 'l';
    b[t++] = 'i';
    b[t++] = 'b';
    b[t++] = 'r';
    b[t++] = 'e';
    b[t++] = 's';
    b[t++] = '=';
    {
        char nb[12];
        u_itoa((int)v[0], nb);
        q = 0;
        while (nb[q] && t < 40) b[t++] = nb[q++];
    }
    b[t++] = ' ';
    b[t++] = 'u';
    b[t++] = 's';
    b[t++] = 'a';
    b[t++] = 'd';
    b[t++] = 'o';
    b[t++] = 's';
    b[t++] = '=';
    {
        char nb[12];
        u_itoa((int)v[1], nb);
        q = 0;
        while (nb[q] && t < 60) b[t++] = nb[q++];
    }
    b[t] = 0;
    pon(b);
}

static void cmd_procs(void) {
    char b[64];
    int t;
    uproc_t ps[24];
    int n = u_listar(ps, 24), i;
    if (n < 0) {
        pon("procs: error");
        return;
    }
    for (i = 0; i < n; i++) {
        int q = 0;
        char nb[12];
        t = 0;
        u_itoa(ps[i].pid, nb);
        while (nb[q] && t < 10) b[t++] = nb[q++];
        b[t++] = ' ';
        q = 0;
        while (ps[i].nombre[q] && t < 30) b[t++] = ps[i].nombre[q++];
        b[t++] = ' ';
        b[t++] = (char)('0' + (ps[i].estado % 10));
        b[t] = 0;
        pon(b);
    }
}

static void cmd_fecha(void) {
    unsigned int v[6];
    char b[32];
    int t = 0, k;
    if (u_reloj(v)) {
        pon("fecha: error");
        return;
    }
    for (k = 0; k < 6; k++) {
        unsigned x = v[k];
        if (k == 1 || k == 2) b[t++] = '/';
        else if (k == 3) b[t++] = ' ';
        else if (k == 4 || k == 5) b[t++] = ':';
        if (k == 0) {
            b[t++] = (char)('0' + (x / 1000) % 10);
            b[t++] = (char)('0' + (x / 100) % 10);
        }
        b[t++] = (char)('0' + (x / 10) % 10);
        b[t++] = (char)('0' + x % 10);
    }
    b[t] = 0;
    pon(b);
}

static void cmd_ver(const char *ruta) {
    UFILE *f = u_fopen(ruta, "r");
    char b[128];
    int n, total = 0;
    if (!f) {
        pon("ver: no se abre");
        return;
    }
    while (total < 3000 && (n = u_fread(b, 100, f)) > 0) {
        int i = 0;
        while (i < n && total < 3000) {
            char ln[66];
            int k = 0;
            while (i < n && b[i] != '\n' && k < NCOL - 1) ln[k++] = b[i++];
            ln[k] = 0;
            pon(ln);
            if (i < n && b[i] == '\n') i++;
            total += k + 1;
        }
    }
    u_fclose(f);
}

static void ejecuta_linea(ug_win_t *v, const char *ln) {
    char cmd[32], resto[128];
    int i = 0, j = 0;
    (void)v;
    while (ln[i] == ' ') i++;
    while (ln[i] && ln[i] != ' ' && j < 31) cmd[j++] = ln[i++];
    cmd[j] = 0;
    while (ln[i] == ' ') i++;
    j = 0;
    while (ln[i] && j < 127) resto[j++] = ln[i++];
    resto[j] = 0;
    if (!cmd[0]) return;
    if (!u_strcmp(cmd, "ayuda")) ayuda();
    else if (!u_strcmp(cmd, "version")) cmd_version();
    else if (!u_strcmp(cmd, "mem")) cmd_mem();
    else if (!u_strcmp(cmd, "procs")) cmd_procs();
    else if (!u_strcmp(cmd, "fecha")) cmd_fecha();
    else if (!u_strcmp(cmd, "limpiar")) nlin = 0;
    else if (!u_strcmp(cmd, "salir")) ug_cierra(v);
    else if (!u_strcmp(cmd, "ver")) {
        if (!resto[0]) pon("ver: falta archivo");
        else cmd_ver(resto);
    } else if (!u_strcmp(cmd, "ejecutar")) {
        char prog[32], args[128];
        int p = 0, q = 0, rc;
        char rb[32];
        while (resto[p] && resto[p] != ' ' && q < 31) prog[q++] = resto[p++];
        prog[q] = 0;
        while (resto[p] == ' ') p++;
        q = 0;
        while (resto[p] && q < 127) args[q++] = resto[p++];
        args[q] = 0;
        if (!prog[0]) {
            pon("ejecutar: falta programa");
            return;
        }
        rc = u_ejecutar(prog, args);
        rb[0] = 'r';
        rb[1] = 'c';
        rb[2] = '=';
        {
            char nb[12];
            int t = 3;
            u_itoa(rc, nb);
            q = 0;
            while (nb[q] && t < 30) rb[t++] = nb[q++];
            rb[t] = 0;
        }
        pon(rb);
        ug_sync(v);
    } else {
        pon("no entiendo (prueba ayuda)");
    }
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    ent[0] = 0;
    if (ug_abre(&v, srv, 480, 320, "terminal")) return 98;
    pon("terminal FacilOS32 (escribe ayuda)");
    {
        char sb[24];
        int t = 0, q = 0;
        char nb[12];
        sb[t++] = 's';
        sb[t++] = 'r';
        sb[t++] = 'v';
        sb[t++] = '=';
        u_itoa(srv, nb);
        while (nb[q] && t < 22) sb[t++] = nb[q++];
        sb[t] = 0;
        pon(sb);
    }
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        int i;
        if (t == UG_EV_SALIR) return 0;
        if (t == UG_EV_TECLA) {
            int k = ev.tecla, n = u_strlen(ent);
            if (k == 13) {
                char eco[NENT + 8];
                int e = 0;
                eco[e++] = 'f';
                eco[e++] = '3';
                eco[e++] = '2';
                eco[e++] = '>';
                eco[e++] = ' ';
                for (i = 0; i < n && e < NENT + 6; i++) eco[e++] = ent[i];
                eco[e] = 0;
                pon(eco);
                if (nhist < NHIST) {
                    u_strcpy(hist[nhist++], ent);
                } else {
                    int h;
                    for (h = 0; h < NHIST - 1; h++)
                        u_strcpy(hist[h], hist[h + 1]);
                    u_strcpy(hist[NHIST - 1], ent);
                }
                hh = nhist;
                ejecuta_linea(&v, ent);
                ent[0] = 0;
                pos = 0;
            } else if (k == 8) {
                if (pos > 0 && pos <= n) {
                    for (i = pos - 1; i < n; i++) ent[i] = ent[i + 1];
                    pos--;
                }
            } else if (k == 0x103) {
                if (pos > 0) pos--;
            } else if (k == 0x104) {
                if (pos < n) pos++;
            } else if (k == 0x101) {
                if (hh > 0) {
                    hh--;
                    u_strcpy(ent, hist[hh]);
                    pos = u_strlen(ent);
                }
            } else if (k == 0x102) {
                if (hh < nhist) {
                    hh++;
                    if (hh == nhist) {
                        ent[0] = 0;
                        pos = 0;
                    } else {
                        u_strcpy(ent, hist[hh]);
                        pos = u_strlen(ent);
                    }
                }
            } else if (k >= 32 && k < 127 && n + 1 < NENT) {
                for (i = n; i >= pos; i--) ent[i + 1] = ent[i];
                ent[pos++] = (char)k;
            }
        } else if (t == UG_EV_DRAW) {
            int vis = (v.h - UG_CHROME - 24) / UG_FH, ini, li;
            ug_comienza(&v);
            ug_limpia(&v, C_NEGRO);
            if (vis < 1) vis = 1;
            ini = nlin > vis ? nlin - vis : 0;
            for (li = 0; li < vis && ini + li < nlin; li++)
                ug_texto(&v, 4, 4 + li * UG_FH, pant[ini + li], C_GRIS);
            ug_texto(&v, 4, 4 + li * UG_FH, "f32>", C_VERDE_CLARO);
            {
                char vis2[NENT + 2];
                int maxc = (v.w - 60) / UG_FW, st = 0;
                if (maxc > NENT) maxc = NENT;
                if (maxc < 1) maxc = 1;
                if (pos > st + maxc - 1) st = pos - maxc + 1;
                for (i = 0; i < maxc && ent[st + i]; i++)
                    vis2[i] = ent[st + i];
                vis2[i] = 0;
                ug_texto(&v, 4 + 4 * UG_FW, 4 + li * UG_FH, vis2, C_BLANCO);
                if (v.foco)
                    ug_relleno(&v, 4 + 4 * UG_FW + (pos - st) * UG_FW,
                               4 + li * UG_FH, 2, UG_FH, C_BLANCO);
            }
            ug_fin(&v);
        }
    }
}
