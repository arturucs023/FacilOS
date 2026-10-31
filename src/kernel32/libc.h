#ifndef F32_LIBC_H
#define F32_LIBC_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* serie (UART COM1) */
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);

/* VGA texto 80x25 */
void vga_pon(int col, int fil, char c, uint8_t attr);
void vga_puts(const char *s);
void vga_nl(void);
void vga_cls(void);

/* Consola (texto o grafica segun bootinfo) */
void cons_init(void);
int cons_gfx(void);
void cons_putc(char c);
void cons_puts(const char *s);
void cons_clear(void);

/* cadena/memoria */
uint32_t libc_strlen(const char *s);
int libc_strcmp(const char *a, const char *b);
int libc_strncmp(const char *a, const char *b, uint32_t n);
char *libc_strcpy(char *d, const char *s);
void *libc_memcpy(void *d, const void *s, uint32_t n);
void *libc_memmove(void *d, const void *s, uint32_t n);
void *libc_memset(void *d, int c, uint32_t n);
int libc_memcmp(const void *a, const void *b, uint32_t n);

/* monticulo (heap fijo, first-fit con union) */
void mm_init(void);
void *kmalloc(uint32_t n);
void kfree(void *p);

/* formateo a serie */
void kprintf(const char *fmt, ...);

#endif
