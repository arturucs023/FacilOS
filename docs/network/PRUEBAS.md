# Pruebas (QEMU; F1+F2 en `FacilOS-v2.3-red-f1.iso`)

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
