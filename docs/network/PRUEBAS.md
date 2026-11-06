# Pruebas (QEMU; F3 en `FacilOS-v2.3-red-f3.iso`, F1/F2 en `...-f1.iso`)

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
