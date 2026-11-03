# DISENO-P0-v2.1 (previo, rama v2.1-p0-visual, base 69827d2)

## Auditoria real (verificada en codigo, no supuesta)
- Color: syscalls 10/11/12/13/14 reciben `int` via `sys_llama` pero el
  kernel trunca a `uint8_t` (`exec.c:539-582`) y `rgb()` solo conoce 16
  VGA (`gfx.c:11`). LFB real XRGB 32bpp. `u_pixel/u_rectangulo/u_texto/
  u_limpiar` (`ulib.c:467-485`) ya pasan `int` sin truncar.
- RGB crudo ya posible solo via sprite 20/21 (`g_sprite`, pixeles u32 de
  usuario). Sin syscall de tamano pantalla para Ring3: servidor fija
  800x600 (`servidor.c:15`); boot pide 800x600x32 (`boot32.asm:54`).
- Cursor 12x12 cuadrado (`servidor.c:219`); save/restore con `pal_rev`
  (cuantiza a 16 colores: con fondo RGB corromperia; hay que pasar a
  restore crudo).
- Orden: fondo (si bg_dirty) + ventanas por z + barra + menu + cursor;
  `cursor_borra` primero, `cursor_show` ultimo (`frame`, 564-661).
  bg_dirty=1 en mover/cerrar/minimizar/menu/muerte (lineas 473-919).
- Arranque: `k32main.c:96` crea "menu" grafico; tarea0 espera
  `proc_hay_graficos()` y luego da shell. Hijos via `kern_ejecutar`
  nacen con grafico=0 (`exec.c:265`); `gfx_toma` marca grafico=1 al
  primer uso (`exec.c:300`).
- Apps desktop usan `ug_limpia(v, C_NEGRO)` + texto paleta (terminal,
  explorador, editor, sysinfo, configura). UGFX controles con C_*.
- ISO sale de `build32.py:284` (`FacilOS-v2.0-etapa4.iso`).

## Decisiones
1. RGB sin syscall nueva: bit 24 como marca (`0x01000000|rrggbb`).
   Paleta 0-15 intacta (compat apps/tests). Cambian solo `gfx.c`
   (`color32`), firmas `g_*` a `uint32_t` (`gfx.h`) y casts en
   `exec.c` sc_10-14. Consola (`glifo/gfx_putc`) intacta uint8.
2. `user/lib/tema.h` nuevo: `RGB24()`, colores semanticos, iconos
   (mapas 16x16 de letras + paleta), constantes panel/cromo/sombra.
   Todo Ring3; kernel no conoce el tema.
3. Servidor: fondo degradado por bandas (solo bg_dirty), cursor flecha
   12x16 con mascara + restore crudo, iconos en menu/panel/tasklist,
   panel 30px, cromo tematizado, sombras 3px en pase cromo.
4. CROMO_H=20 intacto (contrato UGFX/cliente). BARRA_H 24->30 (solo
   servidor; geometria de ventanas la asigna el servidor).
5. Autoarranque: `k32main` crea "servidor" en vez de "menu"; fallback
   a shell intacto; shell/menu siguen como recuperacion.
6. ISO nueva `FacilOS-v2.1-p0.iso` (etapa4 intacta). Sin backbuffer,
   sin vsync, sin resoluciones nuevas, sin workspaces (P1-P3).
7. Apps: solo cambiar fondo a color tema (1 linea cada una); texto y
   controles iguales.

## Archivos afectados
`src/kernel32/gfx.{c,h}`, `src/kernel32/exec.c` (casts),
`src/kernel32/k32main.c` (autoboot), `user/lib/tema.h` (nuevo),
`user/apps/servidor.c` (visual), 5 apps (fondo), `tools/build32.py`
(nombre ISO), `build/reports/INFORME-P0-RENOVACION-VISUAL-v2.1.md`.
