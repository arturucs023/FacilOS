# UDP (v2.3-red F4)

`src/kernel32/red_udp.{h,c}`. Transporte propio sin sockets:
datagramas IPv4 (proto 17) con demultiplexor por puerto destino
y colas por liga. Ring3 accede solo por syscalls 58-60; jamas
toca el hardware ni el anillo.

- Envio (`red_udp_enviar`): cabecera de 8 B {sport,dport,len},
  checksum con pseudo-cabecera IPv4 (origen+destino+0+17+longitud);
  si el calculo da 0 se envia `0xFFFF` (0 en IPv4 significa "sin
  checksum"). TTL 64 fijo. Devuelve la carga pedida o negativo
  (-1 sin adaptador, -2 args, -3 sin ruta, -4 ARP, -5 Tx).
- Entrada (`red_udp_entrada`, desde `red_ip_entrada` con proto 17):
  valida longitud (8 <= campo <= resto), checksum (campo 0 = sin
  checksum, se acepta; si no, la suma con pseudo-cabecera debe dar
  cero), puerto ligado y tamano encolable (carga util maxima
  512 B; lo mayor se descarta entero). Malformados, truncados,
  sin receptor y cola llena se cuentan (`c_err`) y se descartan.
- Ligas: 4 simultaneas, cola circular de 4 datagramas cada una
  (productor y consumidor por indices+contador: lo pendiente nunca
  se sobrescribe; llena rechaza lo nuevo). Cada liga tiene dueno
  pid: ligar el propio puerto es idempotente; otro proceso recibe
  -6; al terminar el proceso se liberan sus ligas (gancho en
  `proc_fusiona`, como `ipc_limpia_proceso`).
- Efimeros 49152..65535 sin colisiones (se comprueba la tabla).
  Tabla llena: desaloja la liga con cola VACIA mas antigua
  (preferencia al propio pid); con pendientes en todas, -6
  explicito (nunca invalida una recepcion pendiente).
- Recepcion (`red_udp_sacar`): FIFO, sin espera; la syscall 60
  anade espera acotada (bombeo + rodajas, tope 10 s). Copia
  {src:4,sport:2,len:2} + hasta max-8 datos (trunca lo que sobre,
  avisa con el total real en `len`).
- Contadores: recibidos en `rsv[3]` de la ficha (`red estado` lo
  muestra como "UDP recibidos"). Basal 10 al arrancar: el KAT
  encola 10 datagramas sinteticos (igual que ICMP deja errores=1).
- KAT al arrancar (`red: KAT udp OK`): checksum valido/cero/
  incorrecto, longitudes, orden FIFO, cola llena sin corrupcion,
  desalojo con/sin pendientes, titularidad por pid, efimero.
  Corre con y sin adaptador (tabla logica pura).

Syscalls (Fase C): 58 `red_udp_ligar`, 59 `red_udp_enviar`,
60 `red_udp_recibir`; 61-62 siguen reservadas (F5). 52-57 y 63
intactas. `sc_54` (trama cruda) mantiene su contrato pero desde
F4 bombea primero y sirve del deposito tap: los lectores crudos
ya no roban tramas a la pila (ARP/ICMP/UDP) ni entre si.

Limites F4: carga encolable 512 B (DNS cabe; general Tx hasta
1472 B); TTL fijo; sin broadcast dirigido (sale por puerta);
sin fragmentacion (como IPv4).
