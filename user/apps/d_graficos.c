#include "demo16.h"

/* Escenas graficas con doble bufer: todo se dibuja en el dorso y se
   presenta por regiones. Verificacion por lectura de pixeles. */

static int fallos = 0;

static void ok(int cond, const char *nom) {
    if (!cond) {
        fallos++;
        u_printf("FALLO gfx %s\n", nom);
    }
}

static USPRITE *cuadro(int w, int h, unsigned int c0, unsigned int c1) {
    USPRITE *s = u_malloc(sizeof(USPRITE) + w * h * 4);
    int x, y;
    if (!s) return 0;
    s->w = (unsigned int)w;
    s->h = (unsigned int)h;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            unsigned int c = ((x / 8 + y / 8) % 2) ? c0 : c1;
            if (x < 2 || y < 2 || x >= w - 2 || y >= h - 2)
                c = U_RGB(255, 255, 255);
            s->pix[y * w + x] = c;
        }
    }
    return s;
}

static void escena1_mosaico(void) {
    int i;
    d_titulo("mosaico");
    u_destino(1);
    u_limpiar(C_NEGRO);
    for (i = 0; i < 16; i++) {
        int x = 20 + (i % 8) * 95;
        int y = 60 + (i / 8) * 120;
        u_rectangulo(x, y, 80, 100, i);
        u_texto(x + 4, y + 108, "c", i ^ 15);
    }
    u_linea(0, 40, 799, 40, C_AMARILLO);
    u_texto(300, 12, "MOSAICO 16 COLORES", C_BLANCO);
    u_presentar(0, 0, 800, 320);
    ok(u_punto(30, 70) == 0x000000, "mosaico-negro");
    ok(u_punto(20 + 7 * 95 + 10, 60 + 120 + 10) == 0xFFFFFF, "mosaico-blanco");
    d_info("mosaico presentado (0,0,800,320)");
}

static void escena2_rebote(void) {
    USPRITE *a = cuadro(48, 48, U_RGB(255, 0, 0), U_RGB(128, 0, 0));
    USPRITE *b = cuadro(48, 48, U_RGB(0, 255, 0), U_RGB(0, 128, 0));
    USPRITE *c = cuadro(48, 48, U_RGB(0, 0, 255), U_RGB(0, 0, 128));
    int x1 = 0, y1 = 350, dx1 = 5, dy1 = 3;
    int x2 = 700, y2 = 350, dx2 = -7, dy2 = 4;
    int x3 = 350, y3 = 500, dx3 = 6, dy3 = -5;
    int f, t0, dt;
    d_titulo("rebote (120 cuadros)");
    t0 = u_ticks();
    for (f = 0; f < 120; f++) {
        u_limpiar(C_NEGRO);
        u_sprite(x1, y1, a);
        u_sprite(x2, y2, b);
        u_sprite(x3, y3, c);
        u_presentar(0, 340, 800, 220);
        x1 += dx1; y1 += dy1;
        x2 += dx2; y2 += dy2;
        x3 += dx3; y3 += dy3;
        if (x1 < 0 || x1 > 752) dx1 = -dx1;
        if (y1 < 340 || y1 > 552) dy1 = -dy1;
        if (x2 < 0 || x2 > 752) dx2 = -dx2;
        if (y2 < 340 || y2 > 552) dy2 = -dy2;
        if (x3 < 0 || x3 > 752) dx3 = -dx3;
        if (y3 < 340 || y3 > 552) dy3 = -dy3;
        u_dormir(16);
    }
    dt = u_ticks() - t0;
    u_printf("120 cuadros en %d ticks (%d/10 cuadros)\n", dt, dt / 12);
    ok(dt > 0 && dt < 3000, "rebote-tiempo");
    u_free(a);
    u_free(b);
    u_free(c);
}

static void escena3_oleaje(void) {
    int f, x;
    d_titulo("oleaje con blit");
    u_limpiar(C_AZUL);
    for (x = 0; x < 800; x += 40)
        u_linea(x, 580, x + 20, 420, C_CYAN_CLARO);
    u_presentar(0, 400, 800, 200);
    for (f = 0; f < 60; f++) {
        /* desplaza la franja a la izquierda y dibuja columna nueva */
        u_blit(0, 420, 799, 160, -1, 0);
        {
            int h = 420 + (int)(d_azar() % 120);
            u_linea(799, 580, 799, h, (f % 2) ? C_AMARILLO : C_BLANCO);
        }
        u_presentar(0, 420, 800, 160);
        u_dormir(33);
    }
    d_info("oleaje: 60 pasos con blit [-1,0]");
    u_pixel(700, 500, C_ROJO);
    u_presentar(700, 500, 1, 1);
    ok(u_punto(700, 500) == 0xAA0000, "oleaje-testigo");
}

static void escena4_transparencias(void) {
    USPRITE *fondo = cuadro(200, 120, U_RGB(64, 64, 64), U_RGB(32, 32, 32));
    USPRITE *nave;
    int i;
    d_titulo("transparencias");
    nave = u_malloc(sizeof(USPRITE) + 40 * 24 * 4);
    if (!nave) {
        d_info("sin memoria");
        u_free(fondo);
        return;
    }
    nave->w = 40;
    nave->h = 24;
    for (i = 0; i < 40 * 24; i++) nave->pix[i] = U_RGB(255, 0, 255);
    for (i = 8; i < 32; i++) {
        int y;
        for (y = 4; y < 20; y++) nave->pix[y * 40 + i] = U_RGB(0, 255, 255);
    }
    nave->pix[11 * 40 + 30] = U_RGB(255, 255, 255);
    u_sprite(100, 100, fondo);
    u_sprite_t(140, 130, nave, U_RGB(255, 0, 255));
    u_presentar(96, 96, 210, 140);
    ok(u_punto(100, 100) == U_RGB(255, 255, 255), "nave-fondo-borde");
    ok(u_punto(150, 140) == U_RGB(0, 255, 255), "nave-cuerpo");
    d_info("nave sobre fondo (clave magenta)");
    u_free(fondo);
    u_free(nave);
}

void escena_graficos(void) {
    fallos = 0;
    u_destino(1);
    escena1_mosaico();
    escena2_rebote();
    escena3_oleaje();
    escena4_transparencias();
    u_destino(0);
    u_printf("graficos: %d fallos\n", fallos);
}
