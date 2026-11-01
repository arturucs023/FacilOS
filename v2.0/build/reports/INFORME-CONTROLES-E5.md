# INFORME-CONTROLES-E5 (Fase G)

Estado: IMPLEMENTADO + PROBADO (en apps reales, verificado por píxeles).

## Inventario (`ugfx.c`, dibujo + hit-test; el estado lo guarda la app)
- Botón (NORMAL/HOVER/PULSADO/OFF) + `ug_clic` (flanco de subida).
- Etiqueta, panel (borde+relleno), casilla (`ug_check`).
- Campo de texto (`ug_campo` con cursor si foco + scroll horizontal;
  `ug_campo_tecla`: insertar, retroceso, izq/der).
- Lista (`ug_lista` con selección+scroll; `ug_lista_tecla` arriba/abajo;
  `ug_lista_clic` por coordenadas).
- Menú: el del escritorio vive en el servidor (ver GESTOR-VENTANAS);
  las apps usan botones/listas como menús (editor: Guardar/Abrir/Salir).
- Barra de título y barra de tareas: las dibuja el servidor.

## Uso real (no decorativo)
- Terminal: campo de entrada con cursor, historial, inserción/borrado.
- Explorador: lista navegable (teclado+ratón), campo de nombre, visor.
- Editor: campo de nombre, botones Guardar/Abrir/Salir clicables,
  cursor de edición multilínea.
- `ugfx32`: botón conmutador, casilla, campo, lista (ejemplo).

## Pruebas (píxeles + funcional)
- Botón [Guardar] clicado con ratón real → archivo guardado y leído
  idéntico por shell (`v50_ed2`, `v50_per2/3`).
- Campo: escritura `nota1`, `doc.txt`, `Hola FacilOS` verificada por
  listado/lectura (`v50_exp8`, `v50_mkd`, `v50_per2`).
- Lista: selección azul + texto blanco, navegación y clic (`v50exp3`,
  `v50vw0/1` con 529 px de contenido).
- Casilla/botón de `ugfx32`: compilado y enlazado (cobertura vía apps).

## Límites
- Sin selección de texto con ratón; sin portapapeles; sin Tab en campos
  (Tab = cambiar foco, decisión documentada).
- Sin menús contextuales ni desplegables en apps (el menú global cubre).
- Sin doble clic (sin política temporal; clic simple).
