# Multitarea v2.0 (planificador, procesos, IPC)

## Planificador (RR, D4)
Round Robin sobre PIT 100 Hz + cesión voluntaria; sin prioridades
(estudiadas y rechazadas: inanición/complejidad sin necesidad).
Observabilidad: `ticks_cpu`, `cedes` por proceso (syscall 37).
Contexto completo: ESP+EBX+ESI+EDI+EBP en kernel (`retoma_kernel`),
los 7 GPR + eip/esp/eflags en usuario (`salta_usuario`).
Equidad medida: 3 quemadores 34/34/34 y 67/67/68 (ratio < 1,5).

## Ciclo de vida
NUEVO→LISTO→EJECUTO→{LISTO,BLOQ,FIN}+reciclable; PID monotónicos;
fusión diferida en IRQ (nunca sobre el proceso en curso); VMA y buzón
limpiados al terminar; picos de marcos por proceso. Límites: MAXPROC=8
(7 hijos + padre), pila kernel 32 KB, heap 1,5 MB.

## IPC (buzones, D5)
16×64 B FIFO por proceso; enviar no bloquea (lleno→-2); recibir bloquea;
emisor entregado; validación de punteros/tamaños; limpieza al terminar.
Syscalls 34/35 (ABI nueva, documentada).

## Pruebas
- Batería 8/8 ×6; planificador opt1/opt3; `equidad32` 3/3; `ipc32` 111/0.
- Creación/destrucción repetida (40+20, ráfagas de 7) sin PANIC ni fugas.
