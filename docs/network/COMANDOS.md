# Comandos de red (castellano; F1: `red`, `mostrar_interfaz`)

- `red` -> una linea de estado (`Red: adaptador activo.` o
  `Red: sin adaptador.`). `red estado` -> detalle completo.
- `mostrar_interfaz` -> adaptador, estado, MAC, MTU, contadores
  Tx/Rx y errores. Sin adaptador: `Red no disponible: sin
  adaptador.` + `Arranque sin red (conecta una RTL8139).`
- Resto del encargo (`configurar_ip`, `configurar_dhcp`,
  `comprovar_conexion`, `mostrar_rutas`, `trazar_ruta`,
  `consultar_dns`, `mostrar_conexiones`, `solicitar_web`) en su
  fase; `comprovar_conexion` se conserva literal.
- Ayuda general actualizada (registro `CMDS`).
- `t_red` (app de prueba, no comando): via negativa sin
  adaptador + ARP Tx/Rx reales con adaptador.
