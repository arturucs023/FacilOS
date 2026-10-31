# FacilOS32 v1.9 — Estabilización de procesos (Fase 1)

## 1. Estructura real del proyecto

- Línea principal 32-bit: `src/kernel32/` (build32.py usa `K32`).
- Aplicaciones: `user/apps/`.
- Biblioteca de usuario: `user/lib/`.
- Herramientas: `tools/`.
- Artefactos: `build/` (bin, iso, images, programs, logs, reports, evidence).
- No se modificó `src/` histórico ni v1.8.

## 2. Línea base

- Compilación completa: OK.
- Generación ISO: OK.
- Arranque QEMU: OK.
- Menú → shell por ESC / Salir: OK.
- `version`: OK.
- `hola` desde shell: OK.
- `procesos32`: listado OK; creación de un proceso OK.
- Creación repetida: inestable (excepciones/PANICO).

## 3. Evidencia de fallos

### Caso A: crear `reloj` desde `procesos32`
- Excepción en pid=3 (reloj), num=14 (page fault), cr2=0x70.
- PANICO posterior en pid=2 (procesos32), cr2=0xc001001.
- EIP de reloj reportado (0x2003d1) queda fuera del rango de código del
  binario (máximo ~0x2002d5), indicando corrupción de contexto.

### Caso B: crear varios `t1_lib`
- Excepción en pid=2 (procesos32), num=13 (protección general), cr2=0.
- `programa abortado`.

## 4. Causas confirmadas por inspección

1. Creación de procesos no atómica frente al PIT.
2. `GUARDA_GPR` depende de un desplazamiento fijo de pila (0x1c).
3. `syscall_c` requiere `-O0` como mitigación para preservar variables
   locales a través de cambios de contexto.
4. Liberación asíncrona desde IRQ (`proc_fusiona`).
5. Procesos gráficos comparten `gfx_pid` y dorso.

## 5. Hipótesis principales

1. Carrera en bitmaps de paginación durante creación.
2. `esp_guard` inválido en algún camino de reanudación.
3. Proceso hijo muere y el padre se reanuda con estado inconsistente.
4. Agotamiento temporal de slots/tablas por liberación diferida.
5. Interferencia entre procesos gráficos concurrentes.

## 6. Funciones afectadas

- `src/kernel32/proc.c`: `proc_crea`, `proc_muere`, `proc_fusiona`,
  `proc_espera_hijo`, `cede_a`, `entra_kernel`, `GUARDA_GPR`.
- `src/kernel32/pag.c`: `pag_nuevo_dir`, `pag_libera_dir`, `pt_nueva`.
- `src/kernel32/exec.c`: `kern_crea_proceso`, `syscall_c`.
- `src/kernel32/irq.c`: `irq_c`.
- `src/kernel32/k32main.c`.

## 7. Pruebas propuestas (no ejecutadas aún)

- 100 ciclos crear/terminar secuenciales.
- Dos procesos bloqueantes y reanudación.
- Procesos gráficos individuales.
- Agotamiento controlado de slots/tablas.
- Verificación de CR3 y marcos tras cada ciclo.

## 8. Decisión

No se han aplicado modificaciones en esta fase. Se detiene a la espera de
aprobación para implementar las correcciones mínimas de la Fase 2.