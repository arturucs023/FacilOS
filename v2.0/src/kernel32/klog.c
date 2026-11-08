#include "klog.h"
#include "cpu.h"
#include "libc.h"

/* F8: klog propio. Estatico, sin heap. La UART se sondea con tope
 * para no colgar si el puerto falla; la consola solo se usa si el
 * nucleo la marco disponible (nunca requerida). */

#define KLOG_CAP 8192

static char kbuf[KLOG_CAP];
static uint32_t khead = 0, ktail = 0;
static int k_umbral = KLOG_INFO;
static int k_en_panico = 0;
static int k_consola_ok = 0;

void klog_consola(int ok) { k_consola_ok = ok ? 1 : 0; }

void klog_init(void) {
    khead = ktail = 0;
    k_umbral = KLOG_INFO;
    k_en_panico = 0;
    k_consola_ok = 0;
}

void klog_umbral(int nivel) {
    if (nivel >= KLOG_DEBUG && nivel <= KLOG_FATAL) k_umbral = nivel;
}

int klog_nivel(void) { return k_umbral; }

static void k_serie_n(const char *s, uint32_t n) {
    /* Polling acotado: ~1M iteraciones max por caracter. */
    for (uint32_t i = 0; i < n && s[i]; i++) {
        uint32_t t = 0;
        while (!(inb(0x3FD) & 0x20)) {
            if (++t > 1000000u) return;
        }
        outb(0x3F8, (uint8_t)s[i]);
    }
}

static void k_serie_hex(uint32_t v) {
    char h[9];
    h[8] = 0;
    for (int i = 7; i >= 0; i--) {
        uint32_t d = v & 15u;
        h[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v >>= 4;
    }
    k_serie_n(h, 8);
}

static void k_guarda(char c) {
    uint32_t nx = (khead + 1) % KLOG_CAP;
    if (nx == ktail) {
        ktail = (ktail + 1) % KLOG_CAP;   /* overwrite: nunca bloquea */
    }
    kbuf[khead] = c;
    khead = nx;
}

static const char *k_nom(int n) {
    switch (n) {
    case KLOG_DEBUG: return "DEBUG";
    case KLOG_INFO: return "INFO";
    case KLOG_WARN: return "WARN";
    case KLOG_ERROR: return "ERROR";
    default: return "FATAL";
    }
}

void klog(int nivel, const char *subs, const char *msg) {
    uint32_t fl;
    const char *s;
    if (nivel < k_umbral) return;
    if (k_en_panico && nivel < KLOG_FATAL) return;
    fl = cpu_eflags();
    cli();
    k_serie_n("[", 1);
    s = k_nom(nivel);
    k_serie_n(s, 5);
    k_serie_n(" ", 1);
    k_serie_n(subs ? subs : "nucleo", 32);
    k_serie_n("] ", 2);
    k_serie_n(msg ? msg : "", 512);
    k_serie_n("\r\n", 2);
    if (!k_en_panico) {
        const char *p = "[";
        while (*p) { k_guarda(*p); p++; }
        p = k_nom(nivel);
        while (*p) { k_guarda(*p); p++; }
        k_guarda(' ');
        p = subs ? subs : "nucleo";
        for (int i = 0; i < 32 && p[i]; i++) k_guarda(p[i]);
        k_guarda(']');
        k_guarda(' ');
        p = msg ? msg : "";
        for (int i = 0; i < 512 && p[i]; i++) k_guarda(p[i]);
        k_guarda('\r');
        k_guarda('\n');
    }
    if (nivel >= KLOG_FATAL) {
        k_en_panico = 1;
        cpu_if_restaura(fl);
        for (;;) hlt();
    }
    cpu_if_restaura(fl);
}

void klog_hex(int nivel, const char *subs, const char *tit, uint32_t v) {
    uint32_t fl;
    if (nivel < k_umbral) return;
    if (k_en_panico && nivel < KLOG_FATAL) return;
    fl = cpu_eflags();
    cli();
    k_serie_n("[", 1);
    k_serie_n(k_nom(nivel), 5);
    k_serie_n(" ", 1);
    k_serie_n(subs ? subs : "nucleo", 32);
    k_serie_n("] ", 2);
    k_serie_n(tit ? tit : "", 64);
    k_serie_n("=0x", 3);
    k_serie_hex(v);
    k_serie_n("\r\n", 2);
    cpu_if_restaura(fl);
    if (nivel >= KLOG_FATAL) {
        k_en_panico = 1;
        for (;;) hlt();
    }
}

void klog_volcar(void) {
    uint32_t fl = cpu_eflags();
    cli();
    k_serie_n("-- klog inicio --\r\n", 19);
    while (ktail != khead) {
        char c = kbuf[ktail];
        uint32_t t = 0;
        while (!(inb(0x3FD) & 0x20)) {
            if (++t > 1000000u) break;
        }
        if (t <= 1000000u) outb(0x3F8, (uint8_t)c);
        ktail = (ktail + 1) % KLOG_CAP;
    }
    k_serie_n("-- klog fin --\r\n", 16);
    cpu_if_restaura(fl);
}
