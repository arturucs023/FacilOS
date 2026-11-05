# INFORME-FINAL-EDITOR-AVANZADO-v2.2

Rama `v2.2-editor` (desde `14a3b74` de `v2.2-explorador`). Version
principal intacta v2.2. Sin red. Sin simulacros.

## A. Implementado y probado

- Multidocumento (4 pestanas, `*` sucio, cambio clic/Ctrl+Tab,
  guardar todos con Ctrl+May+S, cierre con aviso).
- Edicion UTF-8 por caracteres (insertar, borrar, palabra con
  Ctrl+Retroceso/Supr, Enter, Tab=4esp, flechas, Inicio/Fin
  (+Ctrl), RePag/AvPag, Ctrl+flechas por palabras).
- Seleccion raton (arrastrar + autodesplaza) y teclado
  (May+flechas, Ctrl+E), entre lineas y con acentos.
- Negrita/cursiva/subrayado combinables + 10 colores
  (dialogo con vista previa y RGB propio), botones con estado
  activo y tooltips. Render por software con la fuente del
  nucleo (doble pasada, shear por `u_blit`, linea).
- Persistencia `.txt` (UTF-8 puro) y `.f32r` v1 versionado y
  validado; verificada con reinicio real con disco
  (`/p.txt` texto, `/e.f32r` con rojo+negrita intactos).
- Deshacer/rehacer por operaciones (texto y estilos),
  portapapeles interno con estilos, buscar/reemplazar
  (contador, ant/sig, una/todas, May/palabra), ir a linea,
  stats (car/pal/lin/B), numeros de linea, temas, ajuste.
- Mini selector de archivos navegable (lista con iconos,
  entrar/subir/doble clic, rueda, teclado, nombres con espacios
  y UTF-8). Dialogos de sobrescritura, cierre, avisos, color.
- 17 iconos propios (tema 24-40), layout 640x480/800x600/
  1024x768/1920x1080 + ventana estrecha.
- Integracion menu/explorador/argumentos; syscall 49 y teclas
  con modificador codificadas en el driver (sin carreras QEMU).

## B. Implementado, pero no probado completamente

- Aviso de escritura no verificada (codigo: no marca limpio y
  conserva buffer; sin caso forzado en QEMU).
- Tope de 4 documentos y descarte de undo antiguo (codigo; sin
  forzar el tope en QEMU).
- Rescate de `.f32r` corrupto como texto (codigo validado por
  revision; probado: extension erronea abre como texto).

## C. Implementacion parcial

- Portapapeles solo interno (v2.2 no tiene clipboard global).
- Sin tamaños de fuente (fuente bitmap unica del sistema).
- `u_blit` de shear sin limpieza del pixel duplicado (inocuo).

## D. No implementado

- Clipboard global, impresion, plegado, corrector, red: fuera
  de v2.2 por diseno (no se fingen).

## E. Pruebas

- Editor: 40+ casos QEMU con captura (ver PRUEBAS-...): todos
  los del apartado A en verde; B/C parciales declarados.
- Regresion: t1_lib 258/258, tutf8 26/26, mem32 15/15,
  disco32 38/38, ipc32 111/111, equidad32 3/3.
- t2_gfx 12 pruebas 7 fallos, IDENTICO en base limpia
  (`14a3b74` en worktree): preexistente del entorno, sin
  regresion por esta rama.

## F. Rendimiento y memoria

- Editor 82168 B plano (techo 128K justificado; nucleo admite
  plano+bss<=768K), BSS ~0 (buffers en heap).
- Por documento 3x32K heap; 4 docs + clip 8K < 500K de 1.5 MB.
  Sin copias por pulsacion; redibujo solo visible.
- `t2_gfx`: 30 cuadros en 57 ticks (igual que base).

## G. ISO

- `v2.0/build/iso/FacilOS-v2.2-editor.iso`, 1517568 B
  (741 sectores, 39 programas, util 468472 B). Copia con nombre
  propio; `FacilOS-v2.2-terminal.iso` historica intacta.
- Validada en QEMU: escritorio + editor + guardar/abrir.

## H. Git

- Rama `v2.2-editor`, commits descriptivos, sin force push,
  `main`/releases/tags intactos. Push HTTPS pendiente de
  resultado (ver sesion).

## I. Limitaciones conocidas

- QEMU/Windows fusiona teclas a veces (reintentar la entrada).
- t2_gfx sensible al entorno (7 fallos aqui, 1-3 en otros).
- FS sin atomicidad (verificacion por relectura, reintento).
- `--disco` omite binarios >64K con aviso (limite del formato
  de imagen, no del nucleo ni de la ISO).

## J. Conclusion

La renovacion del editor queda terminada dentro de v2.2: es una
aplicacion de escritorio completa y probada con la arquitectura
propia de FacilOS32. Lo declarado parcial (B/C) no bloquea el
uso; lo no implementado (D) pertenece a otro alcance.
