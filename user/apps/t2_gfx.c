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

/* bandas 0..15 de 50 px: y=100..899? no: 16*30=480, y0=60 */
static void bandas(void) {
    int c;
    for (c = 0; c < 16; c++) u_rectangulo(0, 60 + c * 30, 800, 30, c);
}

static USPRITE *haz_sprite(int w, int h, unsigned int a, unsigned int b,
                           unsigned int borde) {
    USPRITE *s = u_malloc(sizeof(USPRITE) + w * h * 4);
    int x, y;
    if (!s) return 0;
    s->w = w;
    s->h = h;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            unsigned int c = ((x / 4 + y / 4) % 2) ? a : b;
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) c = borde;
            s->pix[y * w + x] = c;
        }
    }
    return s;
}

static void t_dorso(void) {
    u_print("dorso...\n");
    u_destino(1);
    u_limpiar(C_NEGRO);
    bandas();
    u_linea(0, 0, 799, 59, C_BLANCO);
    u_linea(799, 0, 0, 59, C_ROJO);
    u_texto(8, 8, "t2_gfx v1.6", C_AMARILLO);
    u_pixel(799, 599, C_BLANCO);
    u_presentar(0, 0, 800, 600);
    ok(u_punto(10, 65) == 0x000000, "banda0-negro");
    ok(u_punto(10, 95) == 0x0000AA, "banda1-azul");
    ok(u_punto(799, 599) == 0xFFFFFF, "esquina-blanca");
}

static void t_sprites(void) {
    USPRITE *s = haz_sprite(32, 32, U_RGB(255, 0, 0), U_RGB(255, 255, 255),
                            U_RGB(0, 0, 255));
    USPRITE *t;
    u_print("sprites...\n");
    ok(s != 0, "sprite-malloc");
    u_sprite(100, 200, s);
    u_presentar(96, 196, 40, 40);
    ok(u_punto(100, 200) == U_RGB(0, 0, 255), "sprite-borde");
    ok(u_punto(104, 204) == U_RGB(255, 0, 0) ||
       u_punto(104, 204) == U_RGB(255, 255, 255), "sprite-dentro");
    /* transparente: esquinas magenta -> se ve el fondo (banda) */
    t = haz_sprite(24, 24, U_RGB(0, 255, 0), U_RGB(255, 0, 255),
                   U_RGB(255, 0, 255));
    ok(t != 0, "sprite-t-malloc");
    u_sprite_t(400, 70, t, U_RGB(255, 0, 255));
    u_presentar(396, 66, 32, 32);
    ok(u_punto(400, 70) == 0x000000, "transparente-fondo");
    ok(u_punto(408, 74) == U_RGB(0, 255, 0), "transparente-verde");
    u_free(s);
    u_free(t);
}

static void t_blit(void) {
    unsigned int antes, despues;
    u_print("blit...\n");
    u_rectangulo(500, 300, 60, 40, C_CYAN_CLARO);
    antes = u_punto(510, 310);
    u_blit(500, 300, 60, 40, 600, 400);
    u_presentar(596, 396, 68, 48);
    despues = u_punto(610, 410);
    ok(antes == despues && antes != 0, "blit-copia");
    /* solape: mueve sobre si mismo */
    u_blit(600, 400, 60, 40, 620, 400);
    u_presentar(616, 396, 68, 48);
    ok(u_punto(625, 410) == antes, "blit-solape");
}

static void t_anim(void) {
    USPRITE *s = haz_sprite(16, 16, U_RGB(255, 255, 0), U_RGB(0, 0, 0),
                            U_RGB(255, 255, 255));
    int x = 0, dx = 6, f, t0, dt;
    u_print("anim (30 cuadros)...\n");
    t0 = u_ticks();
    for (f = 0; f < 30; f++) {
        u_limpiar(C_NEGRO);
        u_sprite(x, 550, s);
        u_presentar(0, 540, 800, 60);
        x += dx;
        if (x > 780 || x < 0) dx = -dx;
        u_dormir(16);
    }
    dt = u_ticks() - t0;
    u_printf("30 cuadros en %d ticks\n", dt);
    ok(dt >= 10 && dt <= 600, "anim-tiempo");
    u_free(s);
    u_destino(0);
}

int main(void) {
    u_print("t2_gfx v1.6\n");
    t_dorso();
    t_sprites();
    t_blit();
    t_anim();
    u_printf("t2: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos > 99 ? 99 : fallos;
}
