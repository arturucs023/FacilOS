#include "ulib.h"

void __main(void) {
}

/* estres_mix (etapa 2, fase 13C): carga combinada concurrente.
 * Dos trabajadores de memoria + un eco IPC + el padre (ping-pong, fichero
 * y sbrk propio) conviven ~5 s bajo el planificador. Todo verificado +
 * balance exacto. Sin args: director. Con "mem": trabajador. */

static int fallos = 0;
static int pruebas = 0;

static void ok(int cond, const char *nom) {
    pruebas++;
    if (!cond) {
        fallos++;
        u_printf("FALLO %s\n", nom);
    } else {
        u_printf("ok %s\n", nom);
    }
}

static int rol_mem(void) {
    int r;
    for (r = 0; r < 3; r++) {
        char *p = (char *)u_malloc(65536);
        int k;
        if (!p) return 60 + r;
        for (k = 0; k < 65536; k += 2048) p[k] = (char)(k & 127);
        for (k = 0; k < 65536; k += 2048) {
            if (p[k] != (char)(k & 127)) return 63 + r;
        }
        u_dormir(400);
        u_free(p);
    }
    return 0;
}

int main(void) {
    const char *a = u_args();
    unsigned int w0[8], w[8];
    int m1, m2, e, i;
    char b[64];
    int de = 0;
    while (*a == ' ') a++;
    if (!u_strncmp(a, "mem", 3)) return rol_mem();
    u_print("estres_mix etapa2/fase13C\n");
    ok(u_info_memoria2(w0) == 0, "info2-ok");
    /* Warmup propio ANTES de la linea base: sbrk no encoge (brk monotono),
       asi que el balance final solo cuadra si lo propio ya esta dentro. */
    {
        char *q = (char *)u_malloc(8192);
        if (!q) return 99;
        q[0] = 1;
        q[8191] = 2;
        if (q[0] != 1 || q[8191] != 2) return 99;
        u_free(q);
        if (u_info_memoria2(w0)) return 99;
    }
    m1 = u_crear("estres_mix", "mem");
    m2 = u_crear("estres_mix", "mem");
    e = u_crear("estres_ipc", "ecoT");
    ok(m1 > 0 && m2 > 0 && e > 0, "crear-todos");
    if (m1 <= 0 || m2 <= 0 || e <= 0) return 99;
    /* ping-pong contra eco mientras otros usan memoria */
    for (i = 0; i < 30; i++) {
        int r;
        if (u_ipc_enviar(e, "X", 2)) { ok(0, "mix-enviar"); break; }
        r = u_ipc_recibir(b, 64, &de);
        if (r != 2 || de != e || b[0] != 'X') { ok(0, "mix-eco"); break; }
        /* fichero propio intercalado cada 10 */
        if (i % 10 == 0) {
            UFILE *f = u_fopen("mixdat", "w");
            if (!f) { ok(0, "mix-fopen"); break; }
            if (u_fwrite("0123456789", 10, f) != 10) { ok(0, "mix-fw"); break; }
            u_fclose(f);
        }
        /* sbrk propio ya calentado antes de la base; aqui solo trafico */
        if (i == 15) {
            u_dormir(10);
        }
        u_dormir(50);
    }
    ok(i == 30, "mix-treinta");
    ok(u_ipc_enviar(e, "fin", 4) == 0, "mix-fin");
    ok(u_esperar(e) == 30, "mix-eco-30");
    ok(u_esperar(m1) == 0 && u_esperar(m2) == 0, "mix-mem-ok");
    ok(u_remove("mixdat") == 0, "mix-borra");
    {
        int k, okb = 0;
        for (k = 0; k < 10; k++) {
            if (u_info_memoria2(w)) break;
            if ((int)w[0] == (int)w0[0] && w[4] == 0 && w[5] == 0) {
                okb = 1;
                break;
            }
            u_dormir(100);
        }
        ok(okb, "mix-balance");
    }
    u_printf("estres_mix: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}
