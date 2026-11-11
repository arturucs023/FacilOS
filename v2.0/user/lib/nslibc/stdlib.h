/* nslibc/stdlib.h: libc minima freestanding para NetSurf en F32 OS. */
#ifndef F32NS_STDLIB_H
#define F32NS_STDLIB_H

#include <stddef.h>

void *malloc(unsigned n);
void *calloc(unsigned nmemb, unsigned tam);
void *realloc(void *p, unsigned n);
void free(void *p);
void abort(void);
void *bsearch(const void *key, const void *base, unsigned n,
              unsigned tam, int (*cmp)(const void *, const void *));
void exit(int n);
long strtol(const char *s, char **fin, int base);
unsigned long strtoul(const char *s, char **fin, int base);
int abs(int v);

#endif
