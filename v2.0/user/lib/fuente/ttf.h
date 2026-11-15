/* ttf: rasterizador TrueType propio (sin hinting), solo enteros.
 *
 * Sin UNIX/BSD/Linux/Windows: aritmetica entera, sin FPU, sin libc
 * del anfitrion (u_malloc/u_free de ulib). Lee head/hhea/maxp/hmtx/
 * cmap(4/6/12)/loca/glyf; tesela quadraticas por subdivision y
 * rellena por scanline (paridad) con supermuestreo x2. Sin GPOS/GSUB,
 * sin kerning, sin bidireccional (fase 2 si hace falta).
 *
 * La fuente vive en el programa (objcopy --input binary); el nucleo
 * solo recibe la mascara ya rasterizada (sys_mezcla, sc_64).
 */
#ifndef F32_TTF_H
#define F32_TTF_H

/* Abre una fuente en memoria. Devuelve 0 ok, <0 error. */
int ttf_abre(const unsigned char *d, int n);

/* Metricas en px para un tamano (px = altura EM). */
int ttf_alto_linea(int px);     /* alto de fila recomendado */
int ttf_ascenso(int px);        /* del tope de fila a la base */
int ttf_avance(unsigned cp, int px);  /* avance horizontal (0 si falta) */
int ttf_cubre(unsigned cp);     /* 1 si hay glifo distinto de .notdef */

/* Ancho en px de s[0..n) UTF-8 (sin dibujar). */
int ttf_ancho(const char *s, int n, int px);

/* Pinta s[0..n) UTF-8 con base en yb, x inicial. color XRGB.
   Devuelve el ancho pintado, <0 error. Los ausentes salen como
   caja hueca. */
int ttf_texto(int x, int yb, const char *s, int n, int px,
              unsigned int color);

/* Glifo en bruto (para pruebas): alfa 0..255 en pix[w*h].
   Devuelve 0 ok, -1 sin glifo/error, -2 vacio (espacio: sin tinta). */
int ttf_glifo(unsigned cp, int px, unsigned char *alfa, int cap,
              int *w, int *h, int *avance, int *dx, int *dy);

/* UTF-8 -> codepoint. Devuelve bytes consumidos (0 si fin), *cp. */
int ttf_utf8(const char *s, int n, unsigned *cp);

#endif
