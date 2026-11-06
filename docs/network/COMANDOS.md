# Comandos de red (castellano; F1+F2+F3 implementados)

- `red` -> una linea de estado. `red estado` -> detalle completo
  (adaptador, MAC, MTU, contadores, IP, mascara, puerta, DNS,
  TTL, entradas ARP en cache, ICMP ok/errores).
- `mostrar_interfaz` -> adaptador, estado, MAC y estadisticas.
- `configurar_ip [ip | mascara|puerta|dns <ip>]` -> fija y muestra
  la configuracion IPv4 manual. Fijar IP emite anuncio gratuito.
- `mostrar_rutas` -> tabla derivada (conectada + defecto).
- `configurar_dhcp [renovar|liberar]` -> obtiene IP, mascara,
  puerta y DNS del servidor; renueva o libera (rc=0 con resumen,
  distinto de 0 si no hubo respuesta; sin adaptador avisa y no
  bloquea el arranque).
- `comprovar_conexion [-c N] <ip>` -> N ecos ICMP (1..10, 1 por
  defecto) con seq, TTL y tiempo; `sin respuesta` si agota la
  espera; requiere IP configurada.
- Sin adaptador: `red`/`mostrar_interfaz` informan
  `sin adaptador`; `configurar_ip`/`mostrar_rutas` funcionan igual
  (la configuracion no exige hardware); `configurar_dhcp` y
  `comprovar_conexion` fallan limpio (rc=1 / TER-0033).

Fases siguientes (reservados, no implementados):
`trazar_ruta`, `consultar_dns`, `mostrar_conexiones`,
`solicitar_web`. `comprovar_conexion` se conserva literal.
