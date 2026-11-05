# DISENO-EDITOR-AVANZADO-v2.2

Rama `v2.2-editor`. Todo Ring3 salvo dos ampliaciones minimas de
nucleo. Sin red, sin v2.3.

## 1. Modelo de documento

- Maximo 4 documentos (`MAXDOC`), texto maximo 32768 B por
  documento (`MAXTXT`). Tres buffers paralelos por documento
  (reservados con `u_malloc` al crear):
  `txt` (UTF-8), `est` (bits 1=N 2=K 4=S por byte),
  `col` (indice 0-9 por byte). Los bytes de continuacion UTF-8
  heredan el estilo del caracter (las operaciones son por
  caracter via `ut_*`, nunca parten secuencias).
- Cursor y ancla en BYTES (`cur`, `anc`); seleccion = `[min,max)`.
- Lineas logicas (`\n`); con ajuste se parten en filas visuales de
  `vis_cols` celdas. Sin ajuste hay scroll horizontal (`scol`).
- Estilo pendiente (`pen_est`, `pen_col`): sin seleccion, los
  botones B/I/U y el color cambian lo que se escribira; con
  seleccion, cambian la seleccion (si todo tiene el bit se quita,
  si no se pone; el estado mixto se muestra neutro).

## 2. Render con estilos (software, fuente del nucleo)

- Por tramos de igual color: `u_texto` con RGB (`color_rgb`).
- Negrita: doble pasada en x y x+1.
- Subrayado: `ug_linea` en y+14 del color del tramo.
- Cursiva: shear por filas con `u_blit` (mitad superior 1 px a la
  derecha) sobre el tramo ya dibujado. Solo lineas visibles;
  tramos coalescentes. Sin fuentes embebidas (techo de 128K plano).
- Seleccion: fondo `T_SELECCION` detras, colores intactos; busqueda:
  fondo marron. Cursor: rect 2 px si foco.
- Tema oscuro (fondo `T_VENT_FONDO`, texto blanco) y claro
  (fondo blanco, texto negro). Negro/blanco se reasignan segun tema
  para no ser ilegibles. Gris = seleccionable.

## 3. Undo/redo por operaciones (max 128 por pila y documento)

- Insercion: guarda estilos (d0=est, d1=col). Borrado: guarda
  texto + estilos (d0=txt, d1=est+col 2n). Estilo: guarda anterior
  (2n). Redo simetrico. Operaciones >8K vacian su pila (acotan
  memoria). Deshacer/rehacer recolocan cursor y ancla.

## 4. Portapapeles interno (8K)

- Copiar/cortar/pegar + todo dentro del editor, con estilos.
- Sin clipboard global (v2.2 no lo tiene): se documenta en el
  mensaje ("(interno)") y aqui. No se finge.

## 5. Buscar/reemplazar (UTF-8)

- Subcadena sobre bytes (UTF-8 autosincronizado), tope 200
  coincidencias, resaltado, contador, anterior/siguiente.
- Opciones: Mayusculas (plegado ASCII) y palabra completa
  (delimitadores = no-palabra). Cambiar actual y todas.

## 6. Formato `.f32r` v1 (solo con esa extension)

- `F32R1\n` + u32 tlen + texto + u32 nrun + runs de 10 B
  (u32 ini, u32 fin, u8 estilo, u8 color), coalescentes.
- Validacion antes de reservar/usar: magia, tlen<=32K,
  nrun<=tlen+1 y <=4096, rangos dentro, estilo<=7, color<=9.
  Corrupto: mensaje + original intacto + intento de rescate como
  texto si es UTF-8 valido. `.txt` = UTF-8 puro (sin metadatos).
- Escritura directa + verificacion por relectura de tamano; ante
  fallo no se marca limpio y se conserva el buffer (el FS no es
  atomico: sin tmp por limite de 31 B en nombres).

## 7. Teclado (driver + syscall 49 + codigos)

- Nucleo: sigue Ctrl (0x1D/E0-0x1D), expone modificadores en
  syscall 49 (bit0 May, bit1 Ctrl, bit2 AltGr, bit3 Caps).
- Sin carreras: el driver codifica en la tecla Ctrl+letra
  (0xE100+idx, 0xE200 con May), Ctrl+Tab (0xE120/0xE220 cambio de
  pestana), Ctrl+Retroceso (0x7F borra palabra) y May/Ctrl en
  especiales (bits 0x200/0x400). La app decodifica (`dec_tecla`);
  la 49 queda de apoyo. Terminal y campos la ignoran/filtran.
- Atajos: N/O/S/G(+May todos)/Z(+May redo)/Y/X/C/V/E/F/R/B/I/U/
  K(color)/T(tema)/J(ajuste)/W(cerrar)/L(ir a linea)/Tab(pestanas)/
  Ctrl+RePag/AvPag (pestanas). Flechas+May seleccionan,
  Ctrl+flechas por palabras, Inicio/Fin (+Ctrl), RePag/AvPag,
  Supr/Retroceso (+Ctrl palabra), Enter, Tab=4 espacios.

## 8. Iconos y presentacion

- 17 iconos nuevos 24-40 en `tema.h` (mismo estilo/paleta 16x16):
  nuevo, abrir, guardar, guardar-como, deshacer, rehacer, cortar,
  copiar, pegar, todo, buscar, reemplazar, negrita, cursiva,
  subrayado, color, cerrar-doc. Un icono por boton, sin reusos.
- Botones 30x30 con 5 estados (normal/hover/pulsado/off/activo),
  tooltip flotante + descripcion en la barra de estado.
- Layout: pestanas 24 + toolbar 34 (2 filas si w<560) + numeros +
  area + estado 36. Ventana 700x500 (min servidor 320x200).
  Verificado 640x480, 800x600, 1024x768, 1920x1080.

## 9. Dialogos y mini selector

- Abrir/guardar-como con mini selector navegable: raiz + pila de
  8 (el FS no expone padre), carpetas primero, doble clic/Enter
  entra-abre, Retroceso/subir sube, rueda desplaza, campo Nombre
  (con `/` = ruta literal por compatibilidad).
- Confirmar cierre (Guardar/Descartar/Nada), sobrescribir,
  avisos, color (10 muestras + RGB propio + vista previa),
  ir a linea, panel buscar. Teclado completo en todos.

## 10. Memoria y limites

- Por documento: 3x32K + undo (~2xops) ; 4 docs + clip 8K < 500K
  << heap 1.5 MB. Sin copias completas por pulsacion. Redibujo
  solo visible. BSS estatico minimo (buffers en heap).
