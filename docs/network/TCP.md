# TCP (v2.3-red F5)

`src/kernel32/red_tcp.{h,c}`. TCP propio sin sockets (subconjunto
RFC 793/1122): apertura activa y pasiva, SYN/SYN-ACK/ACK, numeros
de secuencia y confirmacion de 32 bits, datos ordenados, checksum
con pseudo-cabecera, retransmision con RTO y backoff, ventana de
recepcion, cierre ordenado y abortado con RST, timeouts acotados.
Sin opciones, sin reensamblado (OOO se descarta y lo recupera la
retransmision), sin 2MSL, sin keepalive (todo documentado abajo).

- TCB: 4 conexiones simultaneas con dueno pid. Sin desalojo: tabla
  llena devuelve -6 (una conexion guarda estado y datos; desalojar
  seria perderlos, a diferencia de UDP). Al terminar el proceso se
  liberan las suyas (gancho en `proc_fusiona`): RST si hay ruta y
  ARP en cache (sin bloquear), si no silencio.
- Tx stop-and-wait: un solo segmento sin ACK a la vez (536 B max
  por envio). Suficiente y correcto en LAN; el receptor no puede
  distinguirlo de un par lento.
- Rx: cola circular de 2048 B por conexion; acuse inmediato;
  duplicados confirman sin avanzar; fuera de orden se descarta.
- RTO fijo 500 ms con backoff x2 (tope 4 s), 4 reintentos (5 envios
  en total); SYN, SYN-ACK, datos y FIN comparten el mecanismo.
  Conectar y cerrar acotan su espera (~10 s y ~5 s).
- Entrada: valida longitud (usa la longitud IP real, no la del
  marco: el relleno Ethernet a 60 B falseaba el checksum de los
  SYN puros), offset, checksum y demultiplexa por tupla o escucha.
  RST en apertura se valida por ACK (rechazo rapido y fiable);
  establecida, por ventana. Puerto cerrado responde RST+ACK.
- Cierre: FIN propio con retransmision; al ver el FIN ajeno se
  contesta; con ambos vistos se envia el ACK final y se libera.
  Sin el FIN ajeno, tras ~5 s se libera igual (0 si el nuestro
  fue confirmado, -7 si no). Abort envia RST y libera al instante.
  EOF para la app: recibir devuelve -12 con todo drenado.
- Errores: -1 sin adaptador, -2 args, -3 sin ruta, -4 ARP, -5 Tx,
  -6 tabla llena/en uso, -7 timeout/conexion, -8 estado invalido,
  -9 abortada (RST, sticky hasta cerrar), -12 fin remoto.
- KAT al arrancar (`red: KAT tcp OK`, con y sin adaptador):
  vectores malos, RST a puerto cerrado, handshake activo, datos,
  retransmision con backoff y agotamiento (-7), RST con sticky
  -9, pasiva completa (escucha/SYN/acepta), tabla llena sin
  desalojo. Captura la Tx en RAM: cero trafico al cable.

Syscall unica 61 `red_tcp` op-multiplexada (op 0 abrir, 1
escuchar, 2 aceptar, 3 enviar, 4 recibir, 5 cerrar, 6 abortar,
7 estado); la 62 queda reservada. Ring3: `u_tcp_*` en `ulib`.

Demostrado con QEMU user-net + par Python en el anfitrion:
apertura activa, eco en ambas direcciones, 2 conexiones
simultaneas, EOF y cierre ordenado, rechazo rapido, RTO con
retransmisiones visibles en el cable, pasiva completa con datos
en ambos sentidos. Ver PRUEBAS.md.

Limites F5: MSS 536 de envio (se ignora el MSS del par), recepcion
hasta 2048 B por segmento, 4 conexiones, sin persistencia entre
arranques, sin medio-cierre API (el par puede medio-cerrar; se
drena y se devuelve EOF), ISN por ticks+pid (no aleatorio
criptografico).
