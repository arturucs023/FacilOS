#ifndef DEMO16_H
#define DEMO16_H

#include "ulib.h"

/* demo16: aplicacion C compleja (varios modulos) que ejercita toda la
   plataforma v1.6: ficheros multisector, memoria, graficos con doble
   bufer, temporizacion. Todo determinista (LCG) para pruebas. */

unsigned int d_azar(void);
void d_azar_ini(unsigned int semilla);
void d_titulo(const char *s);
void d_pausa(void);
void d_info(const char *s);

void escena_graficos(void);
void escena_ficheros(void);
void escena_memoria(void);
void escena_banco(void);

#endif
