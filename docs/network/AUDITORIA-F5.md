# Auditoría de transición F1–F4 y plan F5 (v2.3-red @ c46b174)

Fecha: F5. Estado previo verificado: rama `v2.3-red` en `c46b174`,
árbol limpio y sincronizado, `main` en `0b0532a` intacta.
Regresión base ejecutada antes de tocar código: `t_red` 24/0 sin
adaptador; con red DHCP OK + `t_red` 53/0 + `t1_lib` 258/0.

## A.1 Módulos auditados
- `drv_red`: PIO RTL8139, Tx 1 en vuelo (TSD0-3 + TOK), anillo
  Rx 16K, sondeo sin IRQ, Tx acotada con espera finita.
- `red.{h,c}`: cfg ip/mask/gw/dns, rutas derivadas, tap 4x1516
  (descarta la mas antigua, cuenta descartes), `red_bombea`.
- `red_arp`: cache 16 con caducidad 60 s, resolvedor con
  reinspeccion de cache + reintento 0,5 s (corregido en F3),
  anuncio gratuito al fijar IP.
- `red_ip`: construccion/validacion (ver/IHL/longitud/cksum/TTL),
  `red_ip_enviar` devuelve ncarga o -1..-5.
- `red_icmp`: eco req/rep, pendiente unica, espera acotada,
  propaga -3/-4/-5 (no colapsa a -1 desde F3).
- `red_udp` (F4): 4 ligas x cola 4 x 512 B por pid, demux por
  puerto, checksum con pseudo-cabecera (0 = sin checksum),
  desalojo documentado, KAT propio, `rsv[3]` = recibidos.
- `proc`: EST_* / `fusiona` recoge FIN (limpia IPC + UDP);
  `proc_bloquea_ticks` para esperas acotadas; IF=0 en syscall.
- `exec`: SC_N=64, usadas 52-60 y 63; libres 61-62; copia
  paginada por tramos de 64 B; `regs_actual` tras ceder.

## A.2 Memoria, buffers, DMA, colas, timeouts, concurrencia
- Nucleo .bin 87676/98304 (192 sectores): techo de ~10,6 KB para
  todo F5. BSS no engorda el .bin (empirico F4). DMA por
  identidad <4 GB; todos los buffers de red son estaticos <4 GB.
- Colas acotadas en todo (tap drop-oldest, UDP reject-new, ARP
  evict-oldest) con contadores; sin asignacion dinamica en RX.
- Timeouts en ticks de 100 Hz, todos acotados (<=10 s); ningun
  bloqueo indefinido; monolitico sin preempcion en syscall (IF=0).
- `red_bombea` drena el anillo compartido; el tap conserva copia
  para lectores crudos (F4 hizo `sc_54` via tap: mismo contrato).

## A.3 Ciclo de vida procesos/conexiones
Precedente `proc_fusiona` -> `ipc_limpia_proceso` /
`red_udp_limpia_proceso`: TCP sigue el mismo gancho
(`red_tcp_limpia_proceso`: RST si hay ruta+ARP en cache y nunca
bloquea; si no, libera en silencio). Cada TCB tiene dueno pid;
id = gen*TCP_N+idx (nunca se recicla en caliente).

## A.4 Syscalls y ABI
Solo 61 y 62 libres: F5 usa UNA (61 op-multiplexada, precedente
55/56), 62 queda reservada. Todos los structs de tamano fijo;
validacion de punteros y tamanos en cada op; errores -1..-12
documentados en `red_tcp.h` y `ulib.h`.

## A.5 Regresion previa
Sin adaptador `t_red` 24/0; con red DHCP + `t_red` 53/0 +
`t1_lib` 258/0 (ISO F4). Punto de comparacion para el cierre.

## A.6 Riesgos y mitigaciones aplicadas
- R1 desborde de sectores: codigo economico + KAT compacto;
  plan B 224 sectores (cabe: 0x10000+0x1C000 < 0x30000).
- R2 KAT con trafico al cable: modo captura de Tx en el KAT.
- R3 complejidad RTO: RTO fijo 500 ms x2 (tope 4 s), 4 reintentos,
  sin keepalive ni 2MSL (documentado).
- R4 opciones del par: se usa el offset, se ignoran (Tx<=536,
  Rx<=2048).
- R5 OOO/duplicados: se descartan, retransmision recupera.
- R6 monopolio: trabajo O(4 TCB) por bombeo; enviar/recibir
  acotados.
- R7 par de pruebas sin HTTP: listener Python en anfitrion
  (guest->10.0.2.2) + hostfwd para escucha pasiva.
- R8 RST al morir sin bloquear: solo con ARP en cache.
- Hallazgo real durante F5 (corregido y verificado): la entrada
  usaba la longitud del marco (con relleno Ethernet) en la
  pseudo-cabecera y rompia el checksum de todo segmento <60 B;
  se usa longitud IP real. Detectado por captura, no por KAT
  (el KAT es autoconsistente y no lo veia).
