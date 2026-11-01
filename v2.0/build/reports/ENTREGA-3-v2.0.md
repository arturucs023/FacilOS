# v2.0 Etapa 1 — ENTREGA 3: planificador observable y ciclo de vida

Estado: COMPLETADA (cubre Fase 5 y Fase 6 en lo comprometido).

## 1. Problema y causa

- El planificador RR no era observable: sin métricas de reparto no se puede
  probar equidad (Fase 5).
- Hallado por ejecución: `entra_usuario` restauraba EBX/ESI/EDI/EBP pero NO
  EAX/ECX/EDX del usuario (`mov $0x20,%eax` residual). Una preempción entre el
  retorno de syscall y el uso del valor devuelto corrompía el dato
  (`quemador pid=32` = 0x20). Confirmado por desensamblado + 1 caso en QEMU.
- Ciclo de vida sin documentar límites ni picos de consumo (Fase 6).

## 2. Solución (diseño D4: se mantiene RR)

- `proc.h/proc.c`: `ticks_cpu` (ticks PIT en ejecución), `cedes`, 
...[truncated 1580 chars]