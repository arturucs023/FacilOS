# Limitaciones conocidas (F1)

- user-net de QEMU es NAT 10.0.2.0/24: sin entrada sin `hostfwd`,
  sin promiscuo externo, DHCP/DNS los da slirp (nuestro DHCP
  llegara en F3). Pruebas F1 = red virtual + puerta 10.0.2.2.
  Internet/LAN real exige puenteo o tap: fuera de F1.
- Sondeo, no IRQ (F1): la recepcion se recoge al pedirla; latencia
  de sondeo, sin perdida (el anillo retiene).
- 1 trama Tx en vuelo; anillo Rx 16K; MTU 1500; sin reensamblado.
- Sin adaptador todo devuelve error limpio; nada cuelga.
- DMA coherente por TCG/QEMU; en HW real habria que validar
  invalidacion de cache (anotado para F8).
- TLS: solo estudio (ver `TLS.md`).
