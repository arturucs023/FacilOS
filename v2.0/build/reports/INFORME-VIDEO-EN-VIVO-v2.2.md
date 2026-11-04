# Video en vivo + 1920x1080 (v2.2)

## Objetivo
Cambiar la resolucion sin reiniciar, con 1920x1080 como modo nuevo,
y que nada se rompa (ni el escritorio ni la contabilidad de memoria).

## Diseno (todo codigo propio, sin BIOS tras el arranque)
- El adaptador se programa por puertos (indice 0x1CE, datos 0x1CF):
  apagar, geometria, virtual, 32 bpp, encender con marco lineal.
  Si no hay adaptador, la llamada falla y todo sigue igual.
- Syscalls nuevas: 47 `video_modo(w,h)` (0 ok, -1 sin soporte,
  -2 sin memoria) y 48 `video_info` ([W,H,paso,prof]).
- Memoria: el dorso clasico (2 MB en 0xA00000) no cabe para 1080p
  (8,3 MB). Al arrancar se busca por E820 el bloque contiguo alto
  de 9 MB, se reserva y se mapea por identidad; lo heredan todos
  los directorios. Sin sitio (poca RAM), el modo grande avisa
  "sin memoria" en vez de romper. La ventana del marco lineal
  crece a 12 MB.
- Persistencia: CMOS 0x40 admite 3 = 1920x1080 (`video_cmos` 0-3).
  El sector no cabe en 512 B para programar 1080p, asi que arranca
  en VESA 800x600 y el nucleo completa el cambio si el CMOS vale 3
  (sin adaptador, restablece a 0 y sigue en VESA).
- Servidor: mensaje nuevo `VIDEO`; relee el tamano del nucleo,
  reencaja ventanas (maximizadas incluidas), repinta todo.
- `configura`: cuatro modos, cambio inmediato ("aplicado",
  "no soportado", "sin memoria") + CMOS para persistir.
- Raton del nucleo con tope dinamico (antes fijo 799x599).

## Archivos
- Nucleo: `gfx.c/h` (BGA, `pantalla_modo/dime/arranque`), `pag.c/h`
  (dorso grande, LFB 12 MB), `exec.c` (47/48, CMOS 0-3), `ps2.c`
  (tope dinamico), `k32main.c` (CMOS>3), `cpu.h` (outw/inw).
- Usuario: `sys.h`, `ulib.h/c` (`u_video_modo/info`),
  `servidor.c` (VIDEO), `configura.c` (4 modos en vivo).
- Lanzador: `tools/arrancar.ps1` (zoom-to-fit + ventana encajada
  al area util de Windows).

## Pruebas (QEMU, capturas)
- 800 -> clic 1920x1080: escritorio 1920x1080, `modo=1920x1080`,
  `[actual]`, `aplicado`, barra propia visible.
- Vuelta a 800x600: centra igual, sin restos.
- Reset con CMOS=3: arranca directo en 1920x1080.
- Terminal tras los cambios: abre y dibuja bien.
- RAM: 23424 KB utiles (9 MB reservados), marcos 3360 libres.

## Limites conocidos (no regresiones)
- Abrir terminal con configura delante la deja detras: pasaba
  igual sin tocar resolucion (foco del menu, sin tocar).
- Demos de prueba (`t2_gfx`, `d_banco`, menu 800x600) dibujan
  arriba-izquierda a 1080p; recortan por nucleo, no fallan.
