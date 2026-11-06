# IPv4 (v2.3-red F2)

`src/kernel32/red_ip.c`. Cabecera de 20 B sin opciones.

- Construccion (`red_ip_enviar`): ver 4, IHL 5, TOS 0, longitud,
  ID incremental, flags 0, TTL (defecto 64, configurable por
  llamada), protocolo, checksum, origen (nuestra IP), destino.
  Routing: entrega directa si `(dst & mask) == (ip & mask)`;
  si no, puerta de enlace; sin ruta o sin IP: error.
  La MAC destino sale de la cache ARP (`red_arp_resolver`).
- Validacion (`red_ip_valida`): version, IHL, longitud total
  dentro del marco, checksum == 0, TTL != 0. La entrada cuenta
  validas/invalidas; en F2 no hay consumidor (llega en F3+).
- KAT al arrancar (`red: KAT ip OK`): vector RFC 791
  (checksum B861) + rechazos (cabecera corrupta, TTL 0 con
  checksum recalculado, total < IHL, marco corto).

Sonda de diagnostico: `red_consultar` tipo 2 envia un datagrama
IPv4 (proto configurable, p. ej. 17) y prueba el camino completo
Tx (ruta + ARP + cabecera + checksum) por el cable real.
