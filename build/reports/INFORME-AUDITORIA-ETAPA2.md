# INFORME-AUDITORIA-ETAPA2 (Fase 0: mapa inicial)

Rama: `v2.0-etapa2` (desde etapa1 `580d49a`). `main`/v1.9 intactos.
Estado: SOLO INSPECCIÓN — ningún cambio de código en esta fase.

## 1. Arquitectura observada (confirmada por inspección + ejecución)

Arranque BIOS/El Torito (`boot32.asm`) → `entry32` → `kmain`: uart, mm+kheap
test, pag (E820, bitmap+dueños, PD/PT, ventana, WP), proc, idt (PIC+PIT+IPC? no:
teclado+timer), seg (GDT/TSS), sti, ktimer test, drv (pantalla/teclado/pit/
ramfs), cons, cdcat, menu como proceso, shell (tarea 0 Ring0).
Usuario Ring3 con CR3/pila-k propias; syscalls 0–37 por INT 0x80 (tabla
`sc_XX` + puerta); IRQ PIT (ticks, ktimers, fusión diferida, preempción) y
teclado; framebuffer LFB + dorso con dueño único.

## 2. Subsistemas reales y dependencias

mem-física (pag.c bitmap+dueños) ← mem-virtual (PD/PT/VMA/ventana) ← kheap
(mm.c) ← proc/planif (tabla, RR, GUARDA/retoma/salta, fusión) ← syscall
(exec.c+ipc.c) ← apps (ulib). irq/ktimer/drv/vfs/fs/shell/consola/gfx sin
dependencias circulares (verificado en includes).

## 3. Pruebas existentes y cobertura

`aislamiento32` (A–H: aislamiento, ciclos, límites), `mem32` (contabilidad),
`equidad32` (RR), `ipc32` (111 buzones), `sucio32` (32 args inválidos),
`t1_lib` (258 lib/FS/sbrk), `planificador32`, `t2_gfx` (11/12), `reloj`,
`quema32`, FS de shell. FALTA: estrés prolongado, presión hasta
agotar marcos/PD/PT/pilas/buzones, sbrk límite exacto, IPC multiemisor,
muerte durante syscall bloqueante, sesiones largas, pruebas de limites
de VMA/heap-k, fuzz de syscalls más allá de sucio32.

## 4. Riesgos detectados (a verificar, NO confirmados)

R1. `pag_toma_marco` sin sección crítica (RMW bitmap vs IRQ-fusión).
R2. Publicación de proceso (`proc_crea` LISTO antes de esp_k/modo) — ventana
   benigna por análisis, sin prueba de estrés dirigida.
R3. `syscall_regs_ptr` global residual (fallback de `regs_actual`).
R4. VMA: tabla 8 sin prueba de lleno; merge solo-heap.
R5. kheap sin usuarios reales (autoprueba sí, carga no).
R6. Buzón sin prueba multiemisor ni receptor-muerto-bloqueado.
R7. `proc_muere` ocioso con `proc_actual` FIN (fusión lo salta).
R8. Teclado: espera con cola no vacía (ventana sti/hlt/bloquea).
R9. G/T: drenaje por tiempo (no verificado) en ráfagas.
R10. t1_lib: 1 aborto aislado sin mecanismo (vigilancia abierta).

## 6. Contratos C/Assembly verificados (Fase 6, por desensamblado + GDB)

| Ruta | Contrato | Estado |
|---|---|---|
| `_syscall_comun` | pusha+segs, `push esp`, IF=0 en handler | verificado |
| `syscall_c` + handlers O0 | locales en pila sobreviven a cesión | verificado |
| `GUARDA_GPR` | ESP=ebp+4, EBX/ESI/EDI, EBP llamante (`*ebp`) | verificado |
| `retoma_kernel` | restaura EBX/SI/DI/BP/ESP + `ret` | verificado |
| `entra_usuario`/`salta_usuario` | marco iret en cima + TSS.esp0 + 7 GPR | verificado (GDB dis) |
| `_irq_comun`/`_isr_comun` | pusha+segs, EOI antes de fusión/preempt | verificado |
| TSS.esp0 | == cima del proceso en curso en toda entrada Ring3→0 | verificado |
| IF | puertas limpian; handlers preservan; secciones con IF-anidable | verificado |

## 7. Veredicto de subsistemas tras el estrés
Memoria física/virtual/heap/sbrk/planificador/IPC/syscalls/timers/drivers/
## 5. Plan de baterías (ejecutado en esta etapa)

- `estres_mem`: ciclos sbrk de tamaños variados + info2 exacto + presión
  hasta -105/-1 + recuperación; doble-free forzado interno (contable).
- `estres_proc`: ráfagas crear/morir mixtas (normal/excepción/dormir/tecla),
  MAXPROC sostenido, info2 + listar antes/después.
- `estres_ipc`: multiemisor, receptor que muere bloqueado, emisor que muere,
  lleno con timeout, 1000 idas.
- `estres_sched`: quemadores desiguales + cedentes + bloqueantes, ticks.
- Sesión larga H: todo lo anterior ×N iteraciones con balance final.
Cada batería: timeout propio, sin esperas ciegas, verifica contenido y
estado (Fase 14), integrada en build32 + documentada.
