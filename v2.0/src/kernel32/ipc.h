#ifndef F32_IPC_H
#define F32_IPC_H

#include "proc.h"
#include "irq.h"

/* IPC v2.0 (entrega 5): buzones acotados por proceso (ver proc.h).
 * Enviar nunca bloquea; recibir bloquea si esta vacia. */

void ipc_init_proceso(proc_t *p);   /* limpiar al crear (memset ya lo hace) */
void ipc_limpia_proceso(proc_t *p); /* descartar pendientes al terminar */

/* Handlers de syscalls 34/35/40 (firma de tabla). */
void sc_ipc_enviar(regs_t *r, int di);
void sc_ipc_recibir(regs_t *r, int di);
void sc_ipc_hay(regs_t *r, int di);

#endif
