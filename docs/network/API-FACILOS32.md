# API propia de red FacilOS32 (auditada; F1: 52-54)

Nombres en castellano. Sin `socket/bind/listen/accept`.

| N. | Nombre             | Operacion                              | Fase | Justificacion                        |
|----|--------------------|----------------------------------------|------|--------------------------------------|
| 52 | `red_interfaz`     | ficha 32 B: MAC/estado/contadores      | F1   | diagnostico base de todo             |
| 53 | `red_trama_enviar` | Tx trama Ethernet 14..1514             | F1   | prueba real L2 + diagnostico        |
| 54 | `red_trama_recibir`| Rx con espera maxima (ms, tope 10 s)  | F1   | prueba real L2, sin cuelgues        |
| 55 | `red_configurar`   | IP/mascara/puerta/DNS                  | F2/F3| un punto de configuracion L3 (la fija DHCP) |
| 56 | `red_consultar`    | ARP (t0) y sonda IP (t2) con espera; tipo 1 (DNS) no implementado: -2 | F2 | resolucion ARP y sonda sin crudo |
| 57 | `red_comprobar`    | eco ICMP con cuenta y espera           | F3   | `comprovar_conexion` sin ICMP crudo |
| 58 | `red_udp_ligar`    | liga puerto UDP (0=efimero)            | F4   | DNS y transporte sin sockets      |
| 59 | `red_udp_enviar`   | datagrama {ip,dport,sport}+datos       | F4   | Tx por IPv4+ARP+ruta              |
| 60 | `red_udp_recibir`  | datagrama {src,sport,len}+datos        | F4   | espera acotada, 0=agotada          |
| 61 | `red_tcp`        | TCP op-multiplexada (op 0..7)          | F5   | abrir/escuchar/aceptar/datos/cierre |
| 62 | reservada        |                                        |      | libre                               |
| 63 | `red_rutas`        | leer tabla de rutas (solo op 0)        | F2   | `mostrar_rutas` (derivada en vivo)  |

`SC_N` es 64 (52-61 y 63 en uso; 62 libre).
La ABI solo crece, nunca cambia: 52-61 y 63 intactas en F6-D
(`sc_54` sirve del tap tras bombear: mismo contrato).
`red_if_t` (32 B) identica en `drv_red.h` y `ulib.h`;
`rsv[3]` lleva UDP recibidos (F4). Ring3 anade `u_red_udp_*`
y el codec DNS `u_dns_armar`/`u_dns_leer` (compartido por
`dns` y `t_red`).

Envueltos Ring3: `u_red_interfaz`, `u_red_trama_enviar`,
`u_red_trama_recibir` (`sys_red_*` en `sys.h`). F7-B anade el
codec HTTP/1.0 `u_http_*` (`u_http_url`, `u_http_pide`,
`u_http_res_{inicia,datos,fin}`, `u_http_cabecera`,
`u_http_trae`): reutiliza DNS+TCP sin syscalls nuevas.
