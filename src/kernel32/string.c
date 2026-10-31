#include "libc.h"

uint32_t libc_strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

int libc_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int libc_strncmp(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i]) return (int)(uint8_t)a[i] - (int)(uint8_t)b[i];
    }
    return 0;
}

char *libc_strcpy(char *d, const char *s) {
    char *r = d;
    while ((*d++ = *s++)) { }
    return r;
}

void *libc_memcpy(void *d, const void *s, uint32_t n) {
    uint8_t *dd = d;
    const uint8_t *ss = s;
    for (uint32_t i = 0; i < n; i++) dd[i] = ss[i];
    return d;
}

void *libc_memmove(void *d, const void *s, uint32_t n) {
    uint8_t *dd = d;
    const uint8_t *ss = s;
    if (dd < ss) {
        for (uint32_t i = 0; i < n; i++) dd[i] = ss[i];
    } else if (dd > ss) {
        for (uint32_t i = n; i > 0; i--) dd[i - 1] = ss[i - 1];
    }
    return d;
}

void *libc_memset(void *d, int c, uint32_t n) {
    uint8_t *dd = d;
    for (uint32_t i = 0; i < n; i++) dd[i] = (uint8_t)c;
    return d;
}

int libc_memcmp(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = a, *y = b;
    for (uint32_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}
