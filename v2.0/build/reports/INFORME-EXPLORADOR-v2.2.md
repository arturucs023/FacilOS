# INFORME-EXPLORADOR-v2.2 (rama v2.2-explorador)

## Alcance
Gestor de archivos grafico completo para FacilOS32, coherente con el
escritorio (tema, iconos y controles propios). Sin cambios de formato
de disco, sin ABI nueva (salvo `MOU x y bot giro`, ya usada por la
rueda), sin dependencias externas. Nucleo: solo se propaga el codigo
de borrado de sc_16 (`-2` carpeta no vacia, antes aplanado a `-1`;
mismo patron que renombrar).

## Implementado y probado (QEMU, capturas en build/images/)
- Ventana 640x460: barra de iconos con etiquetas (atras, adel.,
  subir, act., nuevo, carpeta, renombrar, copiar, cortar, pegar,
  borrar, propiedades) con resaltado al pasar el raton; barra de
  direccion con la ruta; arbol de carpetas plegable; vista de
  detalle Nombre/Tamano/Tipo con iconos; barra de estado (N
  elementos + orden + ruta); dialogo de nombre y dialogo de
  propiedades (nombre, tipo, tamano/entradas, ubicacion).
- Navegar, subir, atras/adelante (historial), actualizar, ordenar
  por nombre/tipo (clic en cabecera o tecla `o`).
- Crear archivo/carpeta, renombrar, copiar y pegar, cortar y pegar
  (mover), eliminar (fichero y carpeta vacia), `no vacia` y
  `ya existe`/`CD: solo lectura` donde toca.
- Nombres con espacios y `niño` (ES): crear, listar y ver bien
  (UTF-8 opaco en FS, sin partir multibyte al mostrar).
- Directorios anidados (`/datos/sub`), vacios (`(vacia)`) y llenos.
- Persistencia con disco (`--disco` + HDD): creado `/guardado/nota`,
  reinicio y sigue ahi (carpeta y archivo).
- Ejecutar desde el explorador: los programas de consola van en
  modal; los de ventana (p. ej. terminal, editor) cancelan el modal
  y conviven con el escritorio (antes colgaba todo).
- Ratón: botones, arbol (plegar/navegar), seleccion y doble clic,
  cabeceras y rueda. Teclado completo incl. Inicio/Fin y ESC que
  ya no mata el escritorio (modo edicion).
- No se rompe nada: terminal, editor, sysinfo y configura siguen
  igual; shell intacta.

## Implementado pero no probado
- Doble clic (ventana de 0,5 s verificada por codigo; en QEMU el
  harness no da dos clics tan seguidos): abrir con Enter cubre
  el mismo camino (`abre_sel`).

## Parcial
- Copiar/mover: solo archivos (las carpetas se crean, renombran y
  borran vacias; mover carpetas entre rutas no esta soportado).
- Propiedades de carpeta: cuenta entradas (sin tamano recursivo).

## No implementado
- Sin Borrar: no hay papelera (borrado directo con aviso).
- Sin arrastrar y soltar, sin busqueda, sin vistas miniatura.

## Bloqueado / fuera de alcance
- t2_gfx: base v2.1 = 11/12 (banda1-azul preexistente, reproducido
  aqui mismo en su rama); en v2.2 = 9/12 estables (banda0-negro,
  banda1-azul, transparente-fondo). Viene de la etapa de video,
  anterior a esta tarea; no lo toca el explorador (en shell no
  hay servidor). Pendiente de aislar en su propia entrega.
- Editar con doble clic abre el editor de ventana (`editor`),
  no el de consola; el editor decide guardado.

## Regresion v2.2 ejecutada (misma ISO final)
- t1_lib 258/258, tutf8 26/26, mem32 15/15, disco32 38/38: verde.
- t2_gfx: ver Bloqueado.

## ISO
- `build/iso/FacilOS-v2.2-terminal.iso` (1 518 400 B, 39 programas).
- Evidencias: `build/images/exp_*.png`, `flick*.png`,
  `exp_persist_*.png`, `exp_foco_ok.png`, `exp_ejec_*.png`.
