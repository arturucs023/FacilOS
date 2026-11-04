# INFORME-FINAL-v2.2 (FacilOS32 — terminal propia, castellano, Unicode)

Rama: `v2.2-terminal-castellano` (base v2.1-p1-tiempo @8ba820e).
ISO: `FacilOS-v2.2-terminal.iso` (1517568 B). Solo QEMU (sin HW físico).

## IMPLEMENTADO Y PROBADO

- Terminal v2 Ring3 (~20 comandos reales, registro central CMDS,
  parser comillas+`nombre=valor`+`si`/`no`, TAB visual con ENTER que
  completa sin ejecutar, ayuda/manual, historial editable, UTF-8,
  registros ulog en castellano). Ciclo CRUD + anidada `/T/Sub`
  verificado en capturas + cross-check shell.
- UTF-8: utext (tutf8 26/26), driver NFC, gfx/consola, € propio,
  shell/editor/explorer/ug_campo por carácter. `año.txt`, `Mi Carp`,
  persist-reboot, editor roundtrip `C3 B1` exacto.
- Teclado ES/CA/US + muertas + AltGr (€, |@#~) + TAB/Supr/Inicio/Fin;
  syscall 46; persistencia cabecera byte 15 (CA sobrevive a reboot).
- Servidor EDIT opt-in; CWD propio de terminal; FS admite espacios
  (superset, `/` prohibido, sin cambio de formato).
- Fixes de fase: `parte_sys` multinivel (Ring3 no podía tocar
  subdirs), año `mostrar_fecha` 0026→2026, espejo UART con signo,
  € al scancode 0x06, staging CD 0x20000→0x30000 + linker `.rdata`
  explícito (pánico de arranque por solape al crecer el núcleo),
  KERN_SECS 128→160.

## IMPLEMENTADO NO PROBADO

- `estres_mix` en v2.2 (ver §Regresión).
- 640x480/800x600 sobre el binario final (heredado v2.1-p1; el
  bootloader/modos no se tocaron salvo KERN_SECS/staging).

## PARCIAL

- Glifos: todo ES/CA/diéresis/puntuación verificados; faltan
  Ò/Ù/Ý/Ø/Þ/astrales → `?` (documentado). VGA texto: `?` para €.
- NFC: composición latina en borde; VFS byte-exacto (dos formas
  distintas = dos ficheros, precedente Linux, documentado).
- Cobertura de ejecución por comando: verificados E2E ayuda,
  manual, crear_carpeta (+padres, +anidada), crear_archivo,
  eliminar_archivo, listar_archivos (simple y detallado),
  entrar_carpeta, mostrar_ruta, copiar_archivo (+verificar), limpiar_pantalla, informacion_sistema,
  mostrar_fecha/hora, teclado, historial, eco, ejecutar, salir.
  Solo probados a nivel parser/registro (sin ejecución E2E en
  esta pasada): `mover_archivo`, `cambiar_nombre`,
  `mostrar_contenido`, `eliminar_carpeta`, `orden=tipo`. Su código
  comparte los mismos resolvedores y syscalls ya verificados
  (`eliminar_carpeta` usa `op_eliminar` con la protección -2 del
  núcleo).

## NO IMPLEMENTADO (fuera de alcance, documentado)

- Globbing, tuberías, paginador, red/SSH, Ctrl-combinaciones,
  NFC completo con tablas Unicode, más distribuciones.

## BLOQUEADO

- Nada técnico. Harness: QEMU pierde ~5 % de teclas rápidas (gaps
  ≥1 s + verificación visual); `sendkey` sin `"` (mapeado a
  shift-2); `mouse_move` relativo.

## Regresión

Ver INFORME-REGRESION-v2.2.md. Todo verde salvo preexistentes
(t2_gfx banda1-azul; estres crear-bajo-carga, idénticos en base).
Tras el fix `parte_sys` se repite batería sobre el binario final
(ver §Verificación final).

## Verificación final (binario definitivo, 04/11/2026)

- `tfinal`: `mostrar_fecha` → `04/11/2026`; manual con `Categoría:`,
  `Descripción:`, `Documentación:` acentuados; `crear_archivo
  "año.txt"` (ñ física) → `Archivo creado.`; explorador →
  `F año.txt (0)` con ñ.
- Regresión `t_reg2`: tabla §Regresión (estres_mix 8/8 incluido).
- Sanity 1024: `reboot ok: True`, captura 1024x768, terminal pid=2.

## Archivos principales

`user/apps/terminal.c` (nueva), `src/kernel32/teclado.c`,
`gfx.c`, `consola.c`, `exec.c` (sc46 + parte_sys), `fs.c`
(espacios + cab15), `shell.c` (`teclado ca`), `link32.ld`,
`boot32.asm`, `cd.h`, `user/lib/utext.*`, `ulog.*`, `ugfx.*`,
`sys.h`, `ulib.h`, `user/apps/{servidor,editor,explorador}.c`,
`tutf8.c`, `tools/{build32,qrun}.py`, 8 informes en
`build/reports/`.

## Memoria

Terminal 61460 B disco, ~30 KB BSS (pant 15 KB + hist 10 KB);
utext ~1,5 KB + tablas núcleo ~200 B + € 16 B; núcleo <160
sectores (81,9 KB cap). Sin asignaciones sin tope nuevas.
