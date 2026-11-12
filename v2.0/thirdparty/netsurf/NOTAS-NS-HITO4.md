# Hito 4 NetSurf — Enlaces, historial y estado de red en `navega`

Rama `v2.4.3`, sin tocar `main`. FacilBrowser intacto.

## 1. Enlaces clicables

- Puente: `nv_tok` guarda `href` (cap 255) como atributo DOM real
  en cada `<a>` (`dom_element_set_attribute`); el layout lo lee
  con `nv_atributo` (ya endurecido en Hito 3: solo elementos).
- `nv_resuelve`: `https://` absoluta directa (con validación
  mínima de host; el resolver `u_html_resuelve_url` de ulib solo
  acepta `http`, y no se toca la lib compartida); resto vía ulib
  (incluye `..`, `#frag` y `?q=`).
- Tabla `NV_enl` (64): filas `[f0,f1]` por enlace; clic, Intro con
  foco y Tab la recorren (el más interno gana). Foco pintado con
  banda `T_SELECCION`.
- Batería `-p` (stub `/enlaces`, 6 enlaces): número, absoluta,
  raíz `/pagina`, relativa `pagina`, `https://example.com/`.

## 2. Historial

- Pila de 16 URLs con scroll por entrada (`NV_hscroll`), como
  `fbrowser`: empuja al navegar (recorta futuro, dedup), atrás
  (Retroceso/botón `<`), adelante (botón `>`). Barra `< > R X`
  con estados OFF.
- Batería `-p`: `enlaces` → `css` → atrás (`Enlaces`) →
  adelante (`CSS`).
- HALLAZGO: `nv_navegar(f)` recibía `f` aliasando
  `NV_enl[i].url`, que la propia carga reconstruye (el primer
  enlace de `/enlaces` es `/pagina`: el dedup tragaba el push y
  `NV_url` quedaba corrupto). Se copia a slot estable al entrar.
  Sin este fix, seguir enlaces era frágil según la página.

## 3. Estado de red

- `NV_ult_cod/tam` por carga (URL: código+bytes; fichero: bytes) +
  ms con `u_ticks()*10`; barra inferior: título a la izquierda,
  `"200 110B 20ms"` / `"ERR 404"` / `"ERR red"` a la derecha.

## 4. Depuración GUI (notas para próximos hitos)

- `u_print` dentro de `nv_dibuja` tras `ug_comienza` rompe el
  protocolo del dorso (Hito 3, confirmado de nuevo: las trazas
  temporales iban antes o fuera).
- El primer `DRAW` a veces no se compone: la URL inicial se carga
  en el primer `DRAW` (`NV_ini`) con repintado explícito.

## 5. Resultado

- `navega -p`: 9 → 18 pruebas, 0 fallos (QEMU + http_stub).
- Ventana verificada en QEMU (F1 → terminal → `navega URL`):
  foco Tab, seguir con Intro, atrás con Retroceso, `<` activo,
  barra sincronizada (capturas `h4_*`).
- FULL: 713 + 9 = 722 pruebas, 0 fallos.
- Tope `navega` 1245184: plano 1202964 + bss 20404 (holgura ~43K).
- Pendiente (optativo): PNG/JPEG vía nsbmp/nsgif, `style=` en línea.
