# TCP (v2.3-red F5 + F6-B + F6-C)

`src/kernel32/red_tcp.{h,c}`. TCP propio sin sockets (subconjunto
RFC 793/1122): apertura activa y pasiva, SYN/SYN-ACK/ACK, numeros
de secuencia y confirmacion de 32 bits, datos ordenados, checksum
con pseudo-cabecera, retransmision con RTO y backoff, ventana de
recepcion, cierre ordenado y abortado con RST, timeouts acotados.
Sin opciones, sin reensamblado (OOO se descarta y lo recupera la
retransmision), sin keepalive (todo documentado abajo). En vez de
TIME_WAIT clasico: cuarentena de tuplas (ver abajo).

## Maquina de estados

`CERRADO(0) -> SYN_TX(2)/ESCUCHA(1) -> SYN_RX(3) -> ESTABLECIDO(4)
-> FIN_TX(5)/FIN_RX(6) -> CERRADO`. Sin FIN_WAIT_1/2, CLOSE_WAIT,
LAST_ACK, CLOSING ni TIME_WAIT: el cierre ordenado usa FIN_TX
(FIN nuestro, espera `fin_ack && fin_rx`) y FIN_RX (FIN ajeno,
falta `cerrar()`); al completarse se envia el ACK final y la
tupla pasa a cuarentena (no queda TCB ocupado).

Transiciones: `CERRADO->SYN_TX` (abrir), `CERRADO->ESCUCHA`
(escuchar), `ESCUCHA->SYN_RX` (hijo al llegar SYN pelado),
`SYN_TX->ESTABLECIDO` (SYN-ACK con `ack==iss+1`),
`SYN_TX->SYN_RX` (apertura simultanea), `SYN_RX->ESTABLECIDO`
(`ack==snd_nxt`), `ESTABLECIDO->FIN_RX` (FIN en orden),
`ESTABLECIDO/FIN_RX->FIN_TX` (cerrar), `*->CERRADO` por RST
valido, RTO agotado, abortar o limpieza de proceso.

## Semantica de la API (syscall 61, ops 0..7; `u_tcp_*`)

- Abrir (bloquea ~10 s): id estable (oid); si el slot se
  reutiliza devuelve -9, nunca un id ajeno.
- Escuchar (no bloquea): backlog de 1 hijo pendiente; SYN extra
  con pendiente ocupado = gota silenciosa (F6-B).
- Aceptar/recibir (espera acotada 0..10 s): `espera_ms=0` es
  consulta real sin bloqueo (un bombeo; sin dormir). Devuelven
  0 si agota la espera, -8 si el id no es escucha/conexion.
- Enviar (bloquea ~12 s hasta su ACK): respeta `wnd_par`
  (F6-C); con ventana 0 o insuficiente devuelve -8 de
  inmediato para que la app reintente (sin bloquear). Stop-and-
  wait: un segmento (<=536 B) sin ACK a la vez; `-8` si hay
  uno en vuelo. Sin parametro de espera por ABI (documentado).
- Cerrar (bloquea ~5 s): drena el envio previo (3 s); si
  quedan datos sin confirmar NO emite FIN con hueco: aborta
  con RST y devuelve -7 (F6-C D14). Exito = 0 con tupla en
  cuarentena (F6-C D15).
- Abortar (inmediato): RST + libera + cuarentena, devuelve 0.
- Estado: 1..6 o negativo. Recibir devuelve datos, 0 si
  agota, -12 EOF (FIN ajeno con todo drenado), -9 RST.

## Ventanas

- Rx: cola circular de 2048 B; acuse inmediato; duplicados
  confirman sin avanzar; OOO se descarta. Llena = anuncia 0
  y responde a la sonda de 1 byte con ACK wnd=0 sin consumir
  (F6-C D7 verificado en KAT).
- Tx: `wnd_par` del par se aprende tambien del SYN-ACK (F6-C)
  y se respeta en cada envio; -8 = ventana cerrada.

## Retransmisiones y temporizadores

RTO fijo 500 ms con backoff x2 (tope 4 s), 4 reintentos (5
envios); SYN/SYN-ACK/datos/FIN comparten el mecanismo. El
reloj avanza en cada bombeo (`red_bombea` llama a
`red_tcp_reloj`; F6-C), no solo con syscalls TCP bloqueadas.
Re-Tx de FIN repite `fseq = snd_nxt-1`; FIN duplicado
re-acusa sin avanzar.

## Cuarentena de tuplas (en vez de TIME_WAIT)

Tras cierre ordenado o aborto, la tupla queda 6 s en gracia
(tabla de 4, caducidad perezosa, 48 B): SYN nuevo o apertura
activa con la misma tupla se rechaza (-6 / gota silenciosa).
Un TIME_WAIT clasico ocuparia 25% de la tabla por cierre;
la cuarentena da la misma proteccion (sin reencarnaciones)
sin presion sobre los 4 TCB. Best-effort: la entrada mas
antigua se desaloja si hay mas de 4 cierres en 6 s.

## Entrada y validacion

Longitud IP real (no la del marco), offset, checksum,
demux por tupla o escucha. RST en apertura por ACK;
establecida por ventana. Puerto cerrado: RST pelado con
`seq = SEG.ACK` si el entrante trae ACK (F6-C D10), si no
RST+ACK con seq 0. ACK futuro = gota; ACK viejo se ignora
pero datos/FIN validos siguen (F6-B).

## Recursos y limpieza (F6-B + F6-C)

4 TCB con dueno pid, sin desalojo (-6 si llena). Al morir el
proceso se liberan las suyas (RST si ruta+ARP en cache, sin
bloquear) incluyendo CERRADO+sticky; `nuevo`/`libera` sanean
controles y cargas (memset). Half-close coherente: en FIN_TX
se sigue recibiendo hasta el FIN ajeno (F6-C D6); no se
transmite en FIN_TX.

## Errores

-1 sin adaptador, -2 args, -3 sin ruta, -4 ARP, -5 Tx,
-6 tabla llena/en uso (incluye tupla en gracia o ventana
ajena insuficiente: ver contexto), -7 timeout/conexion
(incluye cierre con datos sin confirmar), -8 estado
invalido para la operacion (incluye ventana cerrada o
poll `espera=0` sin objeto: aceptar/recibir devuelven 0,
enviar no tiene espera), -9 abortada (RST, sticky hasta
cerrar), -12 fin remoto.

## KAT

`red: KAT tcp OK` con y sin adaptador: F5 (vectores malos,
RST, handshake, datos, RTO, sticky, pasiva, tabla llena) +
F6-B (D2/D3/D4/D5/D8/D11) + F6-C 13..21 (D6 datos en FIN_TX,
D7 ventana 0 y sonda, D9 saneado, D10 RST pelado, D12 wnd,
D13 poll<=1 tick, D14 sin FIN con hueco, D15 cuarentena y
reuso, reloj en bombea). Captura Tx en RAM.

Syscall unica 61 op-multiplexada; la 62 queda reservada.
Ring3: `u_tcp_*` en `ulib` (F6-D: el fallback DNS-sobre-TCP
abre una conexion por consulta truncada y la aborta tras la
respuesta; sin TCB colgados). Sin cambios ABI en F6-C.

Demostrado con QEMU user-net + par Python: lo de F5 mas
poll-0 inmediato, cuarentena (-6 y reuso tras 7 s),
pasiva completa y cierre ordenado/abortado. Ver PRUEBAS.md.

Limites: MSS 536 (se ignora el MSS del par), Rx 2048,
4 conexiones, backlog 1, gracia 6 s best-effort, sin
persistencia entre arranques, sin medio-cierre API (el par
puede medio-cerrar; se drena y EOF), ISN ticks+pid (no
criptografico), sin reensamblado ni ventana deslizante
(OOO se descarta; documentado, no previsto).
