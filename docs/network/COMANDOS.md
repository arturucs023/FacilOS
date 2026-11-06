# Comandos de red (castellano; F1+F2 implementados)

- `red` -> una linea de estado. `red estado` -> detalle completo
  (adaptador, MAC, MTU, contadores, IP, mascara, puerta, DNS,
  TTL, entradas ARP en cache).
- `mostrar_interfaz` -> adaptador, estado, MAC y estadisticas.
- `configurar_ip [ip | mascara|puerta|dns <ip>]` -> fija y muestra
  la configuracion IPv4 manual. Fijar IP emite anuncio gratuito.
- `mostrar_rutas` -> tabla derivada (conectada + defecto).
- Sin adaptador: `red`/`mostrar_interfaz` informan
  `sin adaptador`; `configurar_ip`/`mostrar_rutas` funcionan igual
  (la configuracion no exige hardware).

Fases siguientes (reservados, no implementados):
`configurar_dhcp`, `comprovar_conexion`, `trazar_ruta`,
`consultar_dns`, `mostrar_conexiones`, `solicitar_web`.
`comprovar_conexion` se conserva literal.
