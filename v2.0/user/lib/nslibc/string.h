/* nslibc/string.h: libc minima freestanding para NetSurf en F32 OS.
 * Sin UNIX/BSD/Linux/Windows: declara lo que usa Hubbub y lo
 * implementa nsshim.c sobre u_*. */
#ifndef F32NS_STRING_H
#define F32NS_STRING_H

#include <stddef.h>

void *memcpy(void *d, const void *s, unsigned n);
void *memmove(void *d, const void *s, unsigned n);
void *memset(void *d, int c, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);
void *memchr(const void *s, int c, unsigned n);
unsigned strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned n);
int strncasecmp(const char *a, const char *b, unsigned n);
int strcasecmp(const char *a, const char *b);
char *strncpy(char *d, const char *s, unsigned n);

#endif
