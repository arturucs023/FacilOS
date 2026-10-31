# FacilOS32 v1.9 — Informe de auditoría técnica

Generado a partir de inspección estática del repositorio y pruebas seguras
(compilación + arranque QEMU). No se afirma ejecución que no se haya realizado.

## 1. Estado general

- Versión estable de referencia: v1.8 (22/22 pruebas, `build/INFORME-v1.8-32.txt`).
- Estado de v1.9: **iniciada, no estable**. Compila y arranca en QEMU, pero
  se detectó un fallo crítico en la reanudación de la tarea 0 (kernel) al
  salir del menú gráfico. Se aplicó una corrección mínima en `src/kernel32/proc.c`
  y `tools/build32.py`; queda pendiente de re-verificación completa.
- Kernel: `build/kernel32.bin` = 49136 bytes (v1.8: 36392).
- ISO: `build/FacilOS-v1.9-32.iso` = 1517568 bytes (741 sectores, 19 programas).
- Programas: 19 F32X (5 v2 + 14 v3).

## 2. Arquitectura (v1.9)

- Arranque BIOS El Torito medio=2, lectura CHS en modo real (`src/kernel32/boot32.asm`).
- Kernel en 0x100000, catálogo F32C + programas en 0x20000 (RAM), copiados
  por el arranque.
- Paginación activada (`CR0.PG`) con páginas de 4 KiB (`src/kernel32/pag.c`).
- Espacio por proceso: código 0x200000, pila 0x2E0000-0x2EFFFF, guardia
  0x2D0000, heap 0x300000-0x480000, args en 0x2F0000.
- Kernel mapeado por identidad como supervisor; memoria de usuario validada
  mediante `pag_valida_usuario` y accedida por ventana 0x40000000.
- Procesos en anillo 3; tarea 0 = shell del sistema en anillo 0.
- Planificador Round Robin sobre PIT a 100 Hz (`src/kernel32/proc.c`, `src/kernel32/irq.c`).
- 34 syscalls (0-33) vía INT 0x80 (`src/kernel32/exec.c`, `user/sys.h`).
- FS FAT-like propio en RAM/disco (`src/kernel32/fs.c`), pool RAM de 256 KB sin disco.

## 3. Diferencias reales v1.8 → v1.9

| Área | v1.8 | v1.9 |
|---|---|---|
| Paginación | No | Sí (4 KiB, por proceso) |
| Memoria física | Sin gestor de marcos | E820 + bitmap de marcos (`pag.c`) |
| Procesos | `lanzar_usuario` único | `kern_crea_proceso` con CR3 propio |
| Syscalls | 28 | 34 (28-33 procesos/memoria) |
| Heap usuario | Fijo | `sbrk` con mapeo de páginas |
| Aislamiento | Parcial | Validación de punteros de usuario |
| Kernel | 36392 B | 49136 B |

## 4. Subsistemas terminados (verificado por inspección)

- Arranque y carga CHS (`boot32.asm`, `entry32.asm`).
- Gestor de marcos y E820 (`pag.c`).
- Creación de proceso con espacio propio (`exec.c:kern_crea_proceso`).
- Validación de punteros de usuario (`pag_valida_usuario`, `ptr_ok`).
- Sistema de archivos multisector (`fs.c`).
- Consola + espejo serie (`consola.c`).
- Catálogo ISO (`cdcat.c`).

## 5. Subsistemas incompletos / frágiles

- **Gestión de memoria virtual**: pool de directorios/tablas con reutilización
  insegura y sin recuperación de tablas (`pag_libera_dir` no decrementa
  `pt_libres`).
- **Ciclo de vida de procesos**: limpieza diferida y condición de fuga al
  esperar hijos (`proc_espera_hijo` marca `reciclable` antes de liberar).
- **Shell anidada**: `fs_init()` se reejecuta y borra el estado RAM.
- **Herramientas**: `ulib.c:u_listar` no respeta `max`.

## 6. Problemas detectados

### P1 — Crítico (corregido provisionalmente)
- Archivo/línea: `src/kernel32/proc.c` ~276-286 (`entra_kernel`), macro ~235 (`GUARDA_GPR`).
- Gravedad: Crítica.
- Causa: asm en línea modificaba `ebx/esi/edi/esp` sin declararlos como
  clobber; el compilador asignaba `p` a `esi` y generaba accesos
  subsiguientes sobre el registro ya sobrescrito; además `GUARDA_GPR` guardaba
  `esp` tras la reserva de 0x1c bytes de la pila, por lo que `esp_guard`
  apuntaba a variables locales y no a la dirección de retorno.
- Consecuencia: al salir del menú, `proc_muere` cedía a la tarea 0 y el
  `ret` saltaba a 0xF00000 → PANICO #PF (verificado en QEMU).
- Solución aplicada: clobbers añadidos y `esp_guard` ajustado en +0x1c.
- Estado: corregido en código; pendiente de re-verificación en QEMU.
- Prueba necesaria: salir del menú (ESC o "Salir") y ejecutar `version`.

### P2 — Alto
- Archivo/línea: `src/kernel32/proc.c:363-377` (`proc_espera_hijo`), `proc.c:177-197`
  (`proc_fusiona`).
- Gravedad: Alta.
- Causa: `proc_espera_hijo` pone `reciclable=1` antes de que `proc_fusiona`
  libere marcos/directorio/pila; `proc_fusiona` omite los `reciclable`.
- Consecuencia: fuga de memoria y de slots de proceso en programas que esperan
  hijos.
- Solución propuesta: separar "rc consumido" de "recursos liberados" (nuevo
  flag `rc_leido`) y que `proc_fusiona` libere siempre que `estado==EST_FIN`.
- Prueba: crear/esperar varios hijos y comprobar `u_info_memoria` / `u_listar`.

### P3 — Alto
- Archivo/línea: `src/kernel32/pag.c:378-390` (`pag_libera_dir`), `pag.c:152-157`
  (`pt_nueva`).
- Gravedad: Alta.
- Causa: `pt_libres` nunca se decrementa al liberar; las PT quedan
  contabilizadas como usadas.
- Consecuencia: tras ~48 procesos creados/liberados, `pag_nuevo_dir` falla
  (-105/-104).
- Solución propuesta: devolver índices de PT al pool (pila libre) o recalcular.
- Prueba: bucle de creación/terminación de procesos.

### P4 — Medio
- Archivo/línea: `src/kernel32/pag.c:394-415` (`pag_nuevo_dir`), `pag.c:378-390`.
- Gravedad: Media.
- Causa: `pd_libres` se actualiza con `if (pd_libres < di) pd_libres = di;`,
  lo que puede reabrir ranuras aún en uso si se liberan fuera de orden.
- Consecuencia: posible reutilización de directorio de otro proceso.
- Solución: bitmap de PD libres en lugar de contador.
- Prueba: creación/liberación intercalada de procesos.

### P5 — Medio
- Archivo/línea: `src/kernel32/shell.c:333` (`fs_init()` dentro de `shell()`).
- Gravedad: Media.
- Causa: la shell anidada (syscall 26) reinicializa el FS, llamando
  `fs_limpia()`; sin disco se pierden ficheros RAM.
- Consecuencia: "Terminal" del menú borra ficheros creados en RAM.
- Solución propuesta: no reinicializar si `fs_disco_ok` ya configurado, o
  separar `fs_init` de `shell`.
- Prueba: crear fichero, abrir Terminal, verificar fichero.

### P6 — Medio
- Archivo/línea: `src/kernel32/exec.c:16` (`exec_base_dir`), `shell.c` (no lo
  actualiza).
- Gravedad: Media.
- Causa: `exec_base_dir` permanece en 0 aunque la shell cambie de carpeta.
- Consecuencia: ejecutar programas desde subcarpetas falla.
- Solución: sincronizar `exec_base_dir` con `fs_actual()` en la shell.
- Prueba: `entrar_carpeta` + lanzar programa.

### P7 — Bajo
- Archivo/línea: `user/ulib.c:536-538` (`u_listar`).
- Gravedad: Baja.
- Causa: ignora `max` y el kernel escribe hasta 9 entradas (216 B).
- Consecuencia: desbordamiento si el búfer del usuario es menor.
- Solución: pasar `max` al kernel o validar en `ulib`.
- Prueba: `u_listar(buf, 1)`.

### P8 — Bajo
- Archivo/línea: `src/kernel32/k32main.c:20-34` (`info_memoria`).
- Gravedad: Baja.
- Causa: el diagnóstico lee 0x7020/0x7000 con offsets incorrectos; imprime
  `n=0` aunque E820 tenga 32 entradas.
- Consecuencia: log confuso, sin impacto funcional.
- Solución: corregir offsets a 0x7001 (e820n).
- Prueba: inspección del log serie.

### P9 — Bajo (código muerto)
- Archivo/línea: `src/kernel32/proc.c:161` (`proc_libera`), `src/kernel32/seg.c:89`
  (`lanzar_usuario`).
- Gravedad: Baja.
- Causa: funciones no usadas en v1.9.
- Consecuencia: mantenimiento; posible confusión.
- Solución: documentar o eliminar tras verificar.
- Prueba: compilación.

## 7. Riesgos

- La corrección P1 usa un desplazamiento fijo de pila (0x1c) dependiente del
  compilador; un cambio de flags/versión de GCC puede romperla.
- La reutilización de ranuras de proceso/directorio puede corromper estado
  bajo carga.
- El FS en RAM no es persistente; la shell anidada lo borra.
- No hay protección completa frente a accesos inválidos desde Ring 3 más allá
  de la validación explícita en syscalls.

## 8. Resultados de pruebas

| Prueba | Resultado |
|---|---|
| Compilación completa (`build32.py`) | Verificado por ejecución (exit 0) |
| Generación kernel | Verificado (49136 B) |
| Generación ISO | Verificado (1517568 B) |
| Arranque QEMU (serial) | Verificado (boot + 19 programas listados) |
| Menú gráfico | Parcialmente verificado (boot, sin captura estable en esta ejecución) |
| Salida del menú a shell | No verificado tras la corrección |
| Shell / comandos | No verificado en v1.9 |
| Aplicaciones | No verificado en v1.9 |
| Syscalls | Verificado por inspección estática |
| Procesos/cambio de contexto | Parcialmente verificado (panic previo corregido, no re-verificado) |
| Aislamiento | Verificado por inspección estática |
| Memoria | Verificado por inspección estática |
| Almacenamiento | No verificado |
| Teclado | Verificado por inspección estática |
| Gráficos | Parcialmente verificado |
| Recuperación de errores | Verificado (panic path en QEMU) |

## 9. Limitaciones

- Solo se ejecutó arranque en QEMU; no se validaron apps ni syscalls en
  ejecución en v1.9.
- No se capturaron pantallas de evidencia nuevas.
- El harness de QEMU mostró comportamiento inconsistente al enviar ESC.

## 10. Recomendaciones

1. Re-verificar en QEMU la salida del menú tras P1 antes de seguir.
2. Rediseñar la gestión de recursos de procesos (P2, P3, P4) con bitmaps.
3. Evitar reinicialización del FS en shell anidada (P5).
4. Sincronizar `exec_base_dir` (P6).
5. Añadir pruebas automatizadas de procesos/hijos.

## 11. Hoja de ruta

1. **Estabilidad del arranque**: objetivo verificar ESC/Salir y shell.
   Archivos: `src/kernel32/proc.c`, `src/kernel32/exec.c`. Riesgo: alto. Criterio: prompt
   `facil:` en serie sin PANICO.

2. **Memoria**: bitmap de marcos y PT/PD recuperables. Archivos: `pag.c`,
   `pag.h`. Criterio: creación/liberación de 100 procesos sin fallo.

3. **Protección Ring 3**: auditoría de todas las syscalls con punteros.
   Archivos: `exec.c`. Criterio: punteros inválidos devuelven -1 sin #PF.

4. **Scheduler**: eliminar dependencia de offset fijo de pila. Archivos:
   `proc.c`, `idt32.asm`. Criterio: reanudación estable de tarea 0 y
   procesos bloqueados.

5. **Syscalls**: documentar ABI 0-33 y validar argumentos. Archivos:
   `exec.c`, `user/sys.h`, `user/ulib.c`.

6. **Sistema de archivos**: persistencia y no reinicialización en shell
   anidada. Archivos: `fs.c`, `shell.c`.

7. **Gestión de procesos**: corregir fuga de recursos. Archivos: `proc.c`.

8. **Gráficos**: capturas de evidencia. Archivos: `gfx.c`, `menu.c`.

9. **Aplicaciones**: ejecutar batería v1.9. Archivos: `user/*`.

10. **Herramientas**: mejorar qrun (ESC estable, esperas).

## 12. Próxima tarea técnica recomendada

Re-verificar en QEMU la salida del menú (ESC/"Salir") y la shell tras la
corrección de `entra_kernel`, y capturar el log serie como evidencia. Si
falla, rediseñar `GUARDA_GPR` sin depender del desplazamiento fijo de pila.
