/* nslibc/stdio.h: solo lo que usa NetSurf (debug NDEBUG fuera).
 * FILE opaco; stdout/stderr existen para que enlacen. */
#ifndef F32NS_STDIO_H
#define F32NS_STDIO_H

typedef struct {
    int _ns_fd;
} FILE;

extern FILE *__ns_stdout;
extern FILE *__ns_stderr;
#define stdout (__ns_stdout)
#define stderr (__ns_stderr)

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);

#endif
