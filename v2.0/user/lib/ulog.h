#ifndef F32_ULOG_H
#define F32_ULOG_H

/* ULOG v2.2: registros en castellano para Ring3.
 * Formato: [AAAA/MM/DD HH:MM:SS] [NIVEL] [COMPONENTE] COD-0000: mensaje
 * Sin asignacion dinamica ni limites ocultos (todo con topes). */

#define ULOG_INFO 0
#define ULOG_AVISO 1
#define ULOG_ERROR 2
#define ULOG_DEPUR 3
#define ULOG_CRITICO 4

/* F32 C++: API C (sin mangling). */
#ifdef __cplusplus
extern "C" {
#endif

/* Formatea un registro en d (cap). Devuelve bytes (sin NUL). */
int ulog_form(char *d, int cap, int nivel, const char *comp, int cod,
              const char *msg);

#ifdef __cplusplus
}
#endif

#endif
