# INFORME-UGFX-E5 (Fase B/C)

Estado: IMPLEMENTADO + PROBADO (vía apps consumidoras y prueba dedicada).

## Archivos
- `user/lib/ugfx.h`, `user/lib/ugfx.c` (nuevo, Ring3, solo syscalls+IPC).
- `tools/build32.py`: compila `ugfx.o`, enlaza con `con_ugfx` (ver=3, 64K).
- Consumidores: `terminal`, `explorador`, `sysinfo`, `editor`, `configura`,
  `ugfx32` (ejemplo/prueba).

## Modelo
Sin acceso al framebuffer físico. El cliente dibuja su CONTENIDO (bajo el
cromo de 20 px, `UG_CHROME`) solo durante DRAW (lease tomado con
`ug_comienza`, confirmado con `ug_fin` = presentar + ACK). Sin hilos: un
bucle de eventos por proceso. Sin libc: solo `ulib.h`.

## Cobertura
Registro (REG+titulo, espera ID), eventos (DRAW/FOCUS/KEY/MOU/SALIR, INF,
SYNC), primitivas (píxel, línea Bresenham, rect, relleno, texto multilínea,
limpieza) con recorte al contenido, controles (ver INFORME-CONTROLES-E5).

## Pruebas
- Compilación ver=3 de 6 programas (bss dentro de límite).
- `ugfx32`: dibuja primitivas + botón + check + campo + lista; responde
  KEY/MOU/QUIT (ejemplo vivo, evidencia indirecta vía apps).
- Terminal/explorador/editor usan campo/lista/botón a diario en las
  pruebas E6 (ver INFORME-PRUEBAS-E5-E6).
- Incidencia metodológica: los píxeles de texto gris/verde son invisibles
  a escaneos que exigen blanco puro; verificar por color exacto.

## Límites
- Sin superficies propias (dibuja directo en lease; documentado).
- Sin doble buffer de cliente; sin VSync (ver COMPOSITOR).
- Títulos sin espacios en REG v1; DRAW sin tamaño lo ignora el cliente
  viejo (`wdemo` compatible).
