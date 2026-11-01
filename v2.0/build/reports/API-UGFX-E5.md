# API-UGFX-E5 (referencia, v1)

Prefijo `ug_`. Cabecera `user/lib/ugfx.h`. Todo freestanding.

## Constantes
- `UG_CHROME 20`: alto del cromo del servidor (el contenido empieza en y+20).
- `UG_FW 8`, `UG_FH 16`: celda de texto (fuente BIOS 8x16 del kernel).
- Eventos: `UG_EV_NADA 0`, `DRAW 1`, `FOCO 2`, `TECLA 3`, `RATON 4`, `SALIR 5`.

## Tipos
- `ug_ev_t {tipo, x,y,w,h, foco, tecla, rx,ry,rbot}`: DRAW trae geometría;
  RATON trae coords de pantalla + botones; TECLA trae código (ASCII, 8, 13,
  9, 27, 0x101-0x105).
- `ug_win_t {srv, id, w,h, x,y, foco, vivo, ox,oy}`: `ox,oy` válidos solo
  entre `ug_comienza` y `ug_fin`.

## Conexión
- `ug_abre(v, srv, w, h, titulo)`: envía `REG w h titulo`, espera `ID`
  (bloquea). 0 ok; <0 error (sin servidor, sin ID, QUIT temprano).
- `ug_cierra(v)`: envía `BYE`, marca no viva.
- `ug_evento(v, ev, bloquear)`: siguiente evento; con bloquear=0 vuelve
  NADA si el buzón está vacío. QUIT → envía BYE y devuelve SALIR.
- `ug_pide_info(v, &w, &h)`: envía `INF`, espera `INF w h`; confirma DRAWs
  interleados sin dibujar. <0 si QUIT en medio.
- `ug_sync(v)`: envía `SYNC` (pide reframe; p. ej. tras hijo de consola).

## Dibujo (coords relativas al contenido, con recorte)
- `ug_comienza` (toma lease) / `ug_fin` (presenta + ACK).
- `ug_pixel/linea/rect/relleno/texto/limpia`. `ug_texto` multilínea `\n`,
  filas de 16 px, tinta transparente sobre el fondo (el núcleo ya no
  pinta celdas negras: los botones se leen). Colores: paleta `C_*` (0-15).

## Controles
- `ug_dentro`, `ug_etiqueta`, `ug_boton(v,x,y,w,h,t,estado)` con estados
  NORMAL/HOVER/PULSADO/OFF, `ug_clic(antes,ahora)` (flanco),
  `ug_panel`, `ug_check`, `ug_campo` + `ug_campo_tecla` (insertar, borrar,
  izq/der; 1 si cambió), `ug_lista` + `ug_lista_tecla` (arriba/abajo con
  scroll) + `ug_lista_clic` (item bajo cursor o -1).

## Errores y ciclo de vida
Sin excepciones: códigos <0 en conexión/info; dibujar sin lease lo deniega
el kernel (-1 silencioso); el cliente muerto sin BYE lo barre el servidor.
Propiedad de memoria: toda del cliente (stack/heap sbrk); el servidor
nunca ve punteros del cliente (solo texto IPC ≤64 B).
