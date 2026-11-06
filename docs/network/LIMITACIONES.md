# Limitaciones conocidas (F1+F2+F3)

- Red `user` de QEMU es NAT 10.0.2.0/24: sin entrada sin `hostfwd`,
  sin promiscuo externo; DHCP/DNS los da slirp (cliente DHCP
  implementado en F3 como app Ring3, sin UDP en el nucleo; el DNS
  solo se guarda, las consultas son F4). Pruebas contra puerta
  10.0.2.2; LAN real exigiria puenteo o tap (fuera de F3).
- ICMP solo eco (sin unreachable/time-exceeded ni reenvio; F6+).
  El contador `ICMP errores` parte de 1 basal (el KAT cuenta su
  trama corta de prueba a proposito).
- Sondeo, no IRQ (F2): la recepcion se recoge al pedirla; latencia
  de sondeo, sin perdida (el anillo retiene; el deposito crudo
  guarda lo no consumido por la pila).
- Tx: 1 en vuelo como maximo, descriptores en round-robin
  (TSD0..3); Rx: anillo 16K; MTU 1500; sin reensamblado.
- Sin adaptador todo devuelve error limpio; nada cuelga.
- Lecciones de implementacion: todo PIO >255 exige `volatile`
  (GCC saca lecturas de los bucles); espera por iteraciones en
  syscall (ticks congelados con IF=0); TSD de Tx se rearma por
  descriptor; el resolvedor ARP debe reinspeccionar la cache tras
  cada bombeo (F3 lo corrige) y no colapsar errores a -1.
- TLS: solo estudio (ver `TLS.md`). IPv4 sin reenvio ni
  fragmentacion (F6+).
