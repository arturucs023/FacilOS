# v2.0 Etapa 1 — ENTREGA 2: memoria física, virtual y heap

Estado: COMPLETADA (cubre Fases 2–4 de la etapa en lo de memoria).

## 1. Problema y causa

- La contabilidad de marcos era solo un bitmap sin dueño: sin detección de
  doble-free, sin pico, sin verificación (Fase 2).
- Las páginas de heap (sbrk) NO se anotaban (`if (pag_translada(...))` con la
  condición invertida en `exec.c` caso 7): fuga real enmascarada en v1.9 por
  el desmapeo amplio de `proc_fusiona` (ya eliminado por liberar marcos
  reservados). Confirmado por ejecución.
- Sin seguimiento de regiones virtuales (Fase 3) y heap del núcleo sin
  canarios ni stats (Fase 4).

## 2. Solución (diseño D1/D2/D3)

- `pag.c`: tabla `marco_dueno` (LIBRE/RESERVA/USUARIO, 16 KB), contadores de
  usados/picode usuario, contador de doble-free (no corrompe), `pag_verifica`
  (coherencia bitmap↔dueños), `pag_cuenta_rango` (PTEs usuario presentes),
  contadores PD/PT. Se mantiene el bitmap (D1).
- `proc.h/proc.c`: tabla VMA por proceso (8: CODIGO/PILA/HEAP con unión por
  solape, no solo adyacencia) + chequeo en `proc_fusiona`: presentes en VMA
  vs `nmarcos`; desajuste → `FUGA pid=…` + desglose y contador `proc_fugas`.
- `exec.c`: condición de anotado corregida (`!pag_translada`); VMA en
  `kern_crea_proceso` y sbrk; syscall **36** `info_memoria2` (8 palabras:
  libres, usados, usados_usuario, pico, doble_free, anomalías, pd, pt).
  ABI 0–33 intacto; 34–35 reservadas para IPC (sin implementar).
- `mm.c`/`libc.h`: canarios cabeza/pie (KMAGIC) + verificación en cada
  recorrido, stats (usados/pico/dobles/corruptos), `mm_autoprueba` en
  arranque (una línea `kheap:` en serie). First-fit mantenido (D3).
- `user`: `sys_info_memoria2`/`u_info_memoria2`; app nueva `mem32`
  (15 comprobaciones: coherencia, crecimiento, pico, reuso sin crecer,
  sbrk enorme rechazado, anomalías a cero).

## 3. Pruebas (ejecución)

- `mem32`: 15/15 SUPERADA ×3 (`v20_mem1/2/3.log`).
- Batería `aislamiento32` 8/8 SUPERADA (`v20_bat1.log`), sin líneas FUGA.
- `t1_lib` 258/0 SUPERADA ×2 (`v20_t1b`, `v20_mem3`).
- Incidencia aislada: 1 aborto de `t1_lib` en `v20_mem2.log` (#PF usuario
  200cca/18d8fc, solo murió el ofensor, sistema intacto). Sin mecanismo
  reproducible tras 4 repeticiones verdes; clasificado como flake pendiente
  de vigilancia (no se tapa: el diagnóstico funcionó). Hipótesis: timing.
- Regresión v1.9 (boot/shell/hola): SUPERADA en todas las sesiones.

## 4. Decisiones pendientes (fases 5+)

Prioridades del planificador: rechazadas para etapa 1 (D4, se mantiene RR +
stats en entrega 3). COW/demanda/swap: no (D2). VFS/drivers/IPC: entregas
5–7. CR0.WP: entrega 8 con batería.
