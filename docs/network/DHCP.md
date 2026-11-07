# DHCP (v2.3-red F3)

Cliente como aplicacion Ring3 (`user/apps/dhcp.c`, programa `dhcp`),
sin UDP en el nucleo: construye Ethernet+IPv4+UDP+DHCP el solo
(trama cruda con syscalls 52/53/54) y aplica con `red_configurar` (55).

- Obtener: discover (broadcast) -> offer -> request (broadcast con
  IP pedida + servidor) -> ACK. 3 intentos por fase, 4 s cada espera.
  Sin oferta: `dhcp: sin respuesta`; sin ACK: `sin confirmacion`.
- Extrae IP, mascara, puerta, DNS, servidor y concesion (s); la puerta
  y el DNS llegan como opciones 3 y 6; la concesion como opcion 51
  en orden de red (big-endian; F6-B corrige la lectura LE anterior:
  `00 00 00 3C` = 60 s, no 1006632960). 58/59 se ignoran a proposito
  (sin temporizador de renovacion en esta fase; concesion informativa).
- Renovar (`dhcp renovar`): relee la IP actual y repite discover con
  IP pedida (INIT-REBOOT); si no hay IP, obtiene desde cero.
- Liberar (`dhcp liberar`): descubre el servidor, envia release
  unicast y limpia la configuracion local (IP/mascara/puerta/DNS a 0)
  aunque no haya servidor; sin IP no hay nada que liberar (rc=0).
- Sin adaptador: `dhcp: sin adaptador` (rc=1); el arranque sigue sin
  red. Ninguna espera es indefinida (topes 2-4 s por intento).
- Informa por terminal y deja `/dhcsal.txt` (IP concedida) para que
  `configurar_dhcp` lo vuelque en la ventana (mismo patron que
  `/f32sal.tmp`; sin pipes en la ABI).

Demostrado con QEMU user-net: oferta de 10.0.2.2, `10.0.2.15/24`,
puerta 10.0.2.2, DNS 10.0.2.3, concesion 2152792320 s.

Limites F3: solo cliente (sin servidor ni reenvio); el DNS solo se
guarda (las consultas son F4); sin persistencia entre arranques.
