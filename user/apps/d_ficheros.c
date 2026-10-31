#include "demo16.h"

/* Escena de ficheros: crea un fichero de 8 KB, lo recorre con seeks,
   lo parchea por offset, lo verifica y lo limpia. */

static int fallos = 0;

static void ok(int cond, const char *nom) {
    if (!cond) {
        fallos++;
        u_printf("FALLO fich %s\n", nom);
    }
}

static void rellena(char *b, int base, int n) {
    int i;
    for (i = 0; i < n; i++) b[i] = (char)(((base + i) * 31) & 255);
}

static int verifica(UFILE *f, int ini, int n) {
    char b[256];
    int off = ini, bien = 1;
    u_fseek(f, ini, U_INI);
    while (off < ini + n && bien) {
        int q = ini + n - off;
        int r, i;
        if (q > 256) q = 256;
        r = u_fread(b, q, f);
        if (r <= 0) { bien = 0; break; }
        for (i = 0; i < r; i++) {
            if (b[i] != (char)(((off + i) * 31) & 255)) {
                bien = 0;
                break;
            }
        }
        off += r;
    }
    return bien;
}

void escena_ficheros(void) {
    UFILE *f;
    char b[512];
    int i;
    fallos = 0;
    d_titulo("ficheros multisector (8 KB)");
    u_remove("demo_big");
    f = u_fopen("demo_big", "w");
    if (!f) {
        d_info("no se pudo crear");
        return;
    }
    ok(u_fijar_tam(f, 8192) == 0, "pre-8k");
    for (i = 0; i < 16; i++) {
        rellena(b, i * 512, 512);
        if (u_fwrite(b, 512, f) != 512) break;
    }
    ok(i == 16 && u_ftell(f) == 8192, "escritura-8k");
    ok(u_fsize(f) == 8192, "tam-8k");
    u_fclose(f);
    f = u_fopen("demo_big", "r");
    if (!f) {
        d_info("no se pudo abrir");
        return;
    }
    ok(verifica(f, 0, 8192), "lectura-total");
    ok(verifica(f, 4096, 2048), "ventana-4k");
    ok(u_fseek(f, -100, U_FIN) == 0, "seek-fin");
    ok(u_ftell(f) == 8092, "tell-fin");
    /* parche por offset: 300 bytes en 5000 */
    u_fseek(f, 5000, U_INI);
    for (i = 0; i < 300; i++) b[i] = (char)(i + 1);
    ok(u_fwrite(b, 300, f) == 300, "parche-300");
    u_fseek(f, 4990, U_INI);
    ok(u_fread(b, 320, f) == 320, "relee-parche");
    {
        int bien = 1;
        for (i = 0; i < 320; i++) {
            int pos = 4990 + i;
            char esp = (pos >= 5000 && pos < 5300) ? (char)(pos - 5000 + 1) :
                (char)((pos * 31) & 255);
            if (b[i] != esp) { bien = 0; break; }
        }
        ok(bien, "parche-ok");
    }
    ok(u_fsize(f) == 8192, "tam-tras-parche");
    u_fclose(f);
    /* anexado */
    f = u_fopen("demo_big", "a");
    if (f) {
        rellena(b, 8192, 128);
        ok(u_fwrite(b, 128, f) == 128, "anexa-128");
        ok(u_fsize(f) == 8320, "tam-8320");
        u_fclose(f);
    } else ok(0, "abrir-anexar");
    /* copia con la shell? no: verifica tam final y borra */
    ok(u_remove("demo_big") == 0, "borra-demo");
    ok(u_fopen("demo_big", "r") == 0, "borra-efecto");
    u_printf("ficheros: %d fallos\n", fallos);
}
