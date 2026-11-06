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
| 58 | `red_canal_abrir`  | canal udp/tcp cliente|servidor        | F4/F5| unidad de transporte propia     |
| 59 | `red_canal_enviar` | envio por canal                        | F4/F5| datagramas/flujo por canal       |
| 60 | `red_canal_recibir`| recepcion acotada por canal           | F4/F5| sin bloqueos indefinidos         |
| 61 | `red_canal_estado` | estado y contadores del canal          | F5   | `mostrar_conexiones`                |
| 62 | `red_canal_cerrar` | libera el canal                        | F4/F5| limites de recursos               |
| 63 | `red_rutas`        | leer/definir tabla de rutas            | F2/F6| `mostrar_rutas`/`trazar_ruta`     |

`SC_N` es 64 (52-57 y 63 en uso; 58-62 reservadas F4/F5).
La ABI solo crece, nunca cambia: 52-56 y 63 intactas en F3.
`red_if_t` (32 B) identica en `drv_red.h` y `ulib.h`;
`rsv[1]`/`rsv[2]` llevan ICMP ok/errores (F3).

Envueltos Ring3: `u_red_interfaz`, `u_red_trama_enviar`,
`u_red_trama_recibir` (`sys_red_*` en `sys.h`).
