# Limitaciones conocidas (F1+F2+F3+F4)

- Red `user` de QEMU es NAT 10.0.2.0/24: sin entrada sin `hostfwd`,
  sin promiscuo externo. DNS lo da slirp (10.0.2.3); el eco ICMP
  externo depende de SLIRP (en estas pruebas responde; no se
  asume). LAN real exigiria puenteo o tap (fuera de F4).
- UDP: carga encolable 512 B, TTL 64 fijo, 4 ligas x cola 4,
  tabla llena desaloja la vacia mas antigua (-6 si todas tienen
  pendientes); sin broadcast dirigido ni fragmentacion.
- DNS solo A/IN sobre UDP (sin TCP: respuestas >512 darian
  timeout); sin persecucion CNAME; sin cache (cada consulta
  pregunta al servidor); una pregunta por consulta.
- ICMP solo eco (sin unreachable/time-exceeded ni reenvio; F6+).
  Contadores basales del KAT: `ICMP errores`=1, `UDP recibidos`
  =10 nada mas arrancar.
- El cargador lee el nucleo en sectores fijos: F4 lo sube de 160
  a 192 (llega a 0x28000, sin pisar el catalogo en 0x30000).
  Si el nucleo crece mas, hay que volver a subirlo (2 lineas:
  `boot32.asm` + `KERN_SECS`) y re-verificar el arranque.
- Sondeo, no IRQ: la recepcion se recoge al pedirla; latencia
  de sondeo, sin perdida (el anillo retiene; el deposito crudo
  guarda lo no consumido por la pila; desde F4 `sc_54` sirve del
  tap tras bombear para no robar tramas a la pila).
- Tx: 1 en vuelo como maximo, descriptores en round-robin
  (TSD0..3); Rx: anillo 16K; MTU 1500; sin reensamblado.
- Sin adaptador todo devuelve error limpio; nada cuelga.
- Lecciones de implementacion: todo PIO >255 exige `volatile`
  (GCC saca lecturas de los bucles); espera por iteraciones en
  syscall (ticks congelados con IF=0); TSD de Tx se rearma por
  descriptor; el resolvedor ARP debe reinspeccionar la cache tras
  cada bombeo (F3 lo corrige) y no colapsar errores a -1; la
  salida de una funcion solo vale si devuelve exito (F4: el test
  reutilizaba el bufer tras `armar` fallido).
- TLS: solo estudio (ver `TLS.md`). IPv4 sin reenvio ni
  fragmentacion (F6+).
