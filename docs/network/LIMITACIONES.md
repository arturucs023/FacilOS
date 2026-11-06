# Limitaciones conocidas (F1+F2)

- Red `user` de QEMU es NAT 10.0.2.0/24: sin entrada sin `hostfwd`,
  sin promiscuo externo, DHCP/DNS los da slirp (nuestro DHCP
  llegara en F3). Pruebas contra puerta 10.0.2.2; Internet/LAN
  real exigen puenteo o tap (fuera de F2).
- Sondeo, no IRQ (F2): la recepcion se recoge al pedirla; latencia
  de sondeo, sin perdida (el anillo retiene; el deposito crudo
  guarda lo no consumido por la pila).
- Tx: 1 en vuelo como maximo, descriptores en round-robin
  (TSD0..3); Rx: anillo 16K; MTU 1500; sin reensamblado.
- Sin adaptador todo devuelve error limpio; nada cuelga.
- Lecciones de implementacion: todo PIO >255 exige `volatile`
  (GCC saca lecturas de los bucles); espera por iteraciones en
  syscall (ticks congelados con IF=0); TSD de Tx se rearma por
  descriptor.
- TLS: solo estudio (ver `TLS.md`). IPv4 sin reenvio ni
  fragmentacion (F6+).
