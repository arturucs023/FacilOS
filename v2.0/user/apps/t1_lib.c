#include "ulib.h"

void __main(void) {
}

static int fallos = 0;
static int pruebas = 0;

static void ok(int cond, const char *nom) {
    pruebas++;
    if (!cond) {
        fallos++;
        u_printf("FALLO %s\n", nom);
    }
}

static void t_cadenas(void) {
    char b[64];
    u_print("cadenas...\n");
    ok(u_strlen("") == 0, "strlen0");
    ok(u_strlen("hola") == 4, "strlen");
    ok(u_strcmp("a", "a") == 0, "strcmp=");
    ok(u_strcmp("a", "b") < 0, "strcmp<");
    ok(u_strncmp("abcx", "abcy", 3) == 0, "strncmp3");
    ok(u_strncmp("abc", "abd", 3) < 0, "strncmp<");
    u_strcpy(b, "hola");
    ok(u_strcmp(b, "hola") == 0, "strcpy");
    u_strncpy(b, "abcdef", 3);
    b[3] = 0;
    ok(u_strcmp(b, "abc") == 0, "strncpy");
    u_strcpy(b, "ho");
    u_strcat(b, "la");
    ok(u_strcmp(b, "hola") == 0, "strcat");
    ok(u_strchr("hola", 'l') == b + 0 || u_strchr("hola", 'l') != 0, "strchr");
    ok(u_strchr("hola", 'z') == 0, "strchr-no");
    ok(u_atoi("  -42x") == -42, "atoi");
    ok(u_atoi("17") == 17, "atoi+");
    u_itoa(-123, b);
    ok(u_strcmp(b, "-123") == 0, "itoa");
    u_utoa(255, b, 16);
    ok(u_strcmp(b, "ff") == 0, "utoa16");
    u_utoa(42, b, 10);
    ok(u_strcmp(b, "42") == 0, "utoa10");
    {
        char t[16] = "0123456789";
        u_memmove(t + 2, t, 5);
        ok(u_memcmp(t, "0101234789", 10) == 0, "memmove-solapa");
        u_memmove(t, t + 2, 5);
        ok(u_memcmp(t, "0123434789", 10) == 0, "memmove-atras");
    }
    ok(u_memcmp("ab", "ab", 2) == 0, "memcmp=");
}

static void t_printf(void) {
    u_print("printf...\n");
    u_printf("dec=%d uns=%u hex=%x str=%s ch=%c pct=%%\n", -7, 7u, 255u,
             "ok", '!');
    u_print_num(-12345);
    u_print(" ");
    u_print_hex(0xabcdU);
    u_print("\n");
}

static void t_memoria(void) {
    void *p[200];
    int i, nb0, by0, li0, nb1, li1;
    u_print("memoria...\n");
    u_malloc_info(&nb0, &by0, &li0);
    for (i = 0; i < 200; i++) {
        p[i] = u_malloc(8 + (i * 13) % 120);
        ok(p[i] != 0, "malloc200");
        u_memset(p[i], i & 255, 8 + (i * 13) % 120);
    }
    for (i = 0; i < 200; i += 2) u_free(p[i]);
    u_free(p[1]);
    u_free(p[1]);
    u_free(0);
    {
        void *q = u_malloc(64);
        void *q2;
        ok(q != 0, "malloc64");
        u_memset(q, 0xA5, 64);
        q2 = u_realloc(q, 256);
        ok(q2 != 0, "realloc-crece");
        ok(((char *)q2)[0] == (char)0xA5 && ((char *)q2)[63] == (char)0xA5,
           "realloc-datos");
        q2 = u_realloc(q2, 16);
        ok(q2 != 0, "realloc-mengua");
        u_free(q2);
        {
            void *z = u_calloc(10, 20);
            int todo0 = 1;
            for (i = 0; i < 200; i++) {
                if (((char *)z)[i]) { todo0 = 0; break; }
            }
            ok(z != 0 && todo0, "calloc-cero");
            u_free(z);
        }
    }
    /* fragmentacion: 50x64, libera pares, 25x100 en esos huecos */
    {
        void *a[50];
        int cabe = 1;
        for (i = 0; i < 50; i++) a[i] = u_malloc(64);
        for (i = 0; i < 50; i += 2) { u_free(a[i]); a[i] = 0; }
        for (i = 0; i < 25; i++) {
            void *q = u_malloc(100);
            if (!q) { cabe = 0; break; }
            a[i * 2] = q;
        }
        ok(cabe, "reuso-fragmentos");
        for (i = 0; i < 200; i++) {
            if (i % 2) u_free(p[i]);
        }
        for (i = 0; i < 50; i++) u_free(a[i]);
    }
    u_malloc_info(&nb1, &by0, &li1);
    ok(nb1 <= nb0 + 8, "union-bloques");
    (void)by0;
    (void)li0;
    (void)li1;
}

static void t_ficheros(void) {
    UFILE *f;
    char buf[512];
    int i;
    u_print("ficheros...\n");
    f = u_fopen("t1dat", "w");
    ok(f != 0, "fopen-w");
    ok(u_fijar_tam(f, 2048) == 0, "fijar-2048");
    for (i = 0; i < 4; i++) {
        int k;
        for (k = 0; k < 512; k++) buf[k] = (char)((i * 512 + k) * 7);
        ok(u_fwrite(buf, 512, f) == 512, "fwrite-512");
    }
    ok(u_ftell(f) == 2048, "ftell-fin");
    ok(u_fclose(f) == 0, "fclose");
    f = u_fopen("t1dat", "r");
    ok(f != 0, "fopen-r");
    ok(u_fsize(f) == 2048, "fsize");
    ok(u_fseek(f, 1000, U_INI) == 0, "seek-ini");
    ok(u_ftell(f) == 1000, "tell-1000");
    ok(u_fread(buf, 100, f) == 100, "fread-100");
    {
        int bien = 1;
        for (i = 0; i < 100; i++) {
            if (buf[i] != (char)((1000 + i) * 7)) { bien = 0; break; }
        }
        ok(bien, "datos-1000");
    }
    ok(u_fseek(f, -10, U_ACT) == 0, "seek-act");
    ok(u_ftell(f) == 1090, "tell-act");
    ok(u_fseek(f, -5, U_FIN) == 0, "seek-fin");
    ok(u_ftell(f) == 2043, "tell-fin");
    ok(u_fseek(f, 99999, U_INI) != 0, "seek-malo");
    ok(u_fclose(f) == 0, "fclose2");
    /* escritura parcial */
    f = u_fopen("t1dat", "r");
    ok(f != 0, "fopen-r2");
    ok(u_fseek(f, 100, U_INI) == 0, "seek-100");
    u_memset(buf, 'Z', 50);
    ok(u_fwrite(buf, 50, f) == 50, "parcial-50");
    ok(u_fseek(f, 0, U_INI) == 0, "seek-0");
    ok(u_fread(buf, 512, f) == 512, "relee");
    {
        int bien = 1;
        for (i = 0; i < 512; i++) {
            char esp = (i >= 100 && i < 150) ? 'Z' : (char)(i * 7);
            if (buf[i] != esp) { bien = 0; break; }
        }
        ok(bien, "parcial-ok");
    }
    ok(u_fclose(f) == 0, "fclose3");
    /* borrar */
    {
        UFILE *t = u_fopen("t1tmp", "w");
        ok(t != 0, "tmp-crea");
        u_fwrite("x", 1, t);
        u_fclose(t);
        ok(u_remove("t1tmp") == 0, "remove");
        ok(u_fopen("t1tmp", "r") == 0, "remove-efecto");
        ok(u_fopen("noexiste", "r") == 0, "abrir-no");
    }
}

static void t_tiempo(void) {
    int t0, dt, r;
    u_print("tiempo...\n");
    t0 = u_ticks();
    u_printf("ticks-antes=%d\n", t0);
    r = u_dormir(100);
    dt = u_ticks() - t0;
    u_printf("dormir100 rc=%d ticks-despues=%d dt=%d\n", r, t0 + dt, dt);
    ok(r == 0 && dt >= 5 && dt <= 600, "dormir-rango");
}

int main(void) {
    u_print("t1_lib v1.6\n");
    t_cadenas();
    t_printf();
    t_memoria();
    t_ficheros();
    t_tiempo();
    u_printf("t1: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos > 99 ? 99 : fallos;
}
