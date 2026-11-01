# INFORME-IPC-ETAPA2 (Fase 8)

## Alcance y resultado
`ipc32` 111/0 ×2 + `estres_ipc` 27/27: envío/recepción normal, 200 y 1000
idas con contenido verificado, multiemisor 3×30 a un eco, muerte del emisor
(mensajes previos íntegros, eco tolerante), orden FIFO en ráfaga de 16,
lleno hasta 16 + 17.º → -2, receptor que termina con pendientes (fusión los
descarta), 1000 idas en 3 ticks, balance exacto posterior.

## Casos verificados (Fase 8.1–12)
Normal, repetidos, vacío (bloqueo con despertar garantizado por protocolo),
límite 16/64 B y exceso controlado, emisor/receptor identificados (pid en
cada entrega), bloqueo/desbloqueo, terminación de participantes, recursos
tras terminar (info2 exacto), recuperación tras errores (-1/-2).
"Morir bloqueado" es imposible por diseño (un bloqueado no ejecuta):
documentado como limitación D, no como defecto.

## Defectos hallados (ver REGISTRO)
- Inicializadores duplicados dejaban `sc_tabla[34/35]` a NULL (puerta -1):
  detectado porque el breakpoint GDB jamás se disparó; corregido.
- Protocolo de test: ecos rancios + ráfaga sin ceder (deadlock de test):
  corregido en el test (drenaje + `u_ceder`); lección en calidad (Fase 14).
