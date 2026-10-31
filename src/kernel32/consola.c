#include "libc.h"
#include "cpu.h"
#include "gfx.h"
#include "bootinfo.h"

static int usa_gfx = 0;

void cons_init(void) {
    gfx_init();
    usa_gfx = gfx_activo();
    if (!usa_gfx) vga_cls();
}

int cons_gfx(void) { return usa_gfx; }

void cons_putc(char c) {
    if (usa_gfx) gfx_putc(c, 0x07);
    else {
        char s[2] = { c, 0 };
        vga_puts(s);
    }
    /* v1.8: espejo en serie. La UART de QEMU siempre esta lista; en una
       maquina real el bit LSR de "puede enviar" tarda ~87 us por byte a
       115200 baudios, asumible para una consola de texto. Asi el log de
       serie sirve para verificar tambien los programas de texto.
       Los caracteres de control sin-interest (retroceso) no se reflejan:
       los emite a mano quien los necesita. */
    if (c == '\n' || c == '\r' || c >= 32) uart_putc(c);
}

void cons_puts(const char *s) {
    while (*s) cons_putc(*s++);
}

void cons_clear(void) {
    if (usa_gfx) gfx_clear();
    else vga_cls();
}

void uart_init(void) {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x03);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}

void uart_putc(char c) {
    while (!(inb(0x3FD) & 0x20)) { }
    outb(0x3F8, (uint8_t)c);
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static int vga_col = 0, vga_fil = 0;

static void vga_cursor(void) {
    int pos = vga_fil * 80 + vga_col;
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

static void vga_scroll(void) {
    for (int f = 0; f < 24; f++)
        for (int c = 0; c < 80; c++)
            VGA[f * 80 + c] = VGA[(f + 1) * 80 + c];
    for (int c = 0; c < 80; c++)
        VGA[24 * 80 + c] = (uint16_t)(0x0700 | ' ');
}

void vga_pon(int col, int fil, char c, uint8_t attr) {
    if (col < 0 || col >= 80 || fil < 0 || fil >= 25) return;
    VGA[fil * 80 + col] = (uint16_t)((attr << 8) | (uint8_t)c);
}

static void vga_avanza(void) {
    vga_col++;
    if (vga_col >= 80) { vga_col = 0; vga_fil++; }
    while (vga_fil >= 25) { vga_scroll(); vga_fil--; }
    vga_cursor();
}

void vga_puts(const char *s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') { vga_col = 0; vga_fil++; while (vga_fil >= 25) { vga_scroll(); vga_fil--; } vga_cursor(); continue; }
        if (s[i] == '\r') { vga_col = 0; vga_cursor(); continue; }
        if (s[i] == '\b') {
            if (vga_col > 0) vga_col--;
            vga_pon(vga_col, vga_fil, ' ', 0x07);
            vga_cursor();
            continue;
        }
        vga_pon(vga_col, vga_fil, s[i], 0x07);
        vga_avanza();
    }
}

void vga_nl(void) {
    vga_col = 0; vga_fil++;
    while (vga_fil >= 25) { vga_scroll(); vga_fil--; }
    vga_cursor();
}

void vga_cls(void) {
    for (int i = 0; i < 80 * 25; i++) VGA[i] = (uint16_t)(0x0700 | ' ');
    vga_col = 0; vga_fil = 0;
    vga_cursor();
}
