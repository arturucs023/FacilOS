# API propia de red FacilOS32 (auditada; F1: 52-54)

Nombres en castellano. Sin `socket/bind/listen/accept`.

| N. | Nombre             | Operacion                              | Fase | Justificacion                        |
|----|--------------------|----------------------------------------|------|--------------------------------------|
| 52 | `red_interfaz`     | ficha 32 B: MAC/estado/contadores      | F1   | diagnostico base de todo             |
| 53 | `red_trama_enviar` | Tx trama Ethernet 14..1514             | F1   | prueba real L2 + diagnostico        |
| 54 | `red_trama_recibir`| Rx con espera maxima (ms, tope 10 s)  | F1   | prueba real L2, sin cuelgues        |
| 55 | `red_configurar`   | IP/mascara/puerta/DNS                  | F2/F3| un punto de configuracion L3 (la fija DHCP) |
| 56 | `red_consultar`    | ARP/DNS: nombre<->IP con espera        | F2/F4| una primitiva de resolucion      |
| 57 | `red_comprobar`    | eco ICMP con cuenta y espera           | F3   | `comprovar_conexion` sin ICMP crudo |
| 58 | `red_udp_ligar`    | liga puerto UDP (0=efimero)            | F4   | DNS y transporte sin sockets      |
| 59 | `red_udp_enviar`   | datagrama {ip,dport,sport}+datos       | F4   | Tx por IPv4+ARP+ruta              |
| 60 | `red_udp_recibir`  | datagrama {src,sport,len}+datos        | F4   | espera acotada, 0=agotada          |
| 61 | reservada          |                                        | F5   | TCP                                |
| 62 | reservada          |                                        | F5   | TCP                                |
| 63 | `red_rutas`        | leer/definir tabla de rutas            | F2/F6| `mostrar_rutas`/`trazar_ruta`     |

`SC_N` es 64 (52-60 y 63 en uso; 61-62 reservadas F5).
La ABI solo crece, nunca cambia: 52-57 y 63 intactas en F4
(`sc_54` sirve del tap tras bombear: mismo contrato).
`red_if_t` (32 B) identica en `drv_red.h` y `ulib.h`;
`rsv[3]` lleva UDP recibidos (F4). Ring3 anade `u_red_udp_*`
y el codec DNS `u_dns_armar`/`u_dns_leer` (compartido por
`dns` y `t_red`).

Envueltos Ring3: `u_red_interfaz`, `u_red_trama_enviar`,
`u_red_trama_recibir` (`sys_red_*` en `sys.h`).
