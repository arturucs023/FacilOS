#include "ulog.h"
#include "ulib.h"

static const char *nom_nivel(int n) {
    if (n == 1) return "AVISO";
    if (n == 2) return "ERROR";
    if (n == 3) return "DEPUR";
    if (n == 4) return "CRITICO";
    return "INFO";
}

static void dos(char *d, int t, unsigned v) {
    d[t] = (char)('0' + (v / 10) % 10);
    d[t + 1] = (char)('0' + v % 10);
}

int ulog_form(char *d, int cap, int nivel, const char *comp, int cod,
              const char *msg) {
    int t = 0, i;
    unsigned int tj[6];
    char nb[12];
    const char *nv;
    if (cap < 32 || !d || !comp || !msg) return 0;
    d[t++] = '[';
    if (!u_reloj(tj)) {
        /* El RTC da ano de 2 digitos: se asume siglo XXI. */
        unsigned yr = tj[0] < 100 ? tj[0] + 2000 : tj[0] % 10000;
        u_itoa((int)yr, nb);
        {
            int k, need = 4 - (int)u_strlen(nb), p = 0;
            char tmp[12];
            for (k = 0; k < need; k++) tmp[p++] = '0';
            for (k = 0; nb[k]; k++) tmp[p++] = nb[k];
            tmp[p] = 0;
            for (k = 0; tmp[k] && t < cap - 1; k++) d[t++] = tmp[k];
        }
        d[t++] = '/';
        dos(d, t, tj[1]);
        t += 2;
        d[t++] = '/';
        dos(d, t, tj[2]);
        t += 2;
        d[t++] = ' ';
        dos(d, t, tj[3]);
        t += 2;
        d[t++] = ':';
        dos(d, t, tj[4]);
        t += 2;
        d[t++] = ':';
        dos(d, t, tj[5]);
        t += 2;
    } else {
        const char *s = "--/--/-- --:--:--";
        while (*s && t < cap - 1) d[t++] = *s++;
    }
    d[t++] = ']';
    d[t++] = ' ';
    d[t++] = '[';
    nv = nom_nivel(nivel);
    while (*nv && t < cap - 1) d[t++] = *nv++;
    d[t++] = ']';
    d[t++] = ' ';
    d[t++] = '[';
    while (*comp && t < cap - 1) {
        d[t++] = *comp++;
    }
    d[t++] = ']';
    d[t++] = ' ';
    {
        const char *pref = "TER-";
        while (*pref && t < cap - 1) d[t++] = *pref++;
        u_itoa(cod, nb);
        {
            int k = 0;
            while (k < 4 - (int)u_strlen(nb) && t < cap - 1) {
                d[t++] = '0';
                k++;
            }
        }
        for (i = 0; nb[i] && t < cap - 1; i++) d[t++] = nb[i];
    }
    d[t++] = ':';
    d[t++] = ' ';
    while (*msg && t < cap - 1) {
        if (*msg == '\n') break;
        d[t++] = *msg++;
    }
    if (t < cap) d[t] = 0;
    return t;
}
