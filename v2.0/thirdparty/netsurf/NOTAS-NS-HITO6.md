# Hito 6 NetSurf — Fidelidad visual (hoja UA, subrayados, fondos, negrita, buscador)

Rama `v2.4.3`, sin tocar `main`. FacilBrowser intacto (no se mira;
referencia única: NetSurf real).

## 1. Hoja de agente de usuario

- `NV_UA_CSS` fusionada DELANTE de la hoja `<style>` en una sola
  stylesheet: `a:link{color:#0000ee;text-decoration:underline}` +
  `b,strong,h1-h6{font-weight:bold}`. Gana el autor por orden y
  especificidad; se evita la cascada multi-hoja (una sola hoja
  como en Hito 5, camino probado).
- Lecturas nuevas por elemento: `text-decoration` (bit UNDERLINE),
  `text-align` (izq/centro/der), `font-weight` (>=600 = negrita),
  fondo "explicito" (autor/inline) para ancho completo.

## 2. Pintado

- Enlaces: color de la cascada + subrayado POR TRAMO (columnas
  `[c0,c1]` por enlace y fila; la fila entera no se subraya).
- Bloques con fondo propio: a todo el ancho; el resto, tras el texto.
- `text-align:center/right` con desplazamiento real.
- Negrita por doble pasada (única fuente 8x16, sin tamaños).
- Imágenes: como en Hito 5 (verificado en ventana).

## 3. Buscador en la barra

- `nv_ir`: URL tal cual | dominio sin esquema (se antepone
  `http://`) | texto libre → búsqueda (`nv_es_busqueda`,
  `nv_busca_url` a `https://html.duckduckgo.com/html/?q=`).
- Probado en `-p` como constructor puro (sin red).

## 4. Repintado determinista (Hito 6, hallazgo)

- El servidor solo compone en DRAW propio; el dibujo explicito
  tras `nv_cargar` no se hace visible (ventana en "cargando").
- Regla: solo se pinta en `UG_EV_DRAW`. Tras cada cambio
  (carga inicial, seguir enlace, historial, recarga, scroll,
  foco) se arma `ug_tick(v,1)`; el DRAW resultante pinta y
  cancela con `ug_tick(v,0)`. Sin TICK la ventana quedaba
  congelada en el primer paint.

## 5. Bugs cazados (todos silenciosos: libcss devuelve OK)

- Atributos `class`/`id` no se guardaban en el DOM: ninguna
  clase casaba (visible al probar `.v`).
- `node_classes` devolvía un único buffer estático que libcss
  compara contra sí mismo al buscar candidatos: falso
  compartido de estilos entre nodos distintos. Ahora tabla
  por llamada (heap, liberada por documento) + refs propias
  (`lwc_string_ref`), porque libcss libera (`unref`) lo que
  recibe y el internado aliasa entre llamadas.
- La restauracion de estilo al cerrar elemento pisaba el
  fg/bg de la fila en curso: los elementos en linea (enlaces)
  perdian su color. La fila adopta el estilo al añadir texto.
- Fondo transparente leído como negro: se filtra por alfa 0.
- Filas creadas antes de conocer el contexto del bloque:
  `nv_bloque` propaga full/alineacion a la fila vacía.

## 6. Resultado

- `navega -p`: 25 → 32 pruebas (`ua-azul/sub/lleno/neg`,
  `ua-clsdiv`, `busca-url/busca-det`), 0 fallos.
- Ventana verificada en QEMU: banner navy centrado en negrita,
  nav rojo, enlaces azules subrayados por tramo, caja amarilla
  full-width, imagen BMP, lista, buscador; Tab/Intro/Retroceso
  con GETs y repintado (capturas demo4, h4d, h4f).
- FULL: 729 pruebas (navega 32), 0 fallos.
- Tope `navega` 1310720: plano ~1270K + bss 20404.

## 7. Límites honestos (no es pixel-clavado)

- Una sola fuente 8x16: sin tamaños reales; negrita simulada.
- Una columna: sin flotantes ni tablas lado a lado (las celdas
  apilan como bloques); sin formularios (no hay inputs/botones
  web; la "caja de búsqueda" de las páginas no se renderiza).
- Subrayado por tramo y color por fila (un estilo por fila).
- Sin JS. PNG/JPEG: sin decodificador (alt). HTTPS real e
  internet no probados en QEMU (sin DNS/ruta; stubs locales).
