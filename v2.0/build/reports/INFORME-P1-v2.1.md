# INFORME-P1-v2.1 (reloj en vivo, antiparpadeo, maximizar, fondo, resolucion)

Rama `v2.1-p1-tiempo`, base P0 (`3a0fe10`). Todo verificado en QEMU.

## 1. Reloj parado -> TICK (IMPLEMENTADO Y PROBADO)
Causa: el servidor solo repintaba por eventos; sysinfo/configura (hora
CMOS) y el reloj del panel se congelaban hasta el proximo clic.
Solucion sin syscalls ni ABI nueva: mensaje IPC `TICK s` (1-60 s, 0
cancela; en bucle ppal y espera_ack). El reposo revisa vencimientos
cada 20 ms y manda DRAW de contenido. `ug_tick()` en UGFX; sysinfo
TICK 1 (muestra segundos: verificado por hash distinto a 12 s sin
clics), configura TICK 5. Panel se refresca con los mismos frames.

## 2. Parpadeos -> MOU sin DRAW (IMPLEMENTADO Y PROBADO)
Causa: cada movimiento sobre ventana forzaba DRAW (~7/s, reframe
completo del cliente). Ahora MOU se reenvia siempre (estado) pero DRAW
solo si cambia ventana/botones o hay boton pulsado; clic/drag intactos
(terminal ignora raton; editor solo flancos: verificado codigo).
Pruebas: hover+teclear, drag con sombra, minimizar/restaurar/cerrar sin
restos, todo pixel-verificado.

## 3. DRAW resiliente (IMPLEMENTADO Y PROBADO)
El flag se limpiaba antes de enviar: con buzon lleno de MOUs el DRAW se
perdia y la ventana quedaba en blanco. Ahora solo se limpia si
`u_ipc_enviar` devuelve 0 + el reposo reintenta flags pendientes.
(caso que lo motivo resulto ser zona de medida fija; el fix queda como
proteccion real).

## 4. Maximizar/restaurar + resize (IMPLEMENTADO Y PROBADO)
Boton cuadrado tipico (w-60, icono ☐/solapado), guarda/restaura
geometria, ocupa (0,0,W,AREA_H); arrastrar maximizada restaura y mueve.
Resize por esquina ya existia: se le puso agarre visible (tras el ACK,
si no el DRAW lo tapa). DRAW lleva w/h y los clientes refluyen
(terminal escribe en maximizado: probado). Cierre sin restos.

## 5. Fondo configurable (IMPLEMENTADO Y PROBADO)
5 degradados (marino/grafito/bosque/burdeos/medianoche). IPC `FONDO n`
+ persiste en `fondo.cfg` (el servidor lo carga al arrancar).
Boton en configura cicla y aplica al instante (repintado total
verificado: grafito en VRAM).

## 6. Resolucion persistente (IMPLEMENTADO Y PROBADO)
Cambio en caliente imposible (sin VM86: verificado, solo int 0x10 en
boot). Cadena real en frio:
- syscall 45 `video_cmos` (leer/escribir byte CMOS 0x40; ABI 0-44
  intacta; justificacion: unico NVRAM visible al arranque con ISO de
  solo lectura).
- bootloader: lee CMOS, tabla {0x143,0x142,0x144} (800x600, 640x480,
  1024x768 x32 LFB; modos VBE estandar 0x112/0x118 no existen en este
  video: probado), valida (LFB+32bpp) y aplica; 512 B exactos.
- kernel: si cae a texto con pref != 0 lo restablece + avisa
  (autocuracion; nadie queda atrapado).
- servidor: dims por args del nucleo (panel/reloj/tasklist/ventanas/
  sombras dinamicos); maximizado usa el area real.
- configura: lista real de 3 modos + [actual]/[elegido], guarda en
  CMOS, "aplica al reiniciar"; muestra el modo real (INF).
Verificado: click 640x480 -> reset -> 640x480 panel+desktop; click
1024x768 -> reset -> 1024x768 + terminal centrada con contenido.
Limite honesto: menu texto modal dibuja a 800 fijos (recorte del
kernel lo contiene; sin cuelgues).

## 7. Memoria/ABI
Servidor ~19 KB (+TICK/FONDO/tabla); kernel +sc_45 (~60 B) + heal;
boot 512 B (margen 0: no tocar sin re-medir). Cero RAM dinamica.
ABI 0-44 intacta; 45 nueva justificada. Regresion: mem32 15/15, t1
258/258, ipc32 111, sucio32 46/46, equidad 3/3, disco32 38/38, t2_gfx
11/12 (banda1-azul preexistente, igual en etapa4).

## Archivos
`servidor.c` (TICK/FONDO/MOU/max/dims), `ugfx.{h,c}` (ug_tick),
`sysinfo.c`/`configura.c` (tick + UI fondo/resolucion), `sys.h`
(sys_video_cmos), `exec.c` (sc_45), `boot32.asm` (CMOS+tabla),
`k32main.c` (heal+dims), `tema.h` (nombres fondo).
ISO `FacilOS-v2.1-p0.iso` (mismo nombre, contenido P1).
