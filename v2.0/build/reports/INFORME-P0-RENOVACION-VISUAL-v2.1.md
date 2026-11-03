# INFORME-P0-RENOVACION-VISUAL-v2.1

## 1. Estado inicial
Escritorio E6 (rama v2.0-etapa4-desktop): fondo azul plano, panel 24px,
boton F32 texto, cursor cuadrado 12x12, cromo gris/rojo, paleta 16 VGA,
sin iconos/sombras/degradados. Arranque a menu texto + shell manual.

## 2. Commit de partida
`69827d2` (v2.0-etapa4-desktop). Baseline funcional: regresion E6 verde
salvo `t2_gfx` 11/12 preexistente y flakes `crear` bajo carga.

## 3. Rama utilizada
`v2.1-p0-visual` (nueva, desde 69827d2). main/v1.9/etapas intactas.

## 4. Auditoria de archivos (verificada en codigo)
Ver `DISENO-P0-v2.1.md`. Claves: syscalls gfx truncaban color a u8
(`exec.c` sc_10-14); wrappers Ring3 ya pasan `int` (`sys.h`, `ulib.c`);
LFB XRGB real; `g_sprite` ya aceptaba RGB crudo; sin syscall de tamano
pantalla (servidor fija 800x600); cursor con restore cuantizado
(`pal_rev`); arranque crea "menu" en `k32main.c:96`; ISO sale de
`build32.py:284`.

## 5. Arquitectura grafica existente
Sin cambios: apps Ring3 -> UGFX -> IPC 64B -> servidor Ring3 (leases
DRAW/ACK, dano, modal) -> LFB. Sin backbuffer, sin vsync.

## 6. Diseno visual ("marino", propio, inspirado en XFCE sin portar nada)
Azul marino oscuro + gris azulado + acento azul #3D7DD8 + texto claro.
Todo centralizado en `user/lib/tema.h`.

## 7. Color RGB (P0.1)
Bit 24 como marca (`0x01000000|rrggbb`); paleta 0-15 intacta.
Cambios: `gfx.c` (`color32`), firmas `g_*` a u32 (`gfx.h`), casts en
`exec.c` sc_10/11/12/13/14. Consola (`glifo/gfx_putc`) intacta.
Sin syscalls nuevas; ABI 0-44 intacta; apps/tests antiguos compatibles
(`t2_gfx` mismo resultado que etapa4, verificado).

## 8. Fondo procedural (P0.2)
16 bandas degradado (27,42,74)->(12,20,40) + banda/lines/medallones.
Solo con bg_dirty. Sin FPU, sin disco, sin backbuffer.

## 9. Cursor (P0.3)
Flecha 12x16 propia, contorno oscuro auto (vecindad), interior claro,
restore CRUDO (antes cuantizaba a 16 colores: romperia el degradado).
Clip en bordes verificado en 2 esquinas + movimientos del QA.

## 10. Iconos (P0.4)
12 iconos 16x16 propios (mapas de letras + `t_icon_rgb`), 3 KB
estaticos. Sin recursos externos, sin disco. En menu/panel/tasklist/
cromo. Botones X/_ con iconos 9/10.

## 11. Panel (P0.5)
30px (`T_BARRA_H`), boton FacilOS con icono, tasklist con icono+
indicador activa, separadores, reloj CMOS (syscall 44, sin cambios).
Hit-tests actualizados (boton 96px, entradas 132px).

## 12. Menu (P0.6)
Cabecera + 5 apps con icono + seleccion tematizada. Mecanismo F1/clic
intacto. Corregido: borde dibujado como relleno tapaba el menu.

## 13. Ventanas (P0.7)
Titulo activa/inactiva, iconos app/X/_, borde 1px, separador
titulo-contenido, interior T_VENT_FONDO. Zonas X/_/arrastre intactas.
CROMO_H=20 intacto (contrato UGFX).

## 14. Sombras (P0.8)
3px progresivos, recorte a AREA_H (nunca panel), en pase cromo,
borrado via bg_dirty. Sin alpha.

## 15. UGFX
Solo `#include tema.h` en `ugfx.h`. API intacta. Controles (`ug_lista`
negro, botones grises) siguen E5 a proposito (consistencia P1).

## 16. Servidor
`servidor.c` +272/-100 aprox: fondo/iconos/cursor/panel/menu/cromo/
sombra + hit-tests. Protocolo, leases, dano, modal, foco intactos.

## 17. Memoria consumida
servidor 10900->15508 B (+4.6 KB: iconos 3.3 KB + codigo; BSS +448 B
cursor). kernel32.bin 62164 B (+~1 KB). Heap usuario/BSS proceso muy
por debajo de limites (plano+bss << 0xC0000). Cero RAM dinamica nueva.

## 18-19. Pruebas y resultados exactos
Arranque directo (12+ runs): escritorio sin teclas, siempre.
- fondo/panel/menu/reloj/cursor/verificado por pixel (zone.py).
- 5 apps lanzan (terminal/explorador/sysinfo/editor/configura),
  REG+DRAW+ACK trazados, contenido pixel-verificado.
- drag (titulo+sombra siguen), minimizar/restaurar, cerrar sin restos
  (tact/cerrar/sombra = 0 tras cerrar), menu modal (`ejecutar menu`
  fullscreen limpio, vuelta con rc).
- cursor esquinas TL/BR sin artefactos ni excepciones.
- Regresion: mem32 15/15, t1_lib 258/258, ipc32 111/111, sucio32
  46/46, equidad32 3/3, disco32 38/38, estres_mem 12/12.
- t2_gfx 11/12 banda1-azul: IDENTICO en etapa4 (control corrido).
- estres_ipc p4-crear-eco FALLO 2/2 P0 y 1/1 etapa4 (mismo host):
  flake ambiental, sin delta P0.

## 20. Fallos encontrados
1. Borde menu como relleno (tapaba menu) -> 4 bordes finos.
2. Falsa alarma "ventanas en blanco": muestreo grueso + quirk
   display QEMU; DRAW/ACK trazados + VRAM (`xp`) + conteo fino
   probaron dibujo correcto.

## 21. Correcciones
Las dos de (20). Sin cambios de protocolo ni ABI.

## 22. Limitaciones pendientes
Sin maximizar/resize-borde/doble-clic/workspaces/iconos escritorio/
multi-res/backbuffer/vsync/transparencias (P1-P3). Controles UGFX con
estilo E5. Reloj HH:MM sin segundos. Quirk display QEMU persiste.

## 23-24. ISO y validacion QEMU
`build/iso/FacilOS-v2.1-p0.iso` (1517568 B, 741 sectores, 38 prog).
Etapa4 intacta. 15+ arranques QEMU: escritorio directo, 5 apps,
interaccion, shell tras ESC/ERROR (recuperacion intacta). Sin HW fisico.

## 25. Archivos modificados
`gfx.{c,h}`, `exec.c`, `k32main.c`, `build32.py`, `servidor.c`,
5 apps (1 linea fondo c/u), `ugfx.h`, nuevos `tema.h`,
`DISENO-P0-v2.1.md`, este informe, ISO nueva.

## 26. Commit final
(tras este informe) `feat(desktop): add FacilOS32 P0 visual redesign`.
