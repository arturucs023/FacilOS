#include "ulib.h"

void __main(void) {
}

/* disco32 (etapa 3, E5/Fase 13 A-E,G): ficheros sobre el backend montado
 * (RAM o disco: transparente por diseno). Verifica contenido exacto,
 * parciales, truncados, errores y estres. La persistencia entre reinicios
 * la verifica el harness (dos arranques QEMU con el mismo disco).
 * Devuelve nº de fallos. */

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

static void pinta(char *b, int n, int semilla) {
    for (int i = 0; i < n; i++) b[i] = (char)(((i * 31 + semilla) % 96) + 32);
}

static int verifica(const char *b, int n, int semilla) {
    for (int i = 0; i < n; i++) {
        if (b[i] != (char)(((i * 31 + semilla) % 96) + 32)) return 0;
    }
    return 1;
}

int main(void) {
    char b[1024];
    u_print("disco32 etapa3/E5\n");
    /* C: crear(3000B multi-sector), patron exacto */
    {
        UFILE *f = u_fopen("d_big", "w");
        char *p;
        int n;
        ok(f != 0, "c-crear");
        if (!f) return 99;
        p = (char *)u_malloc(3000);
        ok(p != 0, "c-malloc");
        pinta(p, 3000, 7);
        ok(u_fwrite(p, 3000, f) == 3000, "c-escribir");
        ok(u_fclose(f) == 0, "c-cerrar");
        f = u_fopen("d_big", "r");
        ok(f != 0, "c-reabrir");
        n = u_fread(b, 1000, f);
        ok(n == 1000 && verifica(b, 1000, 7), "c-leer1");
        /* parcial con offset */
        ok(u_fseek(f, 2000, U_INI) == 0, "c-seek");
        n = u_fread(b, 1000, f);
        ok(n == 1000 && b[0] == (char)(((2000 * 31 + 7) % 96) + 32), "c-parcial");
        ok(u_fclose(f) == 0, "c-cerrar2");
        u_free(p);
    }
    /* reescritura parcial + truncados */
    {
        UFILE *f = u_fopen("d_big", "r");
        char w[100];
        int i;
        ok(f != 0, "m-abrir");
        for (i = 0; i < 100; i++) w[i] = 'Z';
        ok(u_fseek(f, 500, U_INI) == 0, "m-seek");
        ok(u_fwrite(w, 100, f) == 100, "m-parcial");
        ok(u_fseek(f, 0, U_INI) == 0, "m-reb");
        ok(u_fread(b, 1024, f) == 1024, "m-relee");
        {
            int bien = 1, k;
            for (k = 0; k < 1024; k++) {
                char esp = (k >= 500 && k < 600) ? 'Z'
                           : (char)(((k * 31 + 7) % 96) + 32);
                if (b[k] != esp) { bien = 0; break; }
            }
            ok(bien, "m-contenido");
        }
        ok(u_fijar_tam(f, 600) == 0, "m-trunca-abajo");
        ok(u_fsize(f) == 600, "m-tam600");
        ok(u_fijar_tam(f, 1500) == 0, "m-trunca-arriba");
        ok(u_fseek(f, 600, U_INI) == 0, "m-seek600");
        ok(u_fread(b, 100, f) == 100, "m-lee-ceros");
        {
            int bien = 1;
            for (i = 0; i < 100; i++) if (b[i] != 0) bien = 0;
            ok(bien, "m-ceros");
        }
        ok(u_fclose(f) == 0, "m-cerrar");
    }
    /* errores */
    {
        UFILE *f;
        ok(u_fopen("noexiste_zz", "r") == 0, "e-abrir-no");
        ok(u_remove("noexiste_zz") != 0, "e-borrar-no");
        f = u_fopen("d_big", "r");
        ok(f != 0, "e-abrir");
        ok(u_fseek(f, 99999, U_INI) != 0, "e-seek-lejos");
        ok(u_fijar_tam(f, 70000) != 0, "e-truncar-enorme");
        ok(u_fclose(f) == 0, "e-cerrar");
    }
    /* borrar + verificar ausencia */
    ok(u_remove("d_big") == 0, "d-borrar");
    ok(u_fopen("d_big", "r") == 0, "d-ausente");
    /* G: 30 pequenos + 1 grande (60000) + reescrituras + limpieza */
    {
        int i, n;
        char nom[16];
        for (i = 0; i < 30; i++) {
            UFILE *f;
            nom[0] = 'g';
            nom[1] = (char)('0' + (i / 10) % 10);
            nom[2] = (char)('0' + i % 10);
            nom[3] = 0;
            f = u_fopen(nom, "w");
            if (!f) break;
            pinta(b, 200, i);
            if (u_fwrite(b, 200, f) != 200) { u_fclose(f); break; }
            u_fclose(f);
        }
        ok(i == 30, "g-treinta");
        for (i = 0, n = 0; i < 30; i++) {
            UFILE *f;
            nom[0] = 'g';
            nom[1] = (char)('0' + (i / 10) % 10);
            nom[2] = (char)('0' + i % 10);
            nom[3] = 0;
            f = u_fopen(nom, "r");
            if (!f) break;
            if (u_fread(b, 200, f) != 200) { u_fclose(f); break; }
            if (!verifica(b, 200, i)) { u_fclose(f); break; }
            u_fclose(f);
            n++;
        }
        ok(n == 30, "g-releer");
        {
            UFILE *f = u_fopen("d_huge", "w");
            char *p = 0;
            ok(f != 0, "g-huge-crear");
            if (f) {
                ok(u_fijar_tam(f, 60000) == 0, "g-huge-tam");
                p = (char *)u_malloc(2048);
                if (p) {
                    int k, rr = 1;
                    for (k = 0; k < 4; k++) {
                        int j;
                        for (j = 0; j < 2048; j++)
                            p[j] = (char)(((k * 2048 + j) % 96) + 32);
                        if (u_fseek(f, k * 15000, U_INI)) { rr = 0; break; }
                        if (u_fwrite(p, 2048, f) != 2048) { rr = 0; break; }
                    }
                    ok(rr, "g-huge-trozos");
                    u_free(p);
                    p = 0;
                    /* relee los 4 trozos */
                    for (k = 0; k < 4 && rr; k++) {
                        char *q = (char *)u_malloc(2048);
                        int j, bien = 1;
                        if (!q) { rr = 0; break; }
                        if (u_fseek(f, k * 15000, U_INI)) { bien = 0; }
                        else if (u_fread(q, 2048, f) != 2048) { bien = 0; }
                        else {
                            for (j = 0; j < 2048; j++) {
                                if (q[j] != (char)(((k * 2048 + j) % 96) + 32)) {
                                    bien = 0;
                                    break;
                                }
                            }
                        }
                        u_free(q);
                        if (!bien) { rr = 0; break; }
                    }
                    ok(rr, "g-huge-relee");
                }
                ok(u_fclose(f) == 0, "g-huge-cerrar");
                ok(u_remove("d_huge") == 0, "g-huge-borrar");
            }
            for (i = 0; i < 30; i++) {
                nom[0] = 'g';
                nom[1] = (char)('0' + (i / 10) % 10);
                nom[2] = (char)('0' + i % 10);
                nom[3] = 0;
                u_remove(nom);
            }
            u_printf("g-limpio\n");
        }
    }
    u_printf("disco32: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}