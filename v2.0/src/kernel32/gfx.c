#include "gfx.h"
#include "bootinfo.h"

/* Consola grafica LFB 32bpp (XRGB). Fuente 8x16 copiada por el loader.
   Misma logica que video_glifo 16-bit: celdas opacas + cursor XOR. */
static uint32_t lfb = 0;
static uint16_t GW = 0, GH = 0, GP = 0;
static uint16_t gcx = 0, gcy = 0;
static int cur_vis = 0;

/* v2.1-P0: color extendido. Si el bit 24 viene a 1, los 24 bits bajos
   son RGB crudo (0x01000000|rrggbb); si no, paleta VGA 0-15 (compat). */
static uint32_t color32(uint32_t c) {
    static const uint32_t tab[16] = {
        0x000000u, 0x0000AAu, 0x00AA00u, 0x00AAAAu,
        0xAA0000u, 0xAA00AAu, 0xAAAA00u, 0xAAAAAAu,
        0x000000u, 0x0000FFu, 0x00FF00u, 0x00FFFFu,
        0xFF0000u, 0xFF00FFu, 0xFFFF00u, 0xFFFFFFu
    };
    if (c & 0x01000000u) return c & 0x00FFFFFFu;
    return tab[c & 0x0Fu];
}

static uint32_t rgb(uint8_t attr) {
    return color32((uint32_t)attr);
}

/* v1.9: el arranque dejo la fuente 8x16 del BIOS en su direccion propia
   (0x701A/0x701C) porque el sector de arranque ya no tenia sitio; aqui
   se copia a 0x6000, que es donde la leen glifo() y la consola. */
static void copia_fuente(void) {
    uint32_t dir = ((uint32_t)(*BI_FONT_SEG) << 4) + (uint32_t)(*BI_FONT_OFF);
    if (dir < 0x1000) return;                 /* no la devolvio el BIOS */
    volatile uint8_t *src = (volatile uint8_t *)dir;
    volatile uint8_t *dst = FONT_BIN;
    for (int i = 0; i < 4096; i++) dst[i] = src[i];
}

void gfx_init(void) {
    copia_fuente();
    if (BI->vmode != 1 || BI->bpp != 32) return;
    lfb = (uint32_t)BI->lfb_lo | ((uint32_t)BI->lfb_hi << 16);
    if (!lfb) return;
    GW = BI->w; GH = BI->h; GP = BI->pitch;
    if (GW < 640 || GH < 400 || !GP) { lfb = 0; return; }
    gcx = gcy = 0;
    cur_vis = 0;
    gfx_clear();
}

int gfx_activo(void) { return lfb != 0; }

int pantalla_init(void) {
    gfx_init();
    return gfx_activo() ? 0 : -1;
}
uint16_t gfx_w(void) { return GW; }
uint16_t gfx_h(void) { return GH; }

void gfx_clear(void) {
    if (!lfb) return;
    volatile uint32_t *p = (volatile uint32_t *)lfb;
    uint32_t n = (uint32_t)GP * GH / 4;
    for (uint32_t i = 0; i < n; i++) p[i] = 0;
    gcx = gcy = 0;
    cur_vis = 0;
}

/* v2.2: texto Unicode. La fuente BIOS es CP437 (verificada glifo a
   glifo contra el video real: build/font_*.png). Solo se mapean los
   codepoints verificados; el resto -> '?' (documentado en
   DISENO-UNICODE-v2.2.md). U+20AC (€) es dibujo propio en glifo_euro
   (la fuente no lo trae). Bytes sueltos 0x80-0xFF (UTF-8 invalido) se
   interpretan CP437 por compatibilidad con volumenes v1.x. */
static uint8_t glifo_euro[16] = {
    0x3C, 0x60, 0x40, 0x7C, 0x40, 0x40, 0x7C, 0x40,
    0x40, 0x7C, 0x40, 0x40, 0x60, 0x3C, 0x00, 0x00
};

/* Latin-1 -> slot CP437 (0 = sin glifo). Solo entradas verificadas
   contra la fuente real (build/font_*.png). */
static uint8_t l1_glifo(unsigned cp) {
    static const uint8_t t_c0[16] = {
        0xB7, 0xB5, 0xB6, 0xC6, 0x8E, 0x8F, 0x92, 0x80,
        0xD4, 0x90, 0xD2, 0xD3, 0xDE, 0xD6, 0xD7, 0xD8
    };
    static const uint8_t t_d0[16] = {
        0xD0, 0xA5, 0, 0, 0, 0, 0x99, 0,
        0, 0, 0x9A, 0, 0, 0, 0, 0xE1
    };
    static const uint8_t t_e0[16] = {
        0x85, 0xA0, 0x83, 0xC6, 0x84, 0x86, 0x91, 0x87,
        0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B
    };
    static const uint8_t t_f0[16] = {
        0xD0, 0xA4, 0x95, 0xA2, 0x93, 0x94, 0, 0,
        0x97, 0xA3, 0x96, 0x81, 0, 0, 0, 0x98
    };
    if (cp >= 0xC0 && cp <= 0xCF) return t_c0[cp - 0xC0];
    if (cp >= 0xD0 && cp <= 0xDF) return t_d0[cp - 0xD0];
    if (cp >= 0xE0 && cp <= 0xEF) return t_e0[cp - 0xE0];
    if (cp >= 0xF0 && cp <= 0xFF) return t_f0[cp - 0xF0];
    switch (cp) {
    case 0xA0: return 0xFF;
    case 0xA1: return 0xAD;
    case 0xA2: return 0x9B;
    case 0xA3: return 0x9C;
    case 0xA5: return 0x9D;
    case 0xA9: return 0xB8;
    case 0xAA: return 0xA6;
    case 0xAB: return 0xAE;
    case 0xB7: return 0xFA;
    case 0xBB: return 0xAF;
    case 0xBF: return 0xA8;
    default: break;
    }
    return 0;
}

/* Resuelve codepoint a bytes de glifo (16 B). */
static const uint8_t *glifo_cp(unsigned cp) {
    static const uint8_t glifo_q[16] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x3C, 0x42, 0x42, 0x00, 0x3C, 0x00
    };
    if (cp < 0x80) return FONT_BIN + cp * 16;
    if (cp == 0x20AC) return glifo_euro;
    if (cp >= 0x80 && cp < 0x100) {
        uint8_t s = l1_glifo(cp);
        if (s) return FONT_BIN + (unsigned)s * 16;
        return glifo_q;
    }
    return glifo_q;
}

/* Decodificador UTF-8 del nucleo (misma semantica que utext Ring3).
   Secuencias invalidas: byte suelto 0x80-0xFF -> marca CP437 directo
   (0x100|byte, compat volumenes v1.x); resto -> U+FFFD ('?'). */
static unsigned utf8_dec(const char *s, int max, int *av) {
    unsigned char c0;
    unsigned cp;
    int n = 0, i;
    static const unsigned char msk[5] = { 0, 0, 0x1F, 0x0F, 0x07 };
    if (max <= 0) {
        if (av) *av = 0;
        return 0xFFFDu;
    }
    c0 = (unsigned char)s[0];
    if (c0 < 0x80) {
        if (av) *av = 1;
        return c0;
    }
    if (c0 < 0xC2) n = 0;
    else if (c0 < 0xE0) n = 2;
    else if (c0 < 0xF0) n = 3;
    else if (c0 < 0xF5) n = 4;
    if (!n || n > max) {
        if (av) *av = 1;
        return 0x100u | c0;
    }
    cp = c0 & msk[n];
    for (i = 1; i < n; i++) {
        unsigned char cc = (unsigned char)s[i];
        if ((cc & 0xC0) != 0x80) {
            if (av) *av = 1;
            return 0x100u | c0;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }
    if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) ||
        (n == 4 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF) ||
        cp > 0x10FFFF) {
        if (av) *av = 1;
        return 0x100u | c0;
    }
    if (av) *av = n;
    return cp;
}

/* Glifo desde codepoint o marca CP437 (0x100|byte). */
static const uint8_t *glifo_de(unsigned v) {
    if (v & 0x100u) return FONT_BIN + (v & 0xFFu) * 16;
    return glifo_cp(v);
}

static void glifo(int x, int y, char c, uint8_t attr) {
    uint32_t fg = rgb(attr);
    uint8_t *f = FONT_BIN + (uint8_t)c * 16;
    for (int r = 0; r < 16; r++) {
        volatile uint32_t *p =
            (volatile uint32_t *)(lfb + (y + r) * GP + x * 4);
        uint8_t bits = f[r];
        for (int col = 0; col < 8; col++) {
            p[col] = (bits & (0x80 >> col)) ? fg : 0;
        }
    }
}

static void cursor_xor(void) {
    for (int r = 14; r < 16; r++) {
        volatile uint32_t *p =
            (volatile uint32_t *)(lfb + (gcy * 16 + r) * GP + gcx * 8 * 4);
        for (int col = 0; col < 8; col++) p[col] ^= 0x00FFFFFF;
    }
}

static void cursor_borra(void) {
    if (cur_vis) { cursor_xor(); cur_vis = 0; }
}

static void cursor_dibuja(void) {
    if (!cur_vis) { cursor_xor(); cur_vis = 1; }
}

static void gfx_scroll(void) {
    volatile uint32_t *base = (volatile uint32_t *)lfb;
    uint32_t fila4 = (uint32_t)GP * 16 / 4;
    uint32_t filas = GH / 16;
    for (uint32_t i = 0; i < (filas - 1) * fila4; i++)
        base[i] = base[i + fila4];
    for (uint32_t i = (filas - 1) * fila4; i < filas * fila4; i++)
        base[i] = 0;
}

void gfx_putc(char c, uint8_t attr) {
    if (!lfb) return;
    uint16_t cols = GW / 8, filas = GH / 16;
    cursor_borra();
    if (c == '\r') {
        gcx = 0;
    } else if (c == '\n') {
        gcx = 0; gcy++;
        if (gcy >= filas) { gfx_scroll(); gcy = filas - 1; }
    } else if (c == '\b') {
        if (gcx > 0) {
            gcx--;
            glifo(gcx * 8, gcy * 16, ' ', 0x07);
        }
    } else {
        glifo(gcx * 8, gcy * 16, c, attr);
        gcx++;
        if (gcx >= cols) {
            gcx = 0; gcy++;
            if (gcy >= filas) { gfx_scroll(); gcy = filas - 1; }
        }
    }
    cursor_dibuja();
}

void gfx_puts(const char *s) {
    while (*s) gfx_putc(*s++, 0x07);
}

/* v2.2: dibuja un codepoint ya decodificado (ver cons_putc). */
void gfx_putcp(unsigned cp, uint8_t attr) {
    const uint8_t *f;
    int r, col;
    uint16_t cols, filas;
    if (!lfb) return;
    cols = GW / 8;
    filas = GH / 16;
    cursor_borra();
    f = glifo_de(cp);
    for (r = 0; r < 16; r++) {
        volatile uint32_t *p = (volatile uint32_t *)
            (lfb + (gcy * 16 + r) * GP + gcx * 8 * 4);
        uint8_t bits = f[r];
        for (col = 0; col < 8; col++)
            p[col] = (bits & (0x80 >> col)) ? rgb(attr) : 0;
    }
    gcx++;
    if (gcx >= cols) {
        gcx = 0; gcy++;
        if (gcy >= filas) { gfx_scroll(); gcy = filas - 1; }
    }
    cursor_dibuja();
}

/* v2.2: codepoint a byte CP437 para consola de texto (o '?' ). */
uint8_t cp_a_cp437(unsigned cp) {
    uint8_t s;
    if (cp < 0x80) return (uint8_t)cp;
    if (cp == 0x20AC) return (uint8_t)'?';   /* sin celda en texto */
    if (cp < 0x100) {
        s = l1_glifo(cp);
        return s ? s : (uint8_t)'?';
    }
    return (uint8_t)'?';
}

/* ---- primitivas v1.5: solo 32bpp XRGB, clipping con signo ---- */
/* v1.6: destino de dibujo. La consola (glifo/putc/scroll) usa lfb fijo. */
#define DORSO_BASE ((uint32_t)0xA00000)
static uint32_t dorso = 0;
static int gdest = 0;
static uint32_t base_dib(void) {
    return (gdest && dorso) ? dorso : lfb;
}

void g_destino(int d) {
    if (!lfb) { gdest = 0; return; }
    if (d && !dorso) {
        dorso = DORSO_BASE;
        uint32_t n = (uint32_t)GP * GH / 4;
        volatile uint32_t *p = (volatile uint32_t *)dorso;
        for (uint32_t i = 0; i < n; i++) p[i] = 0;
    }
    gdest = d ? 1 : 0;
}

/* v1.9: cuando el dueno del framebuffer cambia, el dorso se pone a negro
   para que el proceso nuevo no mezcle su dibujo con el del anterior. */
void g_dorso_limpia(void) {
    if (!dorso) return;
    volatile uint32_t *p = (volatile uint32_t *)dorso;
    uint32_t n = (uint32_t)GP * GH / 4;
    for (uint32_t i = 0; i < n; i++) p[i] = 0;
}

void g_presentar(int x, int y, int w, int h) {
    if (!lfb || !dorso || w <= 0 || h <= 0) return;
    if (!gdest) return;   /* v2.0-etapa6: presentar solo vuelca el dorso;
                             en frente es no-op (antes tapaba en negro la
                             ventana recien dibujada cuando el dorso existe
                             por otro cliente). */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GW ? GW : x + w;
    int y1 = y + h > GH ? GH : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *d = (uint32_t *)(lfb + (uint32_t)yy * GP +
                                   (uint32_t)x0 * 4);
        uint32_t *s = (uint32_t *)(dorso + (uint32_t)yy * GP +
                                   (uint32_t)x0 * 4);
        uint32_t n = (uint32_t)(x1 - x0);
        for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    }
}

void g_sprite(int x, int y, int w, int h, const uint32_t *pix) {
    uint32_t b = base_dib();
    if (!lfb || !pix || w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GW ? GW : x + w;
    int y1 = y + h > GH ? GH : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        volatile uint32_t *d = (volatile uint32_t *)
            (b + (uint32_t)yy * GP + (uint32_t)x0 * 4);
        const uint32_t *s = pix + (uint32_t)(yy - y) * (uint32_t)w +
                            (uint32_t)(x0 - x);
        uint32_t n = (uint32_t)(x1 - x0);
        for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    }
}

void g_sprite_t(int x, int y, int w, int h, const uint32_t *pix,
                uint32_t clave) {
    uint32_t b = base_dib();
    if (!lfb || !pix || w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GW ? GW : x + w;
    int y1 = y + h > GH ? GH : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        volatile uint32_t *d = (volatile uint32_t *)
            (b + (uint32_t)yy * GP + (uint32_t)x0 * 4);
        const uint32_t *s = pix + (uint32_t)(yy - y) * (uint32_t)w +
                            (uint32_t)(x0 - x);
        uint32_t n = (uint32_t)(x1 - x0);
        for (uint32_t i = 0; i < n; i++)
            if (s[i] != clave) d[i] = s[i];
    }
}

void g_blit(int x0, int y0, int w, int h, int x1, int y1) {
    uint32_t b = base_dib();
    if (!lfb || w <= 0 || h <= 0) return;
    /* recorta origen y destino juntos */
    if (x0 < 0) { w += x0; x1 -= x0; x0 = 0; }
    if (y0 < 0) { h += y0; y1 -= y0; y0 = 0; }
    if (x1 < 0) { w += x1; x0 -= x1; x1 = 0; }
    if (y1 < 0) { h += y1; y0 -= y1; y1 = 0; }
    if (x0 + w > GW) { int e = x0 + w - GW; w -= e; }
    if (y0 + h > GH) { int e = y0 + h - GH; h -= e; }
    if (x1 + w > GW) { int e = x1 + w - GW; w -= e; }
    if (y1 + h > GH) { int e = y1 + h - GH; h -= e; }
    if (w <= 0 || h <= 0) return;
    int dy = (y1 > y0 || (y1 == y0 && x1 > x0)) ? -1 : 1;
    for (int i = 0; i < h; i++) {
        int ys = dy > 0 ? y0 + i : y0 + h - 1 - i;
        int yd = dy > 0 ? y1 + i : y1 + h - 1 - i;
        volatile uint32_t *s = (volatile uint32_t *)
            (b + (uint32_t)ys * GP + (uint32_t)x0 * 4);
        volatile uint32_t *d = (volatile uint32_t *)
            (b + (uint32_t)yd * GP + (uint32_t)x1 * 4);
        if (dy > 0 || yd != ys || x1 <= x0) {
            if (yd == ys && x1 > x0) {
                for (int c = w - 1; c >= 0; c--) d[c] = s[c];
            } else {
                for (int c = 0; c < w; c++) d[c] = s[c];
            }
        } else {
            for (int c = 0; c < w; c++) d[c] = s[c];
        }
    }
}

uint32_t g_leer_pixel(int x, int y) {
    uint32_t b = base_dib();
    if (!lfb || x < 0 || y < 0 || x >= GW || y >= GH) return 0;
    return *(volatile uint32_t *)(b + (uint32_t)y * GP + (uint32_t)x * 4);
}

static void p_pixel(int x, int y, uint32_t fg) {
    uint32_t b = base_dib();
    if (!lfb) return;
    if (x < 0 || y < 0 || x >= GW || y >= GH) return;
    *(volatile uint32_t *)(b + (uint32_t)y * GP + (uint32_t)x * 4) = fg;
}

void g_pixel(int x, int y, uint32_t color) {
    if (!lfb) return;
    p_pixel(x, y, color32(color));
}

void g_rect(int x, int y, int w, int h, uint32_t color) {
    if (!lfb || w <= 0 || h <= 0) return;
    uint32_t b = base_dib();
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GW ? GW : x + w;
    int y1 = y + h > GH ? GH : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t fg = color32(color);
    for (int yy = y0; yy < y1; yy++) {
        volatile uint32_t *p =
            (volatile uint32_t *)(b + (uint32_t)yy * GP + (uint32_t)x0 * 4);
        for (int xx = x0; xx < x1; xx++) p[xx - x0] = fg;
    }
}

void g_linea(int x1, int y1, int x2, int y2, uint32_t color) {
    if (!lfb) return;
    uint32_t b = base_dib();
    uint32_t fg = color32(color);
    int dx = x2 >= x1 ? x2 - x1 : x1 - x2;
    int dy = y2 >= y1 ? y2 - y1 : y1 - y2;
    int sx = x2 >= x1 ? 1 : -1;
    int sy = y2 >= y1 ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;
    for (;;) {
        if (x >= 0 && y >= 0 && x < GW && y < GH) {
            volatile uint32_t *p =
                (volatile uint32_t *)(b + (uint32_t)y * GP + (uint32_t)x * 4);
            *p = fg;
        }
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

void g_texto(int x, int y, const char *s, uint32_t color) {
    if (!lfb || !s) return;
    uint32_t b = base_dib();
    uint32_t fg = color32(color);
    int cx = x, cy = y;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            cy += 16;
            s++;
        } else {
            int av = 0;
            unsigned v = utf8_dec(s, 4, &av);
            const uint8_t *f;
            if (av <= 0) {
                av = 1;
                v = 0x100u | (unsigned char)s[0];
            }
            f = glifo_de(v);
            if (cx + 8 > 0 && cy + 16 > 0 && cx < GW && cy < GH) {
                int c0 = cx < 0 ? -cx : 0;
                int r0 = cy < 0 ? -cy : 0;
                int c1 = cx + 8 > GW ? GW - cx : 8;
                int r1 = cy + 16 > GH ? GH - cy : 16;
                for (int r = r0; r < r1; r++) {
                    volatile uint32_t *p = (volatile uint32_t *)
                        (b + (uint32_t)(cy + r) * GP + (uint32_t)(cx + c0) * 4);
                    uint8_t bits = f[r];
                    /* v2.0-etapa6: transparente (solo tinta). Opaco aqui
                       tapaba botones/controles con celdas negras; la
                       consola usa glifo() (sigue opaco). */
                    for (int c = c0; c < c1; c++)
                        if (bits & (0x80 >> c)) p[c - c0] = fg;
                }
            }
            cx += 8;
            s += av;
        }
    }
}

void g_limpiar(uint32_t color) {
    if (!lfb) return;
    uint32_t b = base_dib();
    uint32_t fg = color32(color);
    uint32_t n = (uint32_t)GP * GH / 4;
    volatile uint32_t *p = (volatile uint32_t *)b;
    for (uint32_t i = 0; i < n; i++) p[i] = fg;
}
