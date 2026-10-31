#include "libc.h"

static void kputc(char c) { uart_putc(c); }

static void kputs(const char *s) { uart_puts(s); }

static void kputn(uint32_t v, int base, int mayus, int signo) {
    char buf[12];
    int i = 0;
    int neg = 0;
    if (signo && (int32_t)v < 0) { neg = 1; v = (uint32_t)(-(int32_t)v); }
    if (!v) { kputc('0'); return; }
    while (v && i < 11) {
        uint32_t d = v % (uint32_t)base;
        buf[i++] = (char)(d < 10 ? '0' + d : (mayus ? 'A' : 'a') + d - 10);
        v /= (uint32_t)base;
    }
    if (neg) kputc('-');
    while (i > 0) kputc(buf[--i]);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { kputc(*p); continue; }
        p++;
        switch (*p) {
        case '%': kputc('%'); break;
        case 'c': kputc((char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            kputs(s ? s : "(null)");
            break;
        }
        case 'd': kputn(va_arg(ap, uint32_t), 10, 0, 1); break;
        case 'u': kputn(va_arg(ap, uint32_t), 10, 0, 0); break;
        case 'x': kputn(va_arg(ap, uint32_t), 16, 0, 0); break;
        case 'X': kputn(va_arg(ap, uint32_t), 16, 1, 0); break;
        default: kputc('%'); kputc(*p); break;
        }
    }
    va_end(ap);
}
