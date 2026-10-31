# FacilOS32 v1.9 — Informe de concurrencia y procesos

## 1. Hechos comprobados

- Compilación completa OK.
- Arranque QEMU OK.
- ESC / Salir del menú vuelven a la shell.
- `version` funciona tras volver a la shell.
- `hola` desde la shell funciona (proceso secuencial).
- `procesos32` lista procesos correctamente.
- Crear un proceso (`p`) desde `procesos32` puede funcionar una vez.
- Crear varios procesos o procesos gráficos provoca excepciones/PANICO.
- El kernel imprime diagnóstico con PID, estado, CR3 y motivo.

## 2. Causas confirmadas por inspección

1. **Creación de procesos no atómica frente a interrupciones**
   - `kern_crea_proceso` (exec.c) modifica tablas de páginas y `proc_tareas`
     sin deshabilitar interrupciones. En llamadas desde el kernel (shell) con
     IF=1, el PIT puede disparar `proc_fusiona` a mitad de la creación.
   - Funciones afectadas: `kern_crea_proceso`, `pag_nuevo_dir`, `pag_mapa_nuevas`,
     `proc_crea`.

2. **Dependencia de desplazamiento fijo de pila**
   - `GUARDA_GPR` asume una reserva de 0x1c bytes en `cede_a`. Si cambia el
     compilador o las flags, `esp_guard` queda inválido.
   - Funciones afectadas: `cede_a`, `entra_kernel`, `GUARDA_GPR`.

3. **Preservación de registros en syscalls bloqueantes**
   - `syscall_c` ahora usa `-O0` para evitar pérdida de variables locales
     durante cambios de contexto. Esto mitiga, pero no resuelve el diseño.

4. **Liberación asíncrona de recursos**
   - `proc_fusiona` libera recursos en la IRQ. Los slots quedan `EST_FIN`
     hasta entonces. `proc_crea` puede reutilizarlos solo cuando `reciclable=1`.
   - Riesgo de agotamiento temporal de slots/tablas bajo carga.

5. **Procesos gráficos compartidos**
   - `gfx_pid` y el dorso se comparten entre procesos. Varios procesos
     gráficos concurrentes pueden interferir.

## 3. Hipótesis ordenadas por probabilidad

1. **Alta**: carrera en bitmaps de paginación durante creación de procesos.
2. **Alta**: `esp_guard` inválido en algún camino de reanudación, provocando
   PANICO al volver a un proceso bloqueado.
3. **Media**: proceso hijo accede a puntero inválido y, al morir, el padre se
   reanuda con estado inconsistente.
4. **Media**: agotamiento temporal de slots/tablas por liberación diferida.
5. **Baja**: corrupción por procesos gráficos compartiendo el dorso.

## 4. Secuencias de fallo reconstruibles

- `procesos32` crea `reloj` → `reloj` ejecuta y provoca excepción de página
  (cr2=0x70). `proc_muere` cede a `procesos32`. El kernel entra en PANICO
  en `procesos32`.
- `procesos32` crea varios `t1_lib` → excepción de protección general en
  `procesos32`.

## 5. Funciones afectadas

- `src/kernel32/proc.c`: `proc_crea`, `proc_muere`, `proc_fusiona`,
  `proc_espera_hijo`, `cede_a`, `entra_kernel`, `GUARDA_GPR`.
- `src/kernel32/pag.c`: `pag_nuevo_dir`, `pag_libera_dir`, `pt_nueva`.
- `src/kernel32/exec.c`: `kern_crea_proceso`, `syscall_c`.
- `src/kernel32/irq.c`: `irq_c`.
- `src/kernel32/k32main.c`: arranque y tarea 0.

## 6. Pruebas propuestas

- Crear y terminar 1 proceso 100 veces (secuencial).
- Crear 2 procesos y esperar a ambos.
- Crear procesos bloqueantes (dormir/tecla) y reanudarlos.
- Crear procesos gráficos de uno en uno.
- Agotar slots y comprobar error controlado, no PANICO.
- Verificar memoria disponible tras cada ciclo.
- Añadir diagnóstico serial con PID, estado, CR3 y motivo de terminación.

## 7. Correcciones mínimas propuestas (sin aplicar aún)

1. Deshabilitar interrupciones en `kern_crea_proceso` y `proc_crea`.
2. Hacer `GUARDA_GPR`/`entra_kernel` independientes del tamaño de marco
   (guardar/restaurar contexto completo).
3. Serializar la creación de procesos.
4. Aumentar la robustez de `proc_fusiona` (no liberar si hay referencias).
5. Evitar procesos gráficos concurrentes hasta estabilizar.

## 8. Conclusión

La inestabilidad bajo carga proviene principalmente de la creación no atómica
de procesos y de la reanudación frágil del contexto. Se requiere aplicar
correcciones mínimas y repetir las pruebas antes de declarar v1.9 estable.