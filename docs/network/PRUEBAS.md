# Pruebas F1 (QEMU, red opcional `QRUN_RED=1`)

Red virtual: `-netdev user,id=red0 -device rtl8139,netdev=red0,
mac=52:54:00:12:34:56` (puerta 10.0.2.2, DNS 10.0.2.3). Sin la
variable, arranque sin red (comportamiento por defecto intacto).

Verificado en `FacilOS-v2.3-red-f1.iso`:
- Arranque sin adaptador: `red: sin adaptador (arranque sin red)`,
  `drv red : ok`; `red`/`mostrar_interfaz` informan en ventana;
  `t_red`: 3 pruebas 0 fallos (via negativa Tx/Rx).
- Arranque con adaptador: `red: rtl8139 io=c000 irq=11
  mac=52:54:00:12:34:56`; `CR=d RCR=f8a TSD0=2000` (OWN=1 en
  reposo: se transmite directo y confirma por TOK).
- Tx/Rx reales: `t_red` envia ARP who-has 10.0.2.2 y recibe
  respuesta de `52:55:0A:00:02:02`; `tx_ok=1 rx_ok=1`;
  7 pruebas 0 fallos.
- Regresion v2.2 en la misma ISO: t1_lib 258/0, tutf8 26/0,
  mem32 15/0, `funcion.f32` OK (`Hola desde F32 Lang`).
- Ventana: `mostrar_interfaz` y `red estado` con MAC/contadores.

No probado en F1 (externo real): LAN fisica e Internet; la red
`user` es NAT local (ver LIMITACIONES.md).
