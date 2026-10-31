# v2.0 Etapa 1 — ENTREGA 5: IPC por buzones

Estado: COMPLETADA (cubre Fase 7).

## 1. Problema y causa

Sin comunicación entre procesos no hay base para servicios ni escritorio
futuro (Fase 7). Faltaba una primitiva pequeña, segura y ampliable.

## 2. Solución (diseño D5)

- `proc.h`: buzón FIFO por proceso (16 mensajes × 64 B + emisor + len),
  índices de anillo y flag `espera_msg` (~1,2 KB por proceso).
- `ipc.h/ipc.c` (nuevo): `sc_ipc_enviar` (34) nunca bloquea (lleno → -2);
  `sc_ipc_recibir` (35) bloquea si vacío (reutiliza `proc_bloquea_msg` nuevo)
  y devuelve len real + emisor (truncado documentado si max < len).
- Solo se copia entre memoria validada del llamante y buzón del núcleo;
  validación de pid/estado (FIN/inexistente → -1), punteros y tamaños.
- `proc_fusiona` descarta pendientes al terminar (sin referencias cruzadas).
- Tabla: [34]/[35] cableadas (incidencia del desarrollo: inicializadores
  duplicados dejaban la tabla a NULL; detectado por GDB —el breakpoint de
  `sc_ipc_enviar` jamás se disparó— y corregido).
- `user`: `sys_/u_ipc_enviar/recibir`; apps `ipc_hijo` (eco) e `ipc32`
  (50 ping-pong verificados + lleno + cierre + errores).

## 3. Pruebas (ejecución)

- `ipc32`: 111 comprobaciones, 0 fallos (`v20_ipc2.log`): 50 ecos con
  orden/contenido/emisor, 16 en cola + 17.º → -2, hijo termina con 50,
  enviar a finado/inexistente/puntero → -1.
- Regresión misma sesión/binario: batería 8/8, `sucio32` 32/32,
  `equidad32` 3/3 SUPERADA (`v20_bat4.log`). Sin PANIC ni FUGA.
