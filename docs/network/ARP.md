# ARP (v2.3-red F2)

`src/kernel32/red_arp.c`. Solicitud/respuesta Ethernet/IPv4
(hw=1, proto=0x0800, hl=6, plen=4); resto se ignora.

- Cache: 16 entradas {IP, MAC, ticks, valida}, caducidad 60 s
  (ticks de 100 Hz), reemplazo de la mas antigua.
- `red_arp_resolver`: si hay entrada valida la sirve sin emitir
  (demostrado: segunda consulta sin Tx nueva); si no, difunde la
  solicitud y espera con rodajas de ~20 ms (tope configurable),
  reinspeccionando la cache tras cada bombeo y reintentando cada
  0,5 s. Devuelve 0 con MAC, 1 espera agotada, -1 sin adaptador.
  (F3 corrige: antes nunca reinspeccionaba y siempre agotaba.)
- `red_arp_entrada`: aprende toda direccion ajena valida; responde
  solo a solicitudes hacia nuestra IP (ignora tramas propias).
- `red_arp_anuncia`: anuncio gratuito al fijar IP (`configurar_ip`).
- Entradas visibles en `red estado` (`rsv[0]` de la ficha).

Demostrado con la puerta QEMU (10.0.2.2 -> 52:55:0A:00:02:02).
