# FacilOS32 v1.9 — Informe de estabilización

## 1. Cambios aplicados

### P1 — Reanudación del kernel
- Archivo: `src/kernel32/proc.c`, `src/kernel32/exec.c`.
- `entra_kernel` declaró clobbers de `ebx`, `esi`, `edi`, `memory`.
- `GUARDA_GPR` ajustó `esp_guard` al desplazamiento de la pila del marco.
- `syscall_c` marcada `__attribute__((optimize("O0")))` para preservar
  variables locales a través de cambios de contexto en syscalls bloqueantes
  (8, 17, 31).
- Verificado en QEMU: ESC y opción "Salir" vuelven a la shell; `version`
  responde sin PANICO.

### P2 — Ciclo de vida de procesos
- Archivo: `src/kernel32/proc.c`.
- `proc_espera_hijo` ya no marca `reciclable` antes de la liberación;
  `proc_fusiona` libera recursos cuando el proceso está `EST_FIN`.

### P3/P4 — Paginación
- Archivo: `src/kernel32/pag.c`.
- Sustituidos contadores `pd_libres`/`pt_libres` por bitmaps `pd_uso`/`pt_uso`.
- `pag_nuevo_dir` y `pag_libera_dir` usan asignación/liberación explícita.
- Eliminado el borrado global de `ventana_pt` en cada creación.

### P5 — Shell / FS
- Archivo: `src/kernel32/fs.c`, `src/kernel32/fs.h`, `src/kernel32/shell.c`.
- `fs_init` ahora es idempotente (`fs_inicializado`), evitando perder el FS
  en RAM al abrir la terminal anidada.

### P6 — Directorio de ejecución
- Archivo: `src/kernel32/shell.c`.
- `exec_base_dir` se sincroniza con `fs_actual()` antes de ejecutar.

### P7 — u_listar
- Archivos: `src/kernel32/exec.c`, `user/sys.h`, `user/ulib.c`.
- El kernel respeta `max` (pasado en ECX) y usa zancada de 36 bytes.
- `u_listar` valida y acota `max`.

### P8 — E820
- Archivo: `src/kernel32/k32main.c`.
- Diagnóstico corregido para usar `BI->e820n`.

## 2. Pruebas ejecutadas

- Compilación completa: OK (exit 0).
- Generación ISO: OK (1517568 B, 19 programas).
- Arranque QEMU: OK (serial de boot).
- Salida del menú (ESC / Salir): OK (vuelve a shell, prompt `facil32:/>`).
- Comando `version`: OK.
- `procesos32`: listado correcto; creación de un proceso OK.
- Creación repetida de procesos: inestable (panic en algunos casos).

## 3. Problemas pendientes

- La ejecución concurrente de procesos gráficos y la creación repetida de
  procesos todavía pueden provocar excepciones/PANICO.
- El desplazamiento fijo de pila en `GUARDA_GPR` sigue dependiendo del
  compilador.
- No se ha verificado almacenamiento con disco.
- No se declaró v1.9 estable.

## 4. Conclusión

v1.9 mejoró notablemente la estabilidad (menú y shell funcionan), pero no
está lista para declararse estable. Se requiere más trabajo en gestión de
procesos concurrentes.