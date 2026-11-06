# Comandos de red (castellano; F1+F2+F3+F4 implementados)

- `red` -> una linea de estado. `red estado` -> detalle completo
  (adaptador, MAC, MTU, contadores, IP, mascara, puerta, DNS,
  TTL, entradas ARP en cache, ICMP ok/errores, UDP recibidos).
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
- `consultar_dns <dominio>` -> resuelve a IPv4 (tipo A) y muestra
  dominio, servidor, IP, tipo, TTL y tiempo; NXDOMAIN, error del
  servidor o timeout con mensaje propio (rc 1/3/4).
- `mostrar_dns` -> servidor DNS configurado (DHCP o manual) y
  aviso de ausencia de cache en F4.
- Sin adaptador: `red`/`mostrar_interfaz` informan
  `sin adaptador`; `configurar_ip`/`mostrar_rutas` funcionan igual
  (la configuracion no exige hardware); `configurar_dhcp`,
  `comprovar_conexion` y `consultar_dns` fallan limpio.

Fases siguientes (reservados, no implementados):
`trazar_ruta`, `mostrar_conexiones`, `solicitar_web`.
`comprovar_conexion` se conserva literal.
