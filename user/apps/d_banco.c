#include "demo16.h"

/* Banco de rendimiento: syscalls, memoria, graficos. Tiempos en ticks
   (100 Hz). Sin umbrales estrictos salvo cordura (no cuelgues). */

static void b_syscalls(void) {
    int t0, dt, i;
    d_titulo("banco: syscalls");
    t0 = u_ticks();
    for (i = 0; i < 200; i++) u_print(".");
    dt = u_ticks() - t0;
    u_printf("\n200 imprimir: %d ticks\n", dt);
    t0 = u_ticks();
    for (i = 0; i < 200; i++) u_ticks();
    dt = u_ticks() - t0;
    u_printf("200 ticks: %d ticks\n", dt);
}

static void b_memoria(void) {
    int t0, dt, i;
    char *p[100];
    d_titulo("banco: memoria");
    t0 = u_ticks();
    for (i = 0; i < 100; i++) {
        p[i] = u_malloc(256);
        if (p[i]) u_memset(p[i], i & 255, 256);
    }
    for (i = 0; i < 100; i++) u_free(p[i]);
    dt = u_ticks() - t0;
    u_printf("100 malloc+free 256 B: %d ticks\n", dt);
    t0 = u_ticks();
    for (i = 0; i < 20; i++) {
        int a = sys_memoria(1024);
        (void)a;
    }
    dt = u_ticks() - t0;
    u_printf("20 sbrk 1 KB: %d ticks (fuga a proposito: se resetea)\n", dt);
}

static void b_graficos(void) {
    int t0, dt, i;
    d_titulo("banco: graficos");
    u_destino(1);
    t0 = u_ticks();
    for (i = 0; i < 50; i++) u_limpiar(i % 2 ? C_AZUL : C_NEGRO);
    dt = u_ticks() - t0;
    u_printf("50 limpiar 800x600: %d ticks\n", dt);
    t0 = u_ticks();
    for (i = 0; i < 50; i++) u_presentar(0, 0, 800, 600);
    dt = u_ticks() - t0;
    u_printf("50 presentar: %d ticks\n", dt);
    t0 = u_ticks();
    for (i = 0; i < 1000; i++) u_punto(i % 800, (i * 7) % 600);
    dt = u_ticks() - t0;
    u_printf("1000 leer_pixel: %d ticks\n", dt);
    u_destino(0);
}

static void b_ficheros(void) {
    UFILE *f;
    char b[512];
    int t0, dt, i;
    d_titulo("banco: ficheros");
    u_remove("demo_bench");
    t0 = u_ticks();
    f = u_fopen("demo_bench", "w");
    if (!f) {
        d_info("no crea");
        return;
    }
    u_fijar_tam(f, 16384);
    u_memset(b, 0x5A, 512);
    for (i = 0; i < 32; i++) u_fwrite(b, 512, f);
    u_fclose(f);
    dt = u_ticks() - t0;
    u_printf("escribir 16 KB: %d ticks\n", dt);
    t0 = u_ticks();
    f = u_fopen("demo_bench", "r");
    for (i = 0; i < 32; i++) u_fread(b, 512, f);
    u_fclose(f);
    dt = u_ticks() - t0;
    u_printf("leer 16 KB: %d ticks\n", dt);
    u_remove("demo_bench");
}

void escena_banco(void) {
    b_syscalls();
    b_memoria();
    b_graficos();
    b_ficheros();
    u_print("banco fin.\n");
}
