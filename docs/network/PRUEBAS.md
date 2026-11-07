# Pruebas (QEMU; F5 en `FacilOS-v2.3-red-f5.iso`, F1 en `...-f1.iso`, F3 en `...-f3.iso`, F4 en `...-f4.iso`)

Red virtual: `-netdev user,id=red0 -device rtl8139,netdev=red0,
mac=52:54:00:12:34:56` (puerta 10.0.2.2, DNS 10.0.2.3). Sin la
variable, arranque sin adaptador (comportamiento por defecto).

F1 (base intacta): `red`/`mostrar_interfaz` en ventana con/sin
adaptador; `t_red`: 7 pruebas 0 fallos (Tx+Rx ARP real contra
la puerta, contadores avanzan).

F2 (esta fase, todo en la ISO de entrega):
- KAT al arrancar: `red: KAT ip OK` (checksum B861 + 4 rechazos:
  cabecera corrupta, TTL 0, total<IHL, marco corto).
- `t_red`: 19 pruebas 0 fallos: F1 intactas + configurar
  (ip/mask/gw/dns + relectura coherente) + resolver puerta por
  nucleo (MAC 52:55:0A:00:02:02) + segunda consulta sin Tx nueva
  (cache, `tx_ok` quieto) + sonda IP Tx + tabla con conectada
  10.0.2.0/24 y defecto via 10.0.2.2 + contadores.
- `configurar_ip`/`mostrar_rutas`/`red estado` en ventana
  (captura): rutas derivadas correctas, anuncio emitido
  (`tx_ok=1` tras configurar), ARP en cache, TTL 64.
- Regresion v2.2 en la misma ISO: t1_lib 258/0, tutf8 26/0,
  mem32 15/0, `funcion.f32` OK (`Hola desde F32 Lang`).

No probado en F2 (externo real): LAN fisica e Internet; la red
`user` es NAT local (ver LIMITACIONES.md).

F3 (esta fase, todo en la ISO de entrega salvo f1 intacta):
- KAT al arrancar: `red: KAT ip OK` + `red: KAT icmp OK` (eco 40 B,
  corrupcion detectada, trama corta rechazada y contada).
- DHCP `configurar_dhcp`: oferta de 10.0.2.2, concedida
  10.0.2.15/24, puerta 10.0.2.2, DNS 10.0.2.3, concesion
  2152792320 s (captura en ventana). `renovar` repite la concesion;
  `liberar` envia release y deja `IPv4 sin configurar` (captura).
- `comprovar_conexion -c 2 10.0.2.2`: 2 de 2 respuestas, seq 1-2,
  TTL 255 (el de slirp), tiempo 0 ms (captura). Contra
  192.168.99.99: `sin respuesta (seq=1)`, 0 de 1 (captura).
- `red estado`: ICMP ok=2 tras el ping (errores=1 basal del KAT).
- `t_red`: 22 pruebas 0 fallos (F1+F2 intactas + eco-2-respuestas,
  eco-seq-ok, eco-timeout-limpio, ping-sin-adaptador).
- Sin adaptador: `t_red` 5/0, `dhcp: sin adaptador`, arranque sin
  red; `configurar_dhcp`/`comprovar_conexion` fallan limpio.
- Regresion v2.2 en la misma ISO: t1_lib 258/0, tutf8 26/0,
  mem32 15/0. `funcion.f32`: interprete y syscalls que usa intactos
  en F3 (sin cambios); pendiente de repaso dedicado.
- Correccion F2 detectada por F3: `red_arp_resolver` nunca
  reinspeccionaba la cache (siempre agotaba la espera); F2 colaba
  por acierto previo. Ahora reinspecciona tras cada `red_bombea`
  con reintento cada 0,5 s (espera total acotada) y `red_icmp_ping`
  propaga -3/-4/-5 en vez de colapsar a -1. Verificado: el ping en
  ventana fallaba (TER-0033) antes del arreglo y pasa despues.

F4 (esta fase, todo en la ISO de entrega salvo f1/f3 intactas):
- KAT al arrancar: `red: KAT udp OK` con y sin adaptador
  (checksum valido/cero/incorrecto, longitudes, FIFO, cola
  llena, desalojo con/sin pendientes, titularidad, efimero).
- UDP vivo en `t_red` (53 pruebas 0 fallos en total): efimero,
  idempotencia, envio y recepcion de consulta DNS real a
  10.0.2.3:53 con validacion (origen, puerto, QR, parseo A),
  desalojo de liga vacia, puerto sin receptor (-2), envio largo
  y sin puerto (-2). Sin adaptador: ligar/enviar/recibir -1.
- `consultar_dns example.com` y `google.com`: respuestas reales
  (IP, TTL y tiempo; p. ej. google 216.58.205.46 ttl 131 20 ms);
  NXDOMAIN con nombre inventado (rc=3); timeout contra
  10.0.2.2:53 sin servicio (rc=1, 3x3 s acotados); `mostrar_dns`
  y `red estado` con "UDP recibidos" (capturas en ventana).
- Integracion: `comprovar_conexion -c 2` a la IP resuelta de
  google: 2 de 2 respuestas (20/40 ms). Resolucion y eco externo
  verificados por separado; la conectividad externa depende de
  SLIRP (aqui responde, queda registrado, no se asume).
- F1+F2+F3 intactas tras centralizar el bombeo: `t_red` 53/0
  incluye ARP/IP/ICMP; DHCP obtener+renovar+liberar y eco a la
  puerta re-ejecutados en la ISO F4.
- Regresion v2.2 en la misma ISO: t1_lib 258/0, tutf8 26/0,
  mem32 15/0. `funcion.f32`: interprete y syscalls que usa
  identicos desde v2.2 (sin cambios F3/F4); repaso dedicado
  pendiente (no hay `.f32` en la ISO).
- Basales de contadores (KAT): ICMP errores=1, UDP recibidos=10
  nada mas arrancar; las pruebas miran deltas.

F5 (esta fase, todo en la ISO de entrega salvo f1/f3/f4 intactas):
- KAT al arrancar: `red: KAT tcp OK` con y sin adaptador
  (vectores malos, RST a puerto cerrado, handshake activo,
  datos, retransmision con backoff y agotamiento, RST con
  sticky -9, pasiva completa, tabla llena sin desalojo; captura
  de Tx en RAM, cero trafico al cable).
- TCP vivo en `t_red` (94 pruebas 0 fallos en total) contra par
  Python en el anfitrion (ver `tools/tcp_par.py`): apertura
  activa + eco + cierre ordenado, 2 conexiones simultaneas con
  datos entrelazados, FIN ajeno -> EOF (-12) y cierre, rechazo
  rapido en puerto cerrado (-7), negro acotado con avance de Tx,
  pasiva completa (escuchar/avisar/recibir/enviar/EOF/cerrar),
  aborto con RST, negativas de argumentos y estados.
- Capturas de cable (pcap + `tools/tcp_par.py`): SYN con checksum
  valido y 0 datos, SYN-ACK aceptados, retransmisiones SYN con
  backoff ante silencio, RST validado por ACK en apertura,
  FIN/ACK de cierre en ambos sentidos, RST a puerto cerrado.
- Sin adaptador: abrir/escuchar/enviar/recibir/cerrar/estado -1.
- F1-F4 intactas en la misma ISO: `t_red` incluye ARP/IP/ICMP
  (eco-2, eco-seq, eco-timeout), UDP y DNS reales (example.com,
  google.com resueltos en la prueba); DHCP obtener+renovar+
  liberar y eco a la puerta re-ejecutados.
- Regresion v2.2 en la misma ISO: t1_lib 258/0, tutf8 26/0,
  mem32 15/0. `funcion.f32`: interprete y syscalls que usa
  identicos desde v2.2 (sin cambios F3-F5); repaso dedicado
  pendiente (no hay `.f32` en la ISO).
- Flaky conocido (entorno, no regresion): `rx-arp-puerta` (ARP
  crudo de un solo disparo) puede fallar si slirp pierde la
  trama ("Slirp: Failed to send packet" en arranque con carga);
  la via del nucleo con reintentos (`arp-nucleo-resuelve`)
  pasa en la misma ejecucion.

F6-B P0 (misma ISO reconstruida con correcciones; sin cambio ABI):
- KAT tcp OK con/sin adaptador con 6 regresiones nuevas
  (D2/D3/D4/D5/D8/D11). Repro previa: KAT FALLO con los tests
  nuevos sobre codigo F5; tras el arreglo: KAT OK.
- DHCP D1: vector `00 00 00 3C`=60 s (`dhcp-tiempo-60-be`,
  `dhcp-tiempo-86400-be`) en `t_red` 34/0 sin adaptador; en vivo
  `concesion (s): 86400` (antes `2152792320` byte-swapped).
  58/59 ignoradas y documentadas (sin renovacion automatica).
- TCP vivo 98 pruebas 1 fallo (solo `rx-arp-puerta` flaky):
  pasiva completa verificada en cable (SYN/SYN-ACK, datos ambos
  sentidos, FIN limpio; pcap `f6blive.pcap`: SYN=154 SYNACK=8
  FIN=17 RST=65 DATA=8). Regresion v2.2 verde (258/26/15).

F6-C P1 (misma ISO reconstruida con correcciones; sin cambio ABI):
- Repro previa: KAT 13-21 nuevos sobre codigo F6-B -> KAT FALLO
  (dos de ellos, 15 y 20, exigieron corregir el propio test:
  assert con ip_rem/pto_rem en TCB recien abierto, y falta de
  higiene de la tabla best-effort; el codigo nunca estuvo roto
  en esos dos puntos). Tras el arreglo: KAT OK con/sin
  adaptador (F5+F6-B+13..21).
- KAT 13 D6: datos (+datos+FIN) en FIN_TX aceptados y con ACK;
  duplicado no duplica. KAT 14 D7: ventana llena anuncia 0, la
  sonda de 1 B responde ACK wnd=0, tras drenar reabre. KAT 15
  D9: libera sanea controles y cargas (memset) + reuso limpio.
  KAT 16 D10: RST pelado seq=ackn ante ACK; SYN sigue RST+ACK.
  KAT 17 D12: envia -8 con wnd 0/insuficiente, ok con wnd>=n;
  lado activo aprende ventana del SYN-ACK. KAT 18 D13: aceptar
  y recibir con espera 0 devuelven 0 en <=1 tick. KAT 19 D14:
  cerrar con datos sin ACK = RST + -7, sin FIN con hueco.
  KAT 20 D15: misma tupla -6 en gracia, distinta ok, SYN
  pasivo en gracia gota, tras caducar reuso ok. KAT 21: RTO
  avanza con solo `red_bombea` (reint=1).
- Vivo `t_red` 110 pruebas 1 fallo (solo `rx-arp-puerta` flaky):
  12 pruebas nuevas (poll aceptar/recibir con dt<=1 tick,
  misma tupla: abrir/eco/cerrar, -6 en gracia, reuso tras 7 s)
  mas todo F1-F6-B intacto (pasiva, dos simultaneas, EOF,
  cierres, abortos, RTO negro, DHCP 86400).
- pcap `f6clive.pcap`: SYN=165 SYNACK=11 FIN=21 RST=72 (14 sin
  ACK = RST pelado D10 en accion) DATA=10, pasiva 6101 completa
  (SYN-ACK del invitado + datos ambos sentidos + FIN limpio).
- DHCP: obtener + renovar + liberar verificados en vivo
  (`concesion (s): 86400`; renovar re-oferta; liberar limpia).
- Regresion v2.2 verde (t1_lib 258/0, tutf8 26/0, mem32 15/0) y
  `funcion.f32` sigue pendiente de ejecucion en vivo.

F6-D (misma ISO reconstruida; sin cambio ABI/syscalls):
- `t_red` sin red 52/0: parser (CNAME solo/nombre/compat/
  ajeno/mas-A, Z, basura final, puntero a cabecera, RDLEN,
  tipo desconocido, bucle CNAME) + cache (viva/borde/caduca/
  ttl-cero/wrap-viva/wrap-muerta/limpia).
- `t_red` vivo 152 pruebas 1 fallo (solo `rx-arp-puerta`
  flaky): 34 nuevas DNS con stub controlado (10.0.2.2:5353
  UDP+TCP, `tools/dns_stub.py`): directo+hit, alias-full,
  cadena (texto verificado), ciclo, 8 saltos ok, 9 saltos
  fallo, NXDOMAIN, NODATA, SERVFAIL, REFUSED, TC->TCP,
  multi, ttl0(+nocache), ttl-tope 86400, timeout, otro
  servidor, cache-servidor, 33 miss, llena (32), desalojo,
  conserva. F1-F6-C intacto (TCP poll/tupla en verde).
- `consultar_dns example.com|google.com` en vivo: IP, TTL,
  origen red, transporte UDP, tiempo; NXDOMAIN con nombre
  inventado. `dns: alias:`/`Cadena:`/`Origen:`/`Transporte:`
  en salida y `/dnssal.txt` (volcado VGA, no serie).
- pcap `f6dfinal.pcap`: 138 intercambios UDP con 5353 y
  10 TCP a 5353 (fallback incl. prefijo BE16); resto TCP
  como en F6-C.
- Regresion v2.2 verde (258/0, 26/0, 15/0).
- Nota: 2 fallos transitorios de `tcp-tupla-reuso` en dos
  pasadas con procesos huerfanos del anfitrion (binds en
  conflicto tras limpieza fallida) y slirp cargado; verde
  en pasada limpia con dt=701 y gracia -6 registradas.
