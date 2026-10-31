# v2.0 Etapa 1 — ENTREGA 8: seguridad, aislamiento y base gráfica

Estado: COMPLETADA (cubre Fase 12 y Fase 13 sin implementar escritorio).

## 1. Modelo de aislamiento (auditoría, confirmado por ejecución)

- Ring3 con CR3 propio por proceso; núcleo y ventana solo-supervisor;
  U/S por página (manda la PT); `pag_valida_usuario` en toda syscall con
  puntero (32/32 en `sucio32`); copias núcleo↔usuario por ventana.
- Un proceso defectuoso muere solo (A/B/C/D de la batería: #PF/div0/UD con
  rc=-1, resto intacto). LFB mapeado supervisor (escritura directa desde
  Ring3 imposible).
- v2.0 añade **CR0.WP=1**: las escrituras del núcleo a páginas de solo
  lectura también fallan (D9). Verificado con batería 8/8 + `sucio32` 32/32
  + `mem32`/`ipc32`/`equidad32`/`t1_lib` en el mismo binario.
- Límites: sin SMAP/SMAP en 386 (el núcleo valida antes de tocar memoria
  de usuario); framebuffer compartido entre gráficos (ver 3); DOS por
  agotamiento se responde con error (-104/-105/-2), no con pánico.

## 2. Base para el futuro entorno gráfico (diseño, no implementado)

- Propiedad del framebuffer: un dueño (`gfx_pid`), toma explícita y
  limpieza del dorso al cambiar (contrato actual, documentado; limitación
  conocida: dibujar antes de tomar propiedad se deniega — caso `t2_gfx`
  11/12 preexistente).
- Eventos de entrada desacoplados: el transporte será IPC por buzones
  (entrega 5); el servidor gráfico futuro publicará eventos de teclado/
  ratón como mensajes. Sin ventanas/gestor/escritorio en esta etapa.
- Memoria de app, temporizadores y procesos independientes: ya disponibles
  (VMA, ktimer, IPC, sbrk).
