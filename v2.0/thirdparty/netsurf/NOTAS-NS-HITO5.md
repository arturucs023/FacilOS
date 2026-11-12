# Hito 5 NetSurf — Imágenes `<img>` y `style=` en línea

Rama `v2.4.3`, sin tocar `main`. FacilBrowser intacto.

## 1. Imágenes BMP/GIF

- Puente: `<img>` crea elemento DOM (antes se descartaba) con
  `src`/`alt` como atributos reales.
- `nv_imagen`: bloque + `nv_resuelve` + `nsfetch_get` (http/https;
  con fichero local solo hay base vacía → alt) + `nv_img_dec`.
  Sin `src` o si algo falla: texto `alt` (o nada si no hay).
- `nv_img_dec`: sniff `"BM"` → nsbmp (0xBBGGRR top-down → RGB),
  `"GIF8"` → nsgif `R8G8B8A8` (primer frame; se ignora animación)
  a RGB en heap (`u_malloc`), tope 150000 px/imagen, 8 imágenes
  por página. PNG/JPEG: sin decodificador (cae a `alt`,
  documentado; requeriría vendorear libpng/libjpeg).
- Layout: filas `estilo=3` con índice (`nv_fila_t.img`);
  pintado con ajuste a 600 px (vecino más próximo) vía `ug_pixel`.
- HALLAZGO: el buffer estático de `nsfetch` se reutiliza sin
  riesgo porque el parse hubbub ya terminó cuando el layout
  descarga imágenes (recorre corre tras `completed`).
- Batería `-p` (`/img-bmp`): 2 imágenes, 48x32 ambas, píxeles
  presentes. Ventana verificada: mosaicos BMP+GIF en color.

## 2. `style=` en línea

- Sin parser propio: el atributo `style` se guarda en el DOM y
  `nv_estilo_nodo` crea por elemento una hoja libcss con
  `params.inline_style=true` (existe `create_for_inline_style`
  para ello) y la pasa como `inline_style` a `css_select_style`,
  que la aplica sobre el estilo de la hoja `<style>`. Mismo
  subconjunto que Hito 3 (color/fondo). Hoja destruida tras usar.
- Batería `-p` (`/css`): `NV_ninline>=1` y la fila "Amarillo"
  con `fg=0xFFFF00`.

## 3. Resultado

- `navega -p`: 18 → 25 pruebas, 0 fallos (QEMU + http_stub).
- FULL: 722 + 7 = 729 pruebas, 0 fallos.
- Tope `navega` 1245184: plano 1224724 + bss 20404 (holgura ~20K;
  si crece, toca tope, no BSS: los píxeles van al heap).
