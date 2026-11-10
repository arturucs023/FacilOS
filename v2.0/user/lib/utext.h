#ifndef F32_UTEXT_H
#define F32_UTEXT_H

/* UTEXT v2.2: utilidades UTF-8 propias para Ring3 (terminal, editor,
 * explorador). Codificacion UTF-8 estandar; validacion estricta aqui
 * (el renderer del nucleo es tolerante por compatibilidad con
 * volumenes antiguos: ver DISENO-UNICODE-v2.2.md). Sin libc. */

/* F32 C++: API C (sin mangling). */
#ifdef __cplusplus
extern "C" {
#endif

/* Decodifica un caracter en s (max bytes). Devuelve codepoint o
   U+FFFD si invalida; *av = bytes consumidos (minimo 1). */
unsigned ut_dec(const char *s, int max, int *av);
/* Codifica cp en d (capacidad cap). Devuelve bytes escritos (0 si no
   cabe o cp invalido). */
int ut_enc(char *d, int cap, unsigned cp);
/* 1 si s[0..n] es UTF-8 valido completo. */
int ut_valido(const char *s, int n);
/* Longitud en caracteres (no bytes). */
int ut_largo(const char *s);
/* Byte offset del caracter que empieza en o antes de pos. */
int ut_anterior(const char *s, int pos);
/* Byte offset del caracter siguiente a pos. */
int ut_siguiente(const char *s, int pos);
/* Compone NFD latino (base + U+0300-U+0308) a NFC. Devuelve compuesto
   o 0 si no hay composicion para ese par. */
unsigned ut_compone(unsigned base, unsigned comb);
/* Normaliza s a NFC (pares latinos) en d (cap). Devuelve bytes. */
int ut_nfc(const char *s, char *d, int cap);
/* Ancho en celdas (nuestra fuente: 1 por caracter). */
int ut_ancho(const char *s);

#define UT_REEMPLAZO 0xFFFDu
/* Muertos del driver (rango privado, nunca texto real). */
#define UT_MU_AGUDO 0xE000u
#define UT_MU_GRAVE 0xE001u
#define UT_MU_CIRC 0xE002u
#define UT_MU_DIER 0xE003u
#define UT_MU_TILDE 0xE004u

#ifdef __cplusplus
}
#endif

#endif
