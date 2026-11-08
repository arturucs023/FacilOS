# F7-C — FacilBrowser en Ring 3

Rama `v2.3-red`. Navegador gráfico propio en anillo 3, sin motores
externos. Sin cambios de ABI ni syscalls nuevas; sin cambios en
TCP/IP del núcleo; sin HTTPS/TLS; sin sockets POSIX; cargas
secuenciales; API en español; código modular.

## 1. Qué se entrega

- `v2.0/user/lib/ulib.{h,c}`: `u_html_*` (parser + layout) con el
  patrón `u_http_*`/`u_dns_*`: `u_html_crea/libera/parse`,
  `u_html_resuelve_url`, `u_html_maqueta`. Límites fijos:
  texto 49152, seg 1024, bloq 512, enl 128, img 64, pila 16,
  filas 2048. Permisivo con contenido, estricto con topes
  (retorno 1 + `truncado`, nunca silencioso).
- `v2.0/user/apps/fbrowser.c`: ventana UGFX (dirección, atrás,
  adelante, recargar, detener, historial, estado, 301/302 ≤3
  saltos manuales, solo `http://`). Una conexión por página
  (`u_http_trae`), sin hilos.
- `v2.0/user/apps/t_fb.c`: batería 94 unitarias + 6 vivas
  (100 con red). Corrección incluida: `t` no se reiniciaba al
  llenar `grande` y `u_strlen` veía 0 (siempre fallaba
  `muchos-bloques-tope`); se pone `t = 0` antes del bucle.
- `v2.0/user/apps/terminal.c`: comando `fbrowser [url]`.
- `v2.0/tools/http_stub.py`: rutas `todas text/html`:
  `/pagina`, `/enlaces`, `/lista`, `/entidades`,
  `/malformada`, `/img-pagina`, `/busca`, `/grande-html` (70 KB).
- `v2.0/user/apps/servidor.c` + `v2.0/user/lib/tema.h`:
  entrada `fbrowser` en el menú (8 items), icono propio 43
  (globo azul) y tarea con icono (título `FacilBrowser`).
- `docs/network/COMANDOS.md`: comando `fbrowser`.
- ISO `FacilOS-v2.3-red-f7c.iso` (46 programas).

## 2. Arranque: corte 1121 → 993 (causa del fallo de `t1_lib`)

Síntoma: `t1_lib` moría al primer `u_dormir` (`#PF` lectura
supervisor, `eip=0x2023c8`, `cr2=0x11`). La RAM baja ya venía
mal al arrancar (`BOOT-BAJA9180` con basura, `BOOT-ALTA0` bien);
el proceso solo copiaba la basura. La corrupción empezaba en
el fichero+9180 (cola de LBA 1117) y seguía hasta `0xA0000`.

Causa: la pila de arranque (64 KB en `[0x90000,0xA0000)`,
`entry32.asm:esp=0xA0000`) más EBDA/VGA pisan la copia baja,
que llegaba hasta `0xA0000`. Al crecer la ISO (F7-C), el
catálogo alcanzó esa zona; antes no llegaba y no se veía.
Coherente con: `S17` parcial bien + cola mal, `ALTA0` bien,
basura tipo pila/núcleo, `SLAST` con patrón de pila.

Arreglo (infraestructura de carga, no red ni ABI):
baja hasta `0x90000`, pila en el hueco `[0x90000,0xA0000)`:

- `boot32.asm`: `bx 1121→993`, comentarios (768 =
  `(0x90000-0x30000)/512`, cola `[993,fin)`).
- `tools/copyalta.asm`: `CORTE_LBA 1121→993`.
- `src/kernel32/cd.c`: `CD_CORTE_LBA 1121→993`.
- `tools/build32.py`: `_cola (_fin_img-993)*512`.

Con fin 2277, cola 1284 sectores (657 KB < 1 MB). `t1_lib`
(LBA 1100) queda entero en alta, a salvo.

## 3. Pruebas (ISO limpia, sin depurado)

- `t1_lib`: 258 pruebas 0 fallos (antes: `#PF` determinista).
- `t_red`: 97/0 (sin red y KAT).
- `tutf8`: 26/0. `mem32`: 15/0.
- `t_fb`: 94/0 sin red; con `dhcp` + stub: 100/0
  (`viva-pagina-200/enlaces/titulo`, `malformada-dibuja`,
  `grande-tope`), stub recibe `/pagina`, `/malformada`,
  `/grande-html`.
- Catálogo: 46 programas, `F32X`, LBAs 229..2049.
- Gráfico: menú con `fbrowser` e icono; `lanza fbrowser
  pid=2 / reg id=0` sin excepción; ventana `FacilBrowser`
  con cromo, botones, barra e icono en tarea
  (capturas `shot_menu`, `shot_fbrowser` 800x600).

## 4. Archivos

Nuevos: `fbrowser.c`, `t_fb.c`, este informe.
Modificados: `ulib.{h,c}`, `terminal.c`, `servidor.c`,
`tema.h`, `http_stub.py`, `build32.py`, `boot32.asm`,
`copyalta.asm`, `cd.c`, `COMANDOS.md`.
Revertido todo el depurado (`irq.c`, `k32main.c`, sin
`t_probe`). `main` en `0b0532a`, `stash@{0}` intacto.
