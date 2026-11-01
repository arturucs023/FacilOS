# INFORME-EVENTOS-E5 (Fase F)

Estado: IMPLEMENTADO + PROBADO.

## Protocolo (texto ≤64 B por buzones IPC 34/35/40; sin ampliar IPC)
- C→S: `REG w h titulo`, `BYE`, `ACK`, `SYNC` (pide reframe), `INF`.
- S→C: `ID n`, `DRAW x y w h`, `FOCUS 0/1`, `KEY código`, `MOU x y bot`,
  `QUIT`, `INF w h`.
- Compatible atrás: `REG w h` sin título y `DRAW x y` siguen valiendo
  (`wdemo` intacto, sin recompilar su lógica).

## Reglas
- Cada DRAW exige un ACK (lease). El servidor nunca bloquea indefinido:
  muerte→libera, BLOQ→envía y sigue (reenvía cada frame), lento→2 s y
  marca `ack_pend` (congela su DRAW, cromo sigue). ACK tardío reanuda.
- FOCUS asíncrono (sin ACK) en cada cambio de foco.
- SYNC/INF/REG/BYE se atienden también dentro de `espera_ack` (sin esa
  cláusula, `INF` tras `REG` interbloqueaba con sysinfo: detectado y
  corregido en E6).
- Cierre asíncrono: QUIT + libera sin esperar (esperar colgaría con
  clientes bloqueados). El cliente sale al desatascarse.
- Cliente que muere sin BYE: barrido por `u_proc_stat==FIN` en cada frame.

## Pruebas
- Secuencias deterministas post-fix del harness: menú×3 apps, teclas a
  terminal/editor/explorador, MOU/clics/botones, resize, minimize.
- `gbye` (muerte sin BYE): barrido sin cuelgue; `ejecutar` devuelve rc=0.
- Colas: buzón 16 suficiente en la práctica; overflow documentado (DRAW
  perdido → SYNC/evento siguiente repara; sin pérdida silenciosa de
  eventos de ventana: REG/BYE/QUIT/KEY/MOU no se descartan en el servidor
  salvo ACK duplicado).
- Sin eventos inventados: releases de teclado no existen en el driver;
  no hay doble clic; botón derecho = foco (sin menú contextual v1).
