#ifndef F32_SEG_H
#define F32_SEG_H
#include <stdint.h>

void seg_init(void);
void tss_pon_esp0(uint32_t esp0);
/* Lanza programa en anillo 3. No vuelve hasta salir/aborto; devuelve codigo. */
int lanzar_usuario(uint32_t entry, uint32_t uesp);
/* Igual pero fresco=0 no reinicia fds ni monton (hijo anidado: el padre
   guarda y restaura su estado). */
int lanzar_usuario2(uint32_t entry, uint32_t uesp, int fresco);
extern int en_usuario;

#endif
