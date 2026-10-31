# v2.0 Etapa 1 — ENTREGA 6: temporizadores e interrupciones + drivers

Estado: COMPLETADA (cubre Fase 9 parcial y Fase 10).

## 1. Problema y causa

- Sin API de temporizadores de núcleo para futuras esperas sin bucles (Fase 9).
- Inits de dispositivos dispersos sin contrato ni orden visible (Fase 10).
- `pit_init` embebido en `idt_init`; teclado sin init (estado implícito).

## 2. Solución

- `ktimer.h/ktimer.c` (nuevo): hasta 16 temporizadores (un disparo +
  periódicos), revisados en la IRQ del PIT (`kt_tick`). Contrato: callbacks
  triviales en IRQ (flags/contadores; prohibido bloquear/asignar). Secciones
  con IF preservado. Autoprueba en arranque (~0,35 s, `ktimer: autoprueba=0`).
- `drv.h/drv.c` (nuevo): registro `{nombre, init}` + `drv_init_todos` con
  tabla por serie. Drivers: pantalla (`pantalla_init` nuevo en gfx.c),
  teclado (`teclado_init` nuevo: cola a cero), pit (extraído a `pit_init` +
  `pit_drv_init`; `idt_init` ya no lo programa), ramfs (`ramfs_init` nuevo).
- `consola.c`: ya no llama a `gfx_init` (lo hace el driver antes; mismo
  orden efectivo). `k32main`: registra e inicializa drivers tras `sti`;
  quita `fs_init` directo (idempotente, lo cubre ramfs + shell).

## 3. Pruebas (ejecución)

- Arranque: `kheap` ok, `ktimer: autoprueba=0`, 4 drivers ok (`v20_bat5`).
- Regresión misma sesión: batería 8/8, `ipc32` 111/0, `sucio32` 32/32
  SUPERADA. Sin PANIC ni FUGA. PIT verificado (dormir/equidad en baterías).
- Fusión diferida en IRQ se mantiene por diseño documentado (segura tras
  IF-anidable de v1.9); sin cola DPC (prematura).
