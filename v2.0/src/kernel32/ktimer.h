#ifndef F32_KTIMER_H
#define F32_KTIMER_H
#include <stdint.h>

/* Temporizadores de nucleo (v2.0, entrega 6).
 *
 * Lista de hasta 16 temporizadores revisada en cada tick del PIT
 * (contexto IRQ, IF=0). CONTRATO del callback: solo fijar flags o
 * contadores; prohibido bloquear, asignar memoria o llamar a syscalls.
 * Sirven para futuras esperas temporizadas sin bucles ocupados.
 *
 * kt_cada: periodico cada `ticks`; kt_una: un disparo. Devuelven id o -1.
 */
typedef void (*kt_fn)(void *arg);

int kt_cada(uint32_t ticks, kt_fn cb, void *arg);
int kt_una(uint32_t ticks, kt_fn cb, void *arg);
void kt_cancela(int id);
void kt_tick(void);          /* desde la IRQ del PIT */
int kt_autoprueba(void);     /* 0 ok; la llama k32main en arranque */

#endif
