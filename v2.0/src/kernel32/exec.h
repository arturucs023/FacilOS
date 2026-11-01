#ifndef F32_EXEC_H
#define F32_EXEC_H
#include <stdint.h>

/* Ejecutable FacilOS plano (F32X). Base 0x200000.
   v1: cabecera 16 B: magic "F32X", version=1, entry, tam. Sin BSS.
   v2: cabecera 20 B: + bss_tam a continuacion del plano (el cargador
       la pone a cero). entry = offset en el PLANO (sin cabecera). */
#define F32X_BASE ((uint32_t)0x200000)
#define F32X_MAX  512

/* Llamadas: EAX=numero, EBX/ECX/EDX=args. 1=imprimir(ptr,len), 2=ticks. */
void syscall_init(void);

/* Cargador F32X de v1.9: crea un PROCESO con espacio de direcciones
   propio (ver proc.c y pag.c). kern_crea_proceso devuelve el pid (>0) o
   un error: -100 no existe, -101 no ejecutable, -102 corrupto,
   -103 error de carga, -104 limite de procesos, -105 sin memoria.
   kern_ejecutar2 crea el proceso y ESPERA a que termine (semantica de
   v1.6-v1.8 para el menu y la shell). */
int kern_crea_proceso(const char *ruta, const char *args, int es_grafico);
int kern_ejecutar2(int dir, const char *base, const char *args, int fresco);
int kern_ejecutar(int dir, const char *base, const char *args);

/* >0 dentro de la terminal de un programa (syscall 26). */
extern int shell_anidada;

#endif
