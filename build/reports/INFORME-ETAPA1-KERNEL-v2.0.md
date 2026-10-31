# INFORME MAESTRO — v2.0 Etapa 1: evolución del kernel

## 1. Estado de v1.9 al inicio
Estable y congelada (árbol original intacto; `INFORME-ESTABILIZACION-
INTEGRAL-v1.9.md` de referencia; tag `v1.9` + release con ISO en GitHub).
8/8 batería, 258/0 `t1_lib`, balance exacto, 0 PANIC. Limitaciones: 8
procesos, gfx compartido, `t2_gfx` 11/12.

## 2. Arquitectura inicial
Monolito en `src/kernel32`: bitmap de marcos, PD/PT estáticos, first-fit,
RR sin métricas, switch de syscalls 0–33, sin IPC, sin timers, sin VFS,
drivers implícitos, sin WP. Ver `ENTREGA-1-v2.0.md`.

## 3. Arquitectura final
Ver `ARQUITECTURA-KERNEL-v2.0.md`: mismos módulos + dueño de marcos, VMA,
heap con canarios, stats de planificador, tabla de syscalls, IPC por
buzones, ktimers, registro de drivers, VFS fina, WP=1. ABI 0–33 congelado;
nuevas 34/35 (IPC), 36 (mem2), 37 (proc_stat).

## 4. Cambios realizados (entregas E1–E8)
E1: copia de trabajo + git, ISO v2.0, auditoría/arquitectura/plan.
E2: dueños de marco, VMA + chequeo FUGA, heap canarios+autoprueba,
syscall 36, `mem32`; corregida fuga de sbrk heredada (condición invertida).
E3: ticks/cedes/picos, syscall 37, `salta_usuario` restaura los 7 GPR
(causa de 1 flake hallada por ejecución), `quema32`/`equidad32`.
E4: tabla `sc_XX` + puerta + nombres + contadores; `sucio32` 32/32.
E5: IPC buzones 34/35 + `ipc32`/`ipc_hijo` 111/0.
E6: ktimers + autoprueba; drivers (pantalla/teclado/pit/ramfs).
E7: VFS fina con backend ramfs (fs.c intacto).
E8: CR0.WP=1 verificado; modelo de seguridad; base gráfica documentada.
E9: regresión final + esta documentación.

## 5. Archivos modificados
`src/kernel32/`: `pag.{c,h}`, `proc.{c,h}`, `exec.c`, `mm.c`, `libc.h`,
`cpu.h`, `irq.{c,h}`, `idt.c`, `idt32.asm`, `teclado.{c,h}`, `consola.c`,
`gfx.{c,h}`, `fs.{c,h}`, `k32main.c`; nuevos `ipc.{c,h}`, `ktimer.{c,h}`,
`drv.{c,h}`, `vfs.{c,h}`. `user/`: `sys.h`, `ulib.{c,h}`, apps `mem32`,
`quema32`, `equidad32`, `sucio32`, `ipc32`, `ipc_hijo` (+`aislamiento32`
solo test G/H). `tools/build32.py` (programas + ISO v2.0).

## 6. Funcionalidades implementadas
Contabilidad de memoria verificable, VMA + chequeo de destrucción, heap
endurecido, planificador observable, tabla de syscalls, IPC, ktimers,
drivers con contrato, VFS, WP, batería de args inválidos.

## 7. Funcionalidades pendientes
Etapa 2: servidor gráfico/ventanas/archivos, eventos por IPC, ratón PS/2,
backends VFS persistentes, telemetría `sc_count`, `proc_termina/libera`.

## 8. Resultados de pruebas
Ver `INFORME-PRUEBAS-v2.0.md`: todo SUPERADA salvo `t2_gfx` 11/12
(preexistente, explicado) y 50 simultáneos BLOQUEADA (diseño). Dos flakes
aislados contenidos por el diagnóstico (1 con causa corregida).

## 9. Comparación con v1.9
Misma estabilidad (baterías verdes) + memoria verificable, IPC, timers,
drivers, VFS, WP, syscalls documentadas/ampliables, 6 apps de prueba
nuevas. Sin regresiones (todas las pruebas v1.9 siguen verdes).

## 10. Limitaciones
Las de v1.9 (H del informe integral) + drenaje de 500 ms en tests de
ráfaga (reclamación diferida por diseño).

## 11. Riesgos conocidos
Ventanas asm↔C (mitigadas con O0/NASM + pruebas); bitmaps compartidos con
IRQ (IF-anidable); VMA e IPC dimensionados para la etapa actual (revisar
al crecer: buzones 16×64, VMA 8).

## 12. Veredicto
**ETAPA 1 COMPLETADA.**
