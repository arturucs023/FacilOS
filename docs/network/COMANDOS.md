# Comandos de red (castellano; F1+F2+F3+F4+F7-B+F7-C implementados)

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
  dominio, IP, TTL, origen (cache/red), transporte (UDP/TCP),
  tiempo y cadena CNAME si la hay; guarda el detalle en
  `/dnssal.txt` (servidor, tipo, origen, transporte, cadena).
  NXDOMAIN, error del servidor o timeout con mensaje propio
  (rc 1/3/4).
- `mostrar_dns` -> servidor DNS configurado y resumen de la
  cache F6-D (32 entradas por proceso, TTL tope 1 dia,
  CNAME hasta 8 saltos, TCP si TC=1).
- `http <url>` -> trae una pagina por HTTP/1.0 (solo GET,
  `http://dominio[:puerto][/ruta]` o IPv4 literal) y muestra
  IP, puerto, version, codigo, motivo, cabeceras clave,
  tamano y cuerpo (binario: aviso sin volcar) y tiempo;
  guarda el detalle en `/httpsal.txt` (URL, IP, estado,
  tamano, tiempo, cuerpo). Sin redirecciones automaticas.
  rc 0 ok; 1 sin respuesta; 2 URL invalida; 3 NXDOMAIN;
  4 respuesta no valida o no soportada.
- `fbrowser [url]` -> abre el navegador grafico FacilBrowser
  (Ring 3, HTTP/1.0 sin TLS, solo `http://`). Historial
  atras/adelante, recarga, detencion, barra de direccion,
  enlaces navegables, imagenes como marcador, maqueta por
  filas y estado con codigo/tamano/tiempo. Sin redireccion
  automatica (301/302 hasta 3 saltos manuales con aviso).
  Requiere escritorio (servidor); la URL inicial es opcional.
  Ejemplo: `fbrowser http://10.0.2.2:18080/pagina`.
  Buscador propio: si existe `/fbrowser.cfg` con una base
  `http://.../...?q=`, escribir una palabra (p. ej. `hola`)
  la busca ahi; el stub trae `/busca?q=hola` con 2 enlaces
  (`/pagina`, `/lista`). Gmail queda fuera: exige HTTPS,
  JS, POST y cookies (no soportados por diseno).
- Sin adaptador: `red`/`mostrar_interfaz` informan
  `sin adaptador`; `configurar_ip`/`mostrar_rutas` funcionan igual
  (la configuracion no exige hardware); `configurar_dhcp`,
  `comprovar_conexion` y `consultar_dns` fallan limpio.

Fases siguientes (reservados, no implementados):
`trazar_ruta`, `mostrar_conexiones`, `solicitar_web`.
`comprovar_conexion` se conserva literal.
