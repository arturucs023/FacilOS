/* nslibc/stdio.h: solo lo que usa NetSurf (debug NDEBUG fuera).
 * FILE opaco; stdout/stderr existen para que enlacen. */
#ifndef F32NS_STDIO_H
#define F32NS_STDIO_H

typedef struct {
    int _ns_fd;
} FILE;

#define EOF (-1)

extern FILE *__ns_stdout;
extern FILE *__ns_stderr;
extern FILE *__ns_stdin;
#define stdout (__ns_stdout)
#define stderr (__ns_stderr)
#define stdin (__ns_stdin)

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int snprintf(char *d, unsigned n, const char *fmt, ...);
int sprintf(char *d, const char *fmt, ...);
unsigned fread(void *b, unsigned tam, unsigned n, FILE *f);
unsigned fwrite(const void *p, unsigned tam, unsigned n, FILE *f);
int ferror(FILE *f);
void clearerr(FILE *f);
int getc(FILE *f);
int vsnprintf(char *d, unsigned n, const char *fmt,
              __builtin_va_list ap);

#endif
