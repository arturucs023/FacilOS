#include "demo16.h"

/* Estres de memoria: cientos de bloques con patrones, liberacion
   parcial, recrecido, y comprobacion de union de libres. */

static int fallos = 0;

static void ok(int cond, const char *nom) {
    if (!cond) {
        fallos++;
        u_printf("FALLO mem %s\n", nom);
    }
}

static void pon_patron(char *p, int n, int semilla) {
    int i;
    for (i = 0; i < n; i++) p[i] = (char)((semilla + i * 37) & 255);
}

static int mira_patron(char *p, int n, int semilla) {
    int i;
    for (i = 0; i < n; i++) {
        if (p[i] != (char)((semilla + i * 37) & 255)) return 0;
    }
    return 1;
}

void escena_memoria(void) {
    /* Punteros en heap (los static cero ocuparian fichero: COFF .data). */
    char **a = u_malloc(500 * sizeof(char *));
    int *t = u_malloc(500 * sizeof(int));
    int i, nb0, by0, li0, nb1, by1, li1;
    int vivos = 0;
    if (!a || !t) {
        d_info("sin memoria");
        u_free(a);
        u_free(t);
        return;
    }
    fallos = 0;
    d_titulo("memoria (500 bloques)");
    u_malloc_info(&nb0, &by0, &li0);
    d_azar_ini(4242);
    for (i = 0; i < 500; i++) {
        t[i] = 16 + (int)(d_azar() % 400);
        a[i] = u_malloc(t[i]);
        if (!a[i]) break;
        pon_patron(a[i], t[i], i);
    }
    ok(i == 500, "500-mallocs");
    /* verifica todos */
    {
        int bien = 1;
        for (i = 0; i < 500; i++) {
            if (!mira_patron(a[i], t[i], i)) { bien = 0; break; }
        }
        ok(bien, "patrones-500");
    }
    /* libera 7 de cada 10 */
    for (i = 0; i < 500; i++) {
        if (i % 10 < 7) { u_free(a[i]); a[i] = 0; }
        else vivos++;
    }
    u_printf("vivos tras poda: %d\n", vivos);
    /* recrece los vivos al doble (realloc) */
    {
        int bien = 1;
        for (i = 0; i < 500; i++) {
            char *np;
            if (!a[i]) continue;
            np = u_realloc(a[i], t[i] * 2);
            if (!np) { bien = 0; break; }
            a[i] = np;
            if (!mira_patron(a[i], t[i], i)) { bien = 0; break; }
            pon_patron(a[i] + t[i], t[i], i + 1000);
            t[i] *= 2;
        }
        ok(bien, "realloc-x2");
    }
    /* rellena huecos con pequenos */
    for (i = 0; i < 500; i++) {
        if (a[i]) continue;
        t[i] = 32;
        a[i] = u_malloc(32);
        if (!a[i]) break;
    }
    ok(i == 500, "relleno-huecos");
    for (i = 0; i < 500; i++) u_free(a[i]);
    u_free(a);
    u_free(t);
    u_malloc_info(&nb1, &by1, &li1);
    u_printf("bloques %d->%d ocup %d->%d libre %d->%d\n", nb0, nb1, by0,
             by1, li0, li1);
    ok(by1 <= by0 + 64, "todo-liberado");
    ok(nb1 <= nb0 + 4, "union-arena");
    u_printf("memoria: %d fallos\n", fallos);
}
