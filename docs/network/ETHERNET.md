# Ethernet II (v2.3-red F2)

Tramas construidas y analizadas a mano, sin pila prestada.

Formato: destino[6] origen[6] EtherType[2] carga[46..1500] (+CRC
de 4 que pone/quita el hardware; el driver presenta tramas sin
CRC a la pila y exige minimo 14 B de cabecera).

EtherTypes atendidos en F2: `0x0806` (ARP, `red_arp_entrada`) y
`0x0800` (IPv4, `red_ip_entrada` + validacion). Resto: al deposito
crudo de diagnostico, sin procesar.

Tx (una en vuelo, round-robin TSD0..3): la 8139 no rellena sola;
el driver completa a 60 B. Confirmacion por TOK en ISR con espera
acotada por iteraciones (los ticks estan congelados en syscall).

Detalles de dispositivo y registros: ver `CONTROLADOR.md`.
