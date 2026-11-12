# Hito 3 NetSurf — Visor `navega` (hubbub → libdom → select CSS → texto)

Rama `v2.4.3`, sin tocar `main`. FacilBrowser congelado (solo se
añade el comando `navega` al terminal, espejo de `fbrowser`).

## 1. Qué es `navega`

`v2.0/user/apps/navega.c` (~1800 líneas, sin código GPL en el
binario: el puente y el `select_handler` son propios):

- Carga: `http(s)://` vía `nsfetch_get` (Hito 2) o fichero local
  (tope 64K, documentado).
- Parse: `hubbub_parser` (tokens) → árbol `libdom` construido a
  mano (`create_document` + `create_element`/`create_text_node` +
  `append_child`). `<script>`/`<style>`/`<title>` conmutan
  `content_model` (CDATA/RCDATA); el CSS de `<style>` (8K) y el
  `<title>` se capturan en el puente. HTML malformado tolerado
  (error handler nulo).
- Estilo: `css_select_style` por elemento con `css_select_handler`
  propio (nombres, clases, id, antecesor/padre/hermanos,
  `= ~= |= ^= $= *=`, `:root`, conteo de hermanos, `:empty`,
  `:link`, resto a falso, sin hints, sin `style=` en línea).
  Se lee `color` y `background-color` (herencia manual para el
  color); el resto de propiedades se ignora. `node_classes`/`node_id`
  devuelven cadenas internadas (libcss las libera); los nodos
  devueltos son prestados del árbol (sin `unref`, fuga acotada y
  documentada).
- Layout: texto a 76 columnas con colapso de blancos que sobrevive
  a límites entre nodos en línea, bloques, `br`/`hr`/`li`/`pre`,
  entidades vía hubbub (`&amp;` → `&`).
- Modos: `-p` vuelca resumen a consola (batería y ficheros);
  con `<srv>` abre ventana ugfx 640x460 (barra URL + R/X/Ir,
  scroll rueda/teclas/pag/barra, estado con título).

Límites honestos (banner y §5): sin JS, sin imágenes, sin
formularios, sin `style=` en línea, sin tabla real (celdas como
bloques), ficheros ≤ 64K.

## 2. Batería `-p`: 9 pruebas, 0 fallos (QEMU + http_stub)

`pagina-*` (título `Inicio Facil`, ≥4 filas, `FacilBrowser`,
entidad `Segunda`) y `css-*` (título `CSS`, ≥2 estilos, texto de
clase) sobre `http://10.0.2.2:18080/{pagina,css}`.

## 3. Build, terminal y harness

- `build32.py`: `navega` con `con_ns1` (dom/css/hubbub/wapcaplet…)
  + `con_br` (BearSSL + `nsfetch`); tope 1245184 (medido 1172756
  plano + 20404 bss, bajo el límite 1536K del núcleo). Fix:
  `NSHIM` ya venía en `NSOBJS` — con `con_br` solo se añade suelto
  si no hay `con_ns/con_ns1` (duplicaba símbolo).
- `terminal.c`: comando `navega [url]` (espejo de `fbrowser`).
- `ftest.py`: `navega -p` en FULL + IMPACTO + stub http; el
  `split` de veredictos ahora tolera argumentos (`ejecutar X args`).
- Ventana verificada en QEMU (F1 → terminal → `navega URL` +
  `screendump`): barra, texto con espacios correctos y colores
  del `/css`. Dos hallazgos y fix: (1) `u_print` dentro de `dibuja`
  rompe el protocolo del dorso (las trazas van antes de
  `ug_comienza` o fuera); (2) el primer `DRAW` a veces no se
  compone: la URL inicial se carga en el primer `DRAW` con
  repintado explícito (`NV_ini`).
- FULL: 704 + 9 = 713 pruebas, 0 fallos.

## 4. Siguiente (Hito 4, pendiente de autorización)

Enlaces clicables + historial + barra de estado de red; luego
PNG/JPEG vía nsbmp/nsgif e `inline style=` (optativo).
