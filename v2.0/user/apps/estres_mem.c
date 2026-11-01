#include "ulib.h"

void __main(void) {
}

/* estres_mem (etapa 2, fase 1): presion de memoria fisica con balance exacto.
 * Sin args: director de fases. Con "hijo <modo> <kb>": trabajador que pinta
 * kb con patron, lo verifica y termina (salir=0, falta=#PF, dormir=0 tras
 * 300 ms, sprint=sbrk de 1 MB hasta -1 devolviendo nº de tramos).
 * Toda espera es acotada; el balance final exige igualdad exacta tras
 * drenaje verificado (no duerme a ciegas). */

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

static int hijo_main(const char *a) {
    char modo[16];
    int i = 0, kb, n;
    while (*a == ' ') a++;
    while (*a && *a != ' ' && i < 15) { modo[i++] = *a++; }
    modo[i] = 0;
    while (*a == ' ') a++;
    kb = u_atoi(a);
    if (kb < 1) kb = 1;
    if (kb > 1200) kb = 1200;
    if (!u_strcmp(modo, "sprint")) {
        int tramos = 0;
        for (i = 0; i < 100; i++) {
            void *p = u_malloc(1024 * 1024);
            if (!p) break;
            ((char *)p)[0] = 7;
            ((char *)p)[1024 * 1024 - 1] = 7;
            tramos++;
        }
        return tramos;
    }
    n = kb * 1024;
    {
        char *p = (char *)u_malloc(n);
        int k;
        if (!p) return 77;
        for (k = 0; k < n; k++) p[k] = (char)(k * 13 + 1);
        for (k = 0; k < n; k += 1024) {
            if (p[k] != (char)(k * 13 + 1)) return 78;
        }
        if (!u_strcmp(modo, "falta")) {
            volatile unsigned char *q = (volatile unsigned char *)0x100000;
            *q = 1;
            return 79;
        }
        if (!u_strcmp(modo, "dormir")) u_dormir(300);
        u_free(p);
    }
    return 0;
}

/* Espera drenaje de fusiona: repite info2 hasta ver libres==obj o agotar. */
static int espera_libres(unsigned int obj) {
    unsigned int w[8];
    int i;
    for (i = 0; i < 10; i++) {
        if (u_info_memoria2(w)) return -1;
        if ((int)w[0] == (int)obj && w[4] == 0 && w[5] == 0) return 0;
        u_dormir(100);
    }
    return -1;
}

static int lanza(const char *args) {
    return u_crear("estres_mem", args);
}

int main(void) {
    const char *a = u_args();
    unsigned int w0[8], w[8];
    int i;
    while (*a == ' ') a++;
    if (!u_strncmp(a, "hijo", 4)) return hijo_main(a + 4);
    u_print("estres_mem etapa2/fase1\n");
    ok(u_info_memoria2(w0) == 0, "info2-ok");
    /* P1: 20 crecer+salir secuenciales, drenaje cada 5 */
    for (i = 0; i < 20; i++) {
        char ar[32];
        int pid, rc;
        u_strcpy(ar, "hijo salir ");
        {
            char nb[8];
            u_itoa(4 + (i * 37) % 100, nb);
            u_strcat(ar, nb);
        }
        pid = lanza(ar);
        if (pid <= 0) { ok(0, "p1-crear"); break; }
        rc = u_esperar(pid);
        if (rc != 0) { ok(0, "p1-rc"); break; }
        if (i % 5 == 4) {
            if (espera_libres(w0[0])) { ok(0, "p1-balance"); break; }
        }
    }
    ok(i == 20, "p1-veinte");
    /* P2: 10 crecer+falta (#PF controlado) */
    for (i = 0; i < 10; i++) {
        int pid = lanza("hijo falta 40");
        int rc;
        if (pid <= 0) { ok(0, "p2-crear"); break; }
        rc = u_esperar(pid);
        if (rc != -1) { ok(0, "p2-rc-muerte"); break; }
    }
    ok(i == 10, "p2-diez-faltas");
    ok(espera_libres(w0[0]) == 0, "p2-balance");
    /* P3: 5 concurrentes durmiendo con memoria */
    {
        int pids[5], vivos = 0;
        for (i = 0; i < 5; i++) {
            pids[i] = lanza("hijo dormir 60");
            if (pids[i] > 0) vivos++;
        }
        ok(vivos == 5, "p3-cinco");
        u_dormir(100);
        ok(u_info_memoria2(w) == 0 && w[2] > w0[2], "p3-presion-visible");
        for (i = 0; i < 5; i++) {
            if (pids[i] > 0 && u_esperar(pids[i]) != 0) { ok(0, "p3-rc"); break; }
        }
        ok(i == 5, "p3-todos");
        ok(espera_libres(w0[0]) == 0, "p3-balance");
    }
    /* P4: sprint hasta el limite de heap (1,5 MB por proceso: el agotamiento
       global de marcos no es alcanzable desde usuario por diseño: 8*608
       < libres; aqui se verifica el tope limpio y el sistema vivo). */
    {
        int pid = lanza("hijo sprint 0");
        int rc = -99, pid2 = -1;
        if (pid > 0) rc = u_esperar(pid);
        ok(rc > 0, "p4-agota-limpio");
        pid2 = lanza("hijo salir 4");
        ok(pid2 > 0 && u_esperar(pid2) == 0, "p4-sistema-vivo");
        ok(espera_libres(w0[0]) == 0, "p4-balance");
    }
    ok(u_info_memoria2(w) == 0 && w[0] == w0[0] && w[4] == 0 && w[5] == 0,
       "balance-final-exacto");
    u_printf("estres_mem: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}
