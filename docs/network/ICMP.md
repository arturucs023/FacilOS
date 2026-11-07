# ICMP (v2.3-red F3)

`src/kernel32/red_icmp.{h,c}`. Solo eco (request/reply); sin mensajes
de error ni reenvio (`trazar_ruta` esta reservado, no implementado).

- `red_icmp_entrada`: valida longitud minima (14+20+8) y checksum;
  cuenta errores. Las respuestas casan con la unica espera pendiente
  (IP+ident+seq); las solicitudes a nuestra IP se responden (type 0,
  TTL 64, checksum recalculado). Otro tipo se ignora sin contar.
- `red_icmp_ping`: una espera pendiente como maximo; envia por
  `red_ip_enviar` (ruta + ARP + TTL) y bombea hasta el reply o el
  tope (max 10 s). Devuelve 1 (con `rtt_ms` y TTL del reply), 0 si
  agota la espera, o el error del envio (-1 sin adaptador, -3 sin
  ruta, -4 ARP agotado, -5 fallo Tx; -2 args). Los errores ya no se
  colapsan a -1 (antes ocultaban la causa real).
- Contadores en `red estado`: `rsv[1]` ok, `rsv[2]` errores. El KAT
  deja `errores=1` basal a proposito (trama corta de prueba).
- KAT al arrancar (`red: KAT icmp OK`): vector de eco de 40 B con
  checksum conocido, corrupcion detectada y rechazo de trama corta.
- Syscall 57 `red_comprobar` + `comprovar_conexion [-c N] <ip>`:
  N ecos (1..10, 1 s cada uno), salida 6 B por eco {seq,rtt,ok,ttl}.

Demostrado contra la puerta QEMU: 2/2 respuestas, TTL 255 (el que
envia slirp), tiempo 0 ms (resolucion de 10 ms); `192.168.99.99`
da `sin respuesta` limpio (0 de 1).
