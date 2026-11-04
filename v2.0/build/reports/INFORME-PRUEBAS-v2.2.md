# INFORME-PRUEBAS-v2.2 (FacilOS32 v2.2-terminal-castellano)

ISO: `FacilOS-v2.2-terminal.iso`. Método: QEMU + inyección `sendkey`
con gaps ≥1 s (el host pierde ~5 % de teclas rápidas; se verifica la
línea en captura antes de RET en los casos críticos), serie UART y
`xp`/screendump. `build/logs/` + `build/images/` conservan la evidencia
(ignorados por git).

## A. UTF-8 (`tutf8`: 26/26, rc 0)

ASCII, ñ (C3 B1), € (E2 82 AC), continuación suelta, incompleta,
sobre-larga, sustituto, codificación 2/3 B + topes, validez, largo,
anterior/siguiente, compone (aguda/grave/circunflejo/diéresis/
virgulilla/cedilla + mayúsculas), NFC (`a`+U+0301 → á).

## B. Teclado físico (serie + captura)

- ES por defecto: ñ (0x27) letra a letra; `teclado ca` → ç directa
  (serie `C3 A7`); `teclado es` de vuelta.
- Muertas ES: apóstrofe (0x28) + a → á (`C3 A1` en serie); +a tras
  `ab` → `ab`+`C3 A1`; espacio tras muerta → literal.
- Shift-3 → `·` (`C2 B7`); AltGr-5 → `€` (`E2 82 AC`, scancode 0x06
  corregido durante la fase: estaba en 0x05).
- TAB llega (selector), Supr/Inicio/Fin/RepAg/AvPag mapeados.
- Layout persiste en cabecera byte 15: `teclado ca` + `system_reset`
  + disco scratch → `Teclado actual: catalan.`

## C. Terminal: parser y comandos

- `mostrar_ruta` → `/`, luego `/T` tras crear+entrar (captura).
- `crear_carpeta T` → `Carpeta creada.`; `entrar_carpeta T` OK.
- `crear_archivo f.txt` en subdir: FALLÓ al inicio (TER-0017) →
  causa raíz: `parte_sys` rechazaba rutas absolutas multinivel;
  ningún Ring3 podía abrir/crear en subdirs. Corregido (extensión
  compatible) y re-verificado en `t_files` sobre la ISO final:
  `Archivo creado.`; `copiar_archivo f.txt g.txt verificar=si` →
  `Archivo copiado.`; `listar_archivos formato=detallado` →
  `F f.txt (0)` + `F g.txt (0)` (tamaños reales);
  `eliminar_archivo f.txt` → `Archivo eliminado.` y listar → solo
  `g.txt`; anidada `/T/Sub` + `crear_archivo h.txt` →
  `Archivo creado.`; shell cross-check en `/T` → `g.txt`.
  (Nota: `ver_archivos` de la shell solo lista tipo 2 —shell.c:247—,
  por eso `T` no sale en raíz: heredado, no bug.)
- `copiar_archivo` origen inexistente → TER-0020 (correcto);
  `listar_archivos` vacía → `(vacia)`; `eliminar_archivo`
  inexistente → TER-0020; `mostrar_fecha` → `03/11/2026` (bug 0026
  detectado y corregido en esta fase); `mostrar_hora` → `20:28:56`;
  `historial` numera los 9 comandos; `informacion_sistema` →
  libres=5580, procesos=3, ventana=480x320.
- Errores con formato ulog `[2026/11/03 HH:MM:SS] [ERROR]
  [TERMINAL] TER-NNNN:` (capturas).
- Shell cross-check (`ver_archivos`) tras cada ciclo.

## D. TAB / ayuda / manual / historial

- `cre`+TAB → selector con crear_*; ↓+ENTER completa SIN ejecutar;
  segundo ENTER ejecuta. ESC cierra. Una coincidencia completa.
- `manual crear_carpeta` → ficha (categoría/descripción/sintaxis/
  opciones/ejemplo). `ayuda` por categorías. Inexistente →
  sugerencias. (Re-captura de acentos con ISO final: pendiente.)
- Historial ↑/↓ con borrador preservado (captura `thist`).

## E. Unicode E2E (archivos + apps)

- `año.txt` (ñ por tecla física) creado en terminal, listado en
  shell con bytes `C3 B1` exactos; `Mi Carp` (espacio) OK.
- `persistñ.txt` sobrevive a `system_reset` con disco scratch.
- Editor: `hola ñmundo` escrito con ñ física, guardado con ratón,
  releído `C3 B1` exacto por serie.
- Explorador con nombres Unicode: VERIFICADO (`tf_explorer.ppm`):
  `F año.txt (0)` renderizado con ñ en la lista (creado desde la
  terminal con la tecla física ñ en la misma sesión).
- Registro acentuado: VERIFICADO (`tf_manual.ppm`): `Categoría:`,
  `Descripción:`, `Documentación:` con tildes en pantalla.

## F. Resoluciones

- 1024x768 con reboot persistente verificado sobre el binario
  definitivo (`p1post.ppm` 1024x768, `reboot ok: True`, terminal
  lanzada pid=2 el 04/11/2026).
- 640x480/800x600: heredado v2.1-p1 (el bootloader/modos no se
  tocaron salvo KERN_SECS/staging, verificados en el arranque).

## G. Cobertura de comandos

Ejecutados E2E: ayuda, manual, crear_carpeta (+padres, +anidada),
crear_archivo, eliminar_archivo, listar_archivos (simple/detallado),
entrar_carpeta, mostrar_ruta, copiar_archivo (+verificar),
limpiar_pantalla, informacion_sistema, mostrar_fecha/hora, teclado,
historial, eco, ejecutar, salir. Solo parser/registro:
`mover_archivo`, `cambiar_nombre`, `mostrar_contenido`,
`eliminar_carpeta`, `orden=tipo` (mismo resolvedor/syscalls).
