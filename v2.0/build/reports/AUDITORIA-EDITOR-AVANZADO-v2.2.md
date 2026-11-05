# AUDITORIA-EDITOR-AVANZADO-v2.2

Fecha: 2026-11-05. Rama: `v2.2-editor` (desde `v2.2-explorador`, commit `14a3b74`).
Objetivo: base tecnica real para el editor avanzado, sin suponer nada.

## 1. Archivo principal y estado actual

- `v2.0/user/apps/editor.c` (363 lineas): editor monodocumento, modal
  (0 = pedir archivo, 1 = editar). Buffer estatico `buf[4096]`, `len`,
  cursor `(li, co)` en BYTES, `scroll`, `sucio`, `ruta[128]`.
- Edicion UTF-8 por caracteres (`retrocede_col`, `ut_dec/ut_enc`),
  flechas 0x101-0x104, Enter, Retroceso. Sin Supr/Inicio/Fin/RePag en
  el editor (el driver si los emite: 0x106-0x10A).
- `mide()` recalcula `ini/lon` (max 200 lineas, 200 B/linea) en cada
  operacion. Dibujado: cabecera + lineas recortadas a 58 B + cursor +
  3 botones (Guardar/Abrir/Salir) + `msg`.
- `carga()` lee con `u_fread` en bucle hasta 4096 B. `guarda()` con
  `u_fopen(w)` + `u_fwrite`, informa bytes o "corto: backend lleno?".
  Acepta ruta como argumento tras el pid (`srv /ruta`).
- Sin pestañas, sin seleccion, sin formato, sin undo, sin buscar,
  sin portapapeles, sin dialogos de ficheros, sin iconos.

## 2. Bibliotecas y API disponibles

- `user/lib/ulib.h/c`: cadenas, `u_malloc/u_free/u_realloc/u_calloc`
  (first-fit sobre sbrk 0x300000-0x480000 = 1.5 MB por proceso),
  ficheros sin bufer (`UFILE`, 8 slots), `u_atan`/`u_itoa`, graficos
  (`u_texto` 256 B max, `u_rectangulo`, `u_pixel`, `u_blit`,
  `u_presentar`, `u_destino`), `u_crear/u_ejecutar`, `u_dir_hijo`
  (syscall 41), `u_renombrar` (43), `u_reloj` (44), video 47/48,
  teclado distribucion 46. Sin hora de modificadores.
- `user/lib/utext.h/c`: UTF-8 estricto (`ut_dec/ut_enc/ut_valido`,
  `ut_anterior/ut_siguiente`, NFC latino, `ut_largo`). El nucleo
  (`gfx.c utf8_dec`) es tolerante (bytes sueltos CP437) por compat.
- `user/lib/ulog.h/c`: `ulog_form` para mensajes `TERMINAL`.
- `user/lib/ugfx.h/c`: protocolo ventanas (REG/ID/DRAW/ACK, KEY/MOU/
  FOCUS/QUIT, INF, MODAL, TICK, EDIT). Eventos: `UG_EV_DRAW/F OCO/
  TECLA/RATON/SALIR`; raton `rx,ry,rbot,rueda`. Dibujo con lease
  (`ug_comienza/ug_fin`), recorte al contenido, `ug_texto` (1 llamada
  = 1 fila + `\n`, recorte visible sin partir UTF-8, max 250 B),
  `ug_boton` (5 estados), `ug_campo`, `ug_lista`, `ug_panel/check`.
  Sin soporte de estilos (una llamada = un color).
- `user/lib/tema.h`: 24 iconos 16x16 (0-23), paleta RGB24, colores
  `T_*`. Explorador usa 12-23 en barra. Servidor solo dibuja 0-11
  (tope `idx>11`), explorador hasta 23. Ampliar iconos exige tocar
  esos topes.
- `user/lib/sys.h`: syscalls 0-48. 34/35 IPC, 38/39 raton, 40 hay_ipc,
  41-44 FS/reloj, 45 CMOS video, 46 teclado, 47/48 video vivo.

## 3. Kernel relevante (solo lectura)

- `src/kernel32/exec.c`: tabla `sc_tabla[49]` (0-48), `SC_N 49`.
  `sc_13` texto (max 256, transparente), `sc_18/19` destino/presentar,
  `sc_22` blit, `sc_23` leer pixel. Sin estilos.
- `src/kernel32/teclado.c/h`: emite Unicode + TEC_* (0x101-0x10A,
  MENU 0x105, TAB 0x0F, ESC 27). Estado interno `sh_izq/sh_der`,
  `altgr`, `bloqmay`, `numlock`, `muerto` (0xE000-0xE004). **No hay
  seguimiento de Ctrl** (0x1D se ignora) y **no se expone Shift/Ctrl
  a Ring3**. Sin esto no hay Shift+flechas ni Ctrl+A/C/V/X/Z/Y.
- `src/kernel32/gfx.c`: fuente BIOS 8x16, `g_texto` transparente
  (solo tinta), `g_blit/g_rect/g_pixel/g_presentar`, dorso 2 MB o
  9 MB (1920x1080). Sin negrita/cursiva/subrayado.
- `src/kernel32/fs.c/h`: 64 entradas, nombres 31 B (UTF-8 opaco,
  `/` y NUL prohibidos), `FS_MAX_TAM16 65535`, pool RAM 512 sec
  (256 KB) sin disco. E/S parcial por offset, `sincroniza` + `flush`.
  Sin atomicidad de reemplazo (truncar al abrir en `w`).
- `src/kernel32/proc.h/ipc.c`: buzon 16 msgs x 64 B (`IPC_NMSG/MAX`).
  Todo el protocolo UGFX cabe (DRAW/KEY/MOU <= 40 B). No sirve para
  datos de ficheros (se usan syscalls FS).
- `src/kernel32/pag.c`: `MARCOS_MAX 768` por proceso; heap 1.5 MB.
- `user/apps/servidor.c`: DRAW/ACK con espera acotada, MODAL
  (hijo consola), EDIT (TAB/ESC a la app), TICK, foco siguiente al
  morir ventana, anti-parpadeo (dorso + present unico + solape).
- `user/apps/explorador.c`: referencia de patrones (toolbar 12
  botones con icono+etiqueta, direccion, arbol, detalle, estado,
  portapapeles de ARCHIVOS -no de texto-, orden, UTF-8, doble clic,
  apertura de texto via `u_ejecutar("editor","srv /ruta")`).
- `user/apps/terminal.c`: referencia de edicion UTF-8, historial,
  parser, `ug_modal` para hijos, dibujado por filas.

## 4. Limites medidos

- Heap Ring3: 1.5 MB (sbrk). Codigo+datos+heap <= ~768 marcos.
- IPC: 64 B/msg, 16 en cola. No ampliar.
- Fichero: 65535 B max, nombres 31 B, rutas 128 B (editor NNOM 128).
- Ventana min servidor: 320x200. Modos: 640x480, 800x600, 1024x768,
  1920x1080 (BGA). `ug_pide_info` devuelve PANTALLA (no ventana).
- Teclas a Ring3: ASCII/Unicode + 0x101-0x10A + TAB/ESC (con EDIT).
  Shift/Ctrl/Alt no llegan como eventos.
- `u_texto`: 256 B/syscall. `ug_texto`: 250 B/tramo, recorte visible.
- Programa v3: plano <= 65535 B, plano+bss <= 0xC0000. Terminal hoy
  62 KB: el editor avanzado debe vigilar talla (`-Os`, sin tablas
  gigantes).

## 5. Reutilizable vs ampliacion

- Reutilizable: `utext` (edicion UTF-8), `ugfx` (lease, recorte,
  botones, campos, listas), `ulib` FS/dir/video, protocolo servidor
  (DRAW/ACK/MODAL/EDIT), patrones explorador/terminal, persistencia
  `w` real, apertura por argumento.
- Ampliacion Ring3 (sin kernel): documentos heap + runs de estilo,
  seleccion raton/teclado, B/I/U por software, colores, undo por
  operaciones, portapapeles interno, buscar/reemplazar, pestañas,
  dialogos, iconos propios (tema.h 24->41), tooltips, formato `.f32r`.
- Ampliacion kernel JUSTIFICADA (minima, ABI 0-48 intacta): syscall
  49 `teclado_mod` (bit0 Shift, bit1 Ctrl, bit2 AltGr, bit3 Caps) +
  seguimiento de Ctrl (0x1D y E0 0x1D) en `teclado.c`. Sin esto los
  requisitos Shift+flechas y Ctrl+A/C/X/V/Z/Y son imposibles (el
  driver hoy descarta Ctrl y no expone modificadores).

## 6. Riesgos

- Talla del binario (techo 64 KB plano). Mitiga: `-Os`, sin fuentes
  embebidas, iconos compactos, sin printf flotante.
- Rendimiento de estilos por software (doble dibujo + shear con
  `u_blit`). Mitiga: solo lineas visibles, runs coalescentes.
- Carrera modificadores (consulta tras KEY). Aceptable: Shift/Ctrl
  siguen pulsados al procesar; se documenta.
- Escritura segura sin FS atomico. Mitiga: tmp + verificar + aviso,
  nunca marcar limpio si falla.
- Corruptos `.f32r`: validar cabecera/version/tamanos antes de
  reservar; ante duda, mensaje + conservar original + abrir como
  texto si es UTF-8 valido.

## 7. Diseño propuesto (v2.2)

- Docs heap (max 4, texto max 32768 B/doc): `txt` + `estilo` (bits
  B/I/U por byte) + `color` (indice 0-9 por byte). Estilo de
  continuaciones UTF-8 = estilo del lead.
- Undo: log de operaciones (ins/del/estilo, max 128/doc, datos con
  tope 1 KB/op; al tope se descarta la mas vieja).
- Portapapeles interno (texto+estilos, 32 KB). Sin clipboard global
  (no existe en v2.2): se documenta.
- `.f32r` v1: `F32R1\n` + `u32 tlen` + texto + `u32 nrun` +
  runs `(u32 ini, u32 fin, u8 estilo, u8 color)` coalescentes,
  validados (rangos, orden, topes). `.txt` = UTF-8 puro.
- Render: `u_texto` por runs de color; negrita = doble pasada x/x+1;
  subrayado = `u_linea`; cursiva = shear por filas con `u_blit` +
  limpieza con color de fondo del run (seleccion o ventana).
- Iconos 24-40 nuevos en `tema.h`, boton-icono propio con 5 estados
  + tooltip (descripcion en barra de estado + globo).
- Layout adaptativo: pestañas 24 px + toolbar 40 px (2 filas si
  w<560) + numeros + area + estado 36 px. Ventana 640x460 por
  defecto, min 320x200 del servidor.
- Memoria: 4 docs x (32K+32K+32K) = 384 KB + undo ~50 KB +
  clip 32 KB < 500 KB << 1.5 MB. Se mide con `u_malloc_info`.
