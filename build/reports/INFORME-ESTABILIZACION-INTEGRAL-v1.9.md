# FacilOS32 v1.9 — Informe de estabilización integral

Fecha: 2026-10-31. Ámbito: validación reproducible de multitarea (v1.9, sin v2.0).

## A. Estado inicial

Funcionaba (heredado + verificado al inicio de esta fase):
- Compilación (`tools/build32.py`, exit 0), kernel 53064 B, ISO 1517568 B, 19 programas.
- Arranque BIOS, E820 ok (32 entradas), paginación, catálogo F32C.
- Menú → shell (ESC), `version`, `ejecutar hola` (rc 42), `procesos32` (listado).
- Corrección TSS.esp0 en `entra_kernel` presente (`src/kernel32/proc.c`).

Fallaba (reproducido en esta fase, determinista):
1. Tras ejecutar CUALQUIER proceso desde la shell, la shell dejaba de reconocer
   comandos (`eco`/`version` → "No entiendo...", o silencio). Reproducción 3/3
   (`build/logs/v19_r1.log`, `v19_r2.log`, `v19_t7`-equivalente).
2. Batería `aislamiento32`: 8 pruebas, 5 fallos (B, E, F, G, H con `rc=-1` sin
   excepción) (`build/logs/v19_aisla.log`).
3. Balance de marcos corrupto: +351 marcos "libres" fantasma tras la batería
   (5664 en arranque → 6015 después).

Incidente de proceso: `tools/build32.py` generaba la ISO con el nombre
`FacilOS-v1.8-32.iso` y sobrescribió ese artefacto durante esta fase. Se cambió
la salida a `FacilOS-v1.9-32.iso` (único cambio en herramientas). El fichero
`build/iso/FacilOS-v1.8-32.iso` quedó con contenido v1.9; no se tocó nada más
de v1.8 (informes y evidencias intactos).

## B. Causa raíz anterior (TSS.esp0)

Fallo: `entra_kernel` no actualizaba `TSS.esp0` al reanudar un proceso
bloqueado en modo kernel; las IRQ usaban la pila de kernel de otro proceso y
corrompían `esp_guard` → PANIC. Corrección (preexistente, verificada):
`entra_kernel` llama a `tss_pon_esp0(p->esp_k)` antes de restaurar el contexto.
Se mantiene en el código nuevo (`src/kernel32/proc.c`, `entra_kernel`) y se
auditó que TODAS las rutas la cumplen: `entra_usuario` (fija `esp0`), 
`entra_kernel` (fija `esp0`), `proc_muere` (pila ociosa + restaura).
`lanzar_usuario`/`entrar_usuario` antiguos son código muerto (sin llamadas).

## C. Auditoría por fases

- F0 (inventario): rutas reales identificadas (`src/kernel32/`, `user/lib`,
  `user/apps`, `tools/qrun.py`). La corrección TSS estaba presente.
- F1 (TSS y pilas): desensamblado de `cede_a`/`entra_usuario`/`entra_kernel`
  correcto para este compilador (solo registros caller-saved en `cede_a`,
  `esp_guard=ebp+4`). Hallado P0 (EBP no preservado, ver D).
- F2 (planificador y atomicidad): hallados P3 (cli/sti no anidables) y P4
  (cola de teclado sin módulo). `kern_crea_proceso` desde shell corre con
  IF=1 frente a `proc_fusiona` en IRQ: los bitmaps afectados están ahora bajo
  secciones con IF preservado.
- F3 (ciclo de vida): estados NUEVO/LISTO/EJECUTO/BLOQ/FIN + `reciclable`
  (FIN con recursos ya liberados, pendiente de reutilizar). `proc_espera_hijo`
  verificado; `proc_termina` sin llamadas (reservado). `ppid` despierta bien.
- F4 (liberación): hallado P2 (desmapeo de 4 MB sobre copias supervisor).
  Liberación diferida vía `proc_fusiona` en IRQ confirmada segura (nunca
  libera la pila/directorio del proceso en curso; salta `proc_actual`).
- F5 (paginación): `pag_nuevo_dir`/`pag_libera_dir` con bitmaps `pd_uso`/
  `pt_uso` correctos; ventana del kernel cerrada por iteración; validación
  `pag_valida_usuario` en syscalls con puntero. Recuperación exacta (ver F).
- F6 (syscalls bloqueantes): se mantiene `optimize("O0")` en `syscall_c` y
  `cede_a` (imprescindible: locales en pila sobreviven a la cesión) y se
  extiende a `entra_kernel`. Hallado P1 (marco global, ver D).
- F7 (concurrencia): ver F (progresivas + resistencia).
- F8 (gráficos): reloj individual verificado con píxeles; 2 relojes simultáneos
  sin pánico (framebuffer/dorso compartidos: limitación documentada, ver H).
- F9 (excepciones): diagnósticos con pid/di/cr3/modo/esp/num/eip/err/cr2
  verificados en logs; etiqueta #PF corregida (P5).
- F10 (regresión): ver F.

## D. Problemas encontrados

### P0 — Crítico (confirmado, corregido). EBP no preservado al reanudar en kernel
- Archivo/función: `src/kernel32/proc.c` (`GUARDA_GPR`, `entra_kernel`).
- Causa: el cambio guardaba ESP+EBX+ESI+EDI pero no EBP; al reanudar, el EBP
  contenía el marco del proceso que cedía (otra pila) y el código retomado
  direccionaba sus locales en memoria ajena.
- Gravedad: crítica.
- Evidencia: GDB en `lee_linea` post-`hola`: `ebp=0xe0fc90` (pila del proceso
  muerto) con `esp=0x9fc04` (pila de la shell); fallo de parseo determinista
  3/3 pre-corrección, 0 tras ella (`v19_r1/r2`, `v19_f1`).

### P1 — Alto (confirmado, corregido). Marco de syscall global compartido
- Archivo/función: `src/kernel32/exec.c` (`syscall_c`, casos 8/17/25/31).
- Causa: tras reanudar, `r = syscall_regs_ptr` (global) podía apuntar al marco
  de OTRO proceso que llamó mientras tanto; el resultado se escribía en la
  pila ajena y el llamante leía el valor inicial (-1).
- Gravedad: alta.
- Evidencia: B/E/F/H con `rc=-1` sin línea EXCEPCION (`v19_aisla.log`); 8/8
  tras guardar el marco por proceso (`sc_regs` + `regs_actual`).

### P2 — Alto (confirmado, corregido). `proc_fusiona` liberaba marcos del kernel
- Archivo/función: `src/kernel32/proc.c` (`proc_fusiona`).
- Causa: `pag_desmapa(p->di, 0x200000, PAG_TAMANO)` abarca 4 MB (hasta
  0x5FFFFF) incluyendo las copias supervisor de 0x480000–0x5FFFFF heredadas
  del núcleo; devolvía marcos reservados al pool (aliasing futuro).
- Gravedad: alta.
- Evidencia: marcos libres 5664 → 6015 (+351 fantasma) (`v19_aisla3.log`);
  tras eliminar los desmapeos redundantes (todo lo de usuario ya está en
  `p->marcos[]` y el directorio privado cae entero): 5664 → 5631
  (= 5664 − 33 del `procesos32` vivo). Balance exacto.

### P3 — Medio (confirmado, corregido). Secciones cli/sti no anidables
- Archivos/funciones: `src/kernel32/proc.c` (pila kernel, `proc_crea`),
  `src/kernel32/pag.c` (`pag_nuevo_dir`, `pag_libera_dir`).
- Causa: `sti()` incondicional re-habilitaba interrupciones en mitad de una
  syscall (IF=0) o dentro de la IRQ (reentrancia), ampliando carreras en
  bitmaps de pilas/PD/PT/marcos.
- Gravedad: media. Evidencia: inspección de código. Corrección: guardar/
  restaurar IF (`cpu_eflags`/`cpu_if_restaura` en `src/kernel32/cpu.h`).

### P4 — Medio (confirmado, corregido). Cola de teclado sin módulo
- Archivo/función: `src/kernel32/teclado.c` (`tk_mete`, `kbd_tecla`).
- Causa: índices `uint8_t` sin `% TK_N`: a partir de 64 teclas se escribía
  fuera de `tk_cola[64]` corrompiendo la BSS vecina.
- Gravedad: media. Evidencia: inspección. Corrección: índices `int` con
  módulo (comportamiento verificado en todas las sesiones largas
  posteriores, >100 teclas por sesión sin anomalías).

### P5 — Bajo (confirmado, corregido). #PF mal etiquetado en diagnósticos
- Archivo: `src/kernel32/irq.c` (`nombres_ex[14]`).
- Causa: tabla desplazada desde el vector 12; el #PF (14) salía como
  "reservada". Evidencia: `reservada num=14` → `fallo de pagina num=14`
  (`v19_aisla8.log`).

### P6 — Test (confirmado, corregido). Expectativa imposible en prueba G
- Archivo: `user/apps/aislamiento32.c` (prueba G).
- Causa: esperaba 8 hijos simultáneos con el padre vivo (9 > MAXPROC=8
  huecos). Evidencia: contabilidad de slots + tabla en vivo. Corrección:
  espera 7 + drenaje de 500 ms antes de las ráfagas G/H (la reclamación es
  diferida por diseño).

### Hipótesis descartadas
- H1 (carrera en bitmaps durante creación): sin evidencia tras P3; 100+
  creaciones sin un solo fallo de asignación.
- H2 (proceso a medio inicializar seleccionado): refutada — `elige_otra`
  solo corre en contextos que no se intercalan con la publicación.
- H3 (ventana del kernel inconsistente): refutada — se abre/cierra por
  iteración, sin estado entre cesiones.

## E. Cambios realizados

1. `src/kernel32/proc.h`: campos `k_ebp` (EBP del llamante al ceder) y
   `sc_regs` (marco de syscall por proceso). Estructura interna del núcleo
   (sin efecto en ABI de usuario).
2. `src/kernel32/proc.c`: `GUARDA_GPR` guarda EBP del llamante; `entra_kernel`
   (O0) restaura EBX/ESI/EDI/EBP/ESP vía `retoma_kernel`; `proc_fusiona` sin
   desmapeos redundantes; pila/`proc_crea` con IF preservado.
3. `src/kernel32/idt32.asm`: `_retoma_kernel` (restaura registros + `ret` a la
   pila guardada; NASM porque GCC prohíbe `%ebp` en asm inline con marco).
4. `src/kernel32/exec.c`: `sc_regs` al entrar en syscall; `regs_actual()` y
   4 sitios de recarga (8/17/25/31). Se mantiene `optimize("O0")`.
5. `src/kernel32/pag.c`: `pag_nuevo_dir`/`pag_libera_dir` con IF preservado.
6. `src/kernel32/cpu.h`: `cpu_eflags()`/`cpu_if_restaura()`.
7. `src/kernel32/irq.c`: etiqueta [14]="fallo de pagina".
8. `src/kernel32/teclado.c`: índices de cola con módulo.
9. `tools/build32.py`: salida `FacilOS-v1.9-32.iso` (preserva artefacto v1.8).
10. `user/apps/aislamiento32.c`: G espera 7 + drenajes de 500 ms (G/H).
No se tocó `src/` histórico, ABI/syscalls, FS, formatos F32X ni interfaces.

## F. Pruebas

Leyenda: SUPERADA / FALLIDA / NO EJECUTADA / BLOQUEADA. Evidencias en
`build/logs/v19_*.log` (serie completa) y `build/images/v19_*.ppm`.

| Nombre | Veces | Resultado | Evidencia | Observaciones |
|---|---|---|---|---|
| Compilación + ISO (kernel 53064 B) | 8+ | SUPERADA | build32 exit 0 | Sin warnings que rompan |
| Arranque (E820 ok, paginación, 19 prog) | 20+ | SUPERADA | serie boot | Todas las sesiones |
| Menú → shell (ESC) | 20+ | SUPERADA | prompt `facil32:/>` | Sin PANIC |
| `version` / `eco` tras procesos | 12+ | SUPERADA | `v19_f1`, `v19_t1lib`… | Pre-fix FALLIDA 3/3 |
| `hola` (rc 42) + shell después | 6 | SUPERADA | `v19_f1`, `v19_t1` | Anillo 3 verificado |
| `dormir32` + shell después | 3 | SUPERADA | `v19_r4` | Bloqueo real (no busy) |
| `procesos32` listar/crear/salir | 5 | SUPERADA | `v19_t3/t4`, `v19_2gfx` | Tabla coherente |
| Crear 1 / 2 procesos | 3 | SUPERADA | `v19_t3`, plan opt1 | — |
| Crear 5 procesos | 2 | SUPERADA | batería E/H | Concurrentes |
| Ráfaga máxima (7 simultáneos, G) | 5 | SUPERADA | `v19_aisla6–9` | -104 controlado el 8.º |
| 50 simultáneos | — | BLOQUEADA | diseño MAXPROC=8 | Límite arquitectónico |
| 40 crear/esperar secuenciales (F) | 5 | SUPERADA | `v19_aisla6–9` | rc='F' siempre |
| 20 crear/esperar (planificador32/3) | 1 | SUPERADA | `v19_plan3` | rc='X' (88) |
| Resistencia (ciclos + marcos exactos) | 5 | SUPERADA | 5631=5664−33 | Sin fugas ni fantasmas |
| Bloqueos mixtos (E: dormir+esperar+muerte) | 5 | SUPERADA | batería E | rc correctos |
| Aislamiento (A/B/C/D: #PF, div0, UD) | 5 | SUPERADA | EXCEPCION + rc=-1 | Solo muere el hijo |
| `reloj` individual + captura | 2 | SUPERADA | `v19_reloj.ppm` (37k px, azul+texto) | Retorno 0 con `q` |
| Relanzar `reloj` + shell después | 2 | SUPERADA | `v19_reloj`, `v19_2gfx` | — |
| 2 gráficos simultáneos | 1 | SUPERADA | `v19_2gfx` (sin pánico) | Con limitación (ver H) |
| `t1_lib` 258 pruebas | 2 | SUPERADA | `v19_t1lib(3)` 0 fallos | sbrk/FS/tiempo |
| `t2_gfx` 12 pruebas | 1 | FALLIDA | `v19_t2gfx` 11/12 | Solo `banda1` (ver H) |
| FS crear/escribir/leer/ver/borrar | 1 | SUPERADA | `v19_t1lib` | `HOLA-MUNDO` íntegro |
| `planificador32` opt1 (2 contad. a la par) | 1 | SUPERADA | `v19_plan1` | A=B=2…14, rc 65/66 |
| Batería `aislamiento32` 8/8 | 5 | SUPERADA | `v19_aisla6–9`, `v19_t1lib2` | 0 fallos, ret 0 |

## G. Memoria y recursos

Tras la batería completa y con un proceso vivo de 33 marcos, libres =
5664 − 33 = 5631 exactos: código/pila/heap por `marcos[]`, pilas de anillo 0
por bitmap (32 ranuras), directorios PD (12) y tablas PT (96) con bitmaps y
devolución explícita, slots/PID reutilizados (`REUSE`, PID monotónicos sin
reutilizar). Sin fugas ni liberaciones fantasma. La reclamación de FIN es
diferida (`proc_fusiona` en PIT, nunca sobre el proceso en curso); los tests
de ráfaga incluyen drenaje de 500 ms por diseño.

## H. Limitaciones

1. Concurrencia gráfica: varios procesos gráficos coexisten sin pánico, pero
   comparten framebuffer/dorso (el propietario cambia con `gfx_toma` y se
   limpia el dorso): parpadeo/interferencia visual posible. Sin riesgo para
   el núcleo (punteros validados, LFB supervisor).
2. `t2_gfx` 11/12: `banda1-azul` falla porque dibuja bandas ANTES de tomar
   propiedad (la política v1.9 deniega dibujo sin propiedad y el dorso nunca
   se asigna si `destino` se deniega; `presentar` es no-op sin dorso). No hay
   corrupción ni caída; el sistema sigue estable. Preexistente (código de la
   política intacto), fuera del alcance de estabilización multitarea.
3. Salida gráfica sin doble búfer efectivo en este escenario (consecuencia de
   lo anterior): funciona, sin búfer.
4. Máximo 8 procesos de usuario simultáneos (7 hijos + padre): límite de
   diseño, con error controlado (-104), no pánico.

## I. Veredicto

**ESTABLE.**

Justificación con evidencias: ningún PANIC reproducible en ~25 sesiones QEMU
tras las correcciones; creación/finalización repetida (40+20 secuenciales,
ráfagas de 7, 5 baterías 8/8) sin errores; contexto verificado por
desensamblado y GDB; `TSS.esp0` correcto en todas las rutas de reanudación;
balance de memoria exacto; bloqueos/despertares consistentes; shell operativa
tras todas las pruebas; pruebas críticas ejecutadas y superadas. Las
limitaciones (H) son funcionales, explicadas y sin impacto en la seguridad
del núcleo.
