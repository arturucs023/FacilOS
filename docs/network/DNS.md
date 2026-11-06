# DNS (v2.3-red F4)

Cliente como aplicacion Ring3 (`user/apps/dns.c`, programa `dns`)
sobre UDP propio (58-60): valida la via UDP real en cada consulta.
Tipo A/IN al servidor de la configuracion (DHCP de F3 o manual
con `configurar_ip dns`). Sin TCP, DNSSEC, DoH/DoT (fuera de F4).

- Consulta: xid aleatorio (pid+ticks+intento), RD, QDCOUNT 1,
  QNAME en etiquetas, QTYPE A, QCLASS IN. 3 intentos x 3 s con
  xid nuevo; dentro del intento ignora lo ajeno y el ruido.
- Respuesta: exige xid y pregunta eco exacta de la enviada,
  QR=1, opcode 0; TC (truncada) es error (sin TCP en F4);
  comprension de nombres con validacion de limites y guarda
  contra bucles (max 16 saltos); lee hasta 8 registros A
  (IP+TTL), salta CNAME y el resto sin asumir nada.
- Codigos: 0 con >=1 A = resuelta; 3 NXDOMAIN; 2/5/otros =
  error del servidor; 0 sin A = "sin registros A"; sin nada
  en 3 intentos = timeout (rc=1). Muestra dominio, servidor,
  IP (+N mas), TTL, tiempo (ms, resolucion 10 ms) y tipo.
- Sin cache en F4 (decision Fase E): el cliente es monodisparo
  y su memoria muere con el proceso; guardar TTL sin persistencia
  no aporta. El TTL se extrae y se muestra en cada respuesta
  (prueba observable de su tratamiento). `mostrar_dns` lo recuerda.
- Salida `/dnssal.txt` para `consultar_dns` (mismo patron que
  `/dhcsal.txt`; sin pipes en la ABI). rc: 0 resuelta, 1 timeout,
  2 uso/dominio, 3 NXDOMAIN, 4 error DNS/truncada/sin A,
  5 sin red/config (adaptador, IP, servidor, puerto).

El codec (`u_dns_armar`/`u_dns_leer` en `ulib`) lo comparten
`dns` y `t_red`: el segundo prueba el parser con 16 vectores
deterministas (malformadas, xid, pregunta, NXDOMAIN, SERVFAIL,
truncada, corta, punteros, bucles, multi-A, CNAME).

Demostrado con QEMU user-net (DNS 10.0.2.3): `example.com` y
`google.com` resueltos con TTL y tiempo reales; NXDOMAIN con
nombre inventado; timeout contra 10.0.2.2:53 (sin servicio);
servidor ausente/manual via `configurar_ip dns`.

Limites F4: sin persecucion CNAME (se informa "sin registros A"
si solo hay CNAME); sin DNS sobre TCP (respuestas >512 no caben
en la cola UDP y darian timeout); una pregunta por consulta.
