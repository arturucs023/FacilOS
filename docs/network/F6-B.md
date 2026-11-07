# Informe F6-B — Correcciones P0 (v2.3-red, base 085f4c5)

Sin cambios ABI (62 sigue libre), sin tocar `drv_red`, `main` ni stash.

## D1 — DHCP opcion 51 big-endian
- Repro: `v2.0/user/apps/dhcp.c:243-244` usaba `lee_u32` (LE) para
  la opcion 51. Vector `00 00 00 3C` daba `1006632960` en vez de
  `60`. En vivo F5: `concesion (s): 2152792320`.
- Arreglo: `lee_be32` solo para opcion 51 (`dhcp.c`); IPs siguen
  en LE (orden del anfitrion FacilOS32). 58/59 ignoradas y
  documentadas (sin temporizador de renovacion).
- Verificacion: `t_red` 34/0 sin adaptador (`dhcp-tiempo-60-be`,
  `dhcp-tiempo-86400-be`); en vivo `concesion (s): 86400`
  (`f6blive.log`). Archivos: `dhcp.c`, `t_red.c`, `DHCP.md`.

## D2 — Fuga TCB sticky al morir proceso
- Repro: KAT nuevo secc. 7 (4x conecta+RST→`-9`, `limpia` debe
  devolver 4 y reutilizar). Antes: `KAT tcp FALLO`
  (`f6brepro.log`); `limpia` devolvia 0 y no saneaba.
- Arreglo: `limpia_proceso` libera tambien `CERRADO+err` del pid
  muerto (sin RST) + `libera`/`nuevo_tcb` con saneado completo
  de tuple/seq/contadores (`red_tcp.c`).
- Verificacion: `KAT tcp OK` con/sin adaptador (`f6bfix.log`,
  `f6bkat.log`). KAT secc. 7 pasa y tabla reutilizable (4
  escuchas nuevas).

## D3 — Id inestable en `abrir`
- Repro: codigo hacia `id = id_de(t)` cada vuelta (alias si el
  slot se reutiliza). KAT secc. 8: id viejo rechazado (`-2`)
  tras `libera`+reuso.
- Arreglo: `oid` estable en `red_tcp_abrir` + deteccion
  `id_de(t) != oid` → `-9` (`red_tcp.c`).
- Verificacion: KAT OK; vivo `tcp-rechazada-rapido`,
  `tcp-negro-*` sin cambios.

## D4 — Re-Tx FIN con seq erronea
- Repro: KAT secc. 9 (`fin_nb`, fuerza `tcp_reloj`, compara
  seq). Antes: segundo FIN con `snd_nxt` (N+1) en vez de
  `fseq=N`.
- Arreglo: `reenvia` en `FIN_TX` usa `snd_nxt-1`.
- Verificacion: KAT OK (ambos FIN con `band=0x11` y `seq=fseq`).

## D5 — FIN duplicado sin re-ACK
- Repro: KAT secc. 10 (FIN, luego mismo FIN con `seq=rn-1`).
  Antes: sin Tx. Despues: +1 ACK (`0x10`) sin avanzar `rcv_nxt`.
- Arreglo: rama `fin_seq+1==rcv_nxt && fin_rx` en ambos estados.
- Verificacion: KAT OK.

## D8 — Backlog lleno con RST incorrecto
- Repro: KAT secc. 11 (escucha, SYN1→SYN-ACK, SYN2 con pendiente
  ocupado). Antes: RST (`0x14`). Comportamiento correcto F6-B:
  gota silenciosa (el cliente reintenta por RTO); RST solo sin
  escucha.
- Arreglo: flag `ocupado` en demux de escucha (`red_tcp.c`).
- Verificacion: KAT OK (segundo SYN sin Tx); pasiva viva
  `tcp-aceptar/datos-huesped` OK.

## D11 — ACK invalido descarta datos/FIN validos
- Repro: KAT secc. 12 (datos en orden con `ack=una-1`).
  Antes: `rcv_n==0` (gota). Despues: `rcv_n==2` + ACK.
  Vector futuro (`ack=nxt+100`) se tira en ambos (correcto).
- Arreglo: `ack_futuro` = gota; `ack_viejo` = ignora ACK pero
  procesa datos/FIN; `wnd_par` se actualiza igual.
- Verificacion: KAT OK; resto de estados sin cambios (vivo 98).

## Archivos
- `v2.0/src/kernel32/red_tcp.c` (D2/D3/D4/D5/D8/D11 + KAT 7-12)
- `v2.0/user/apps/dhcp.c` (D1)
- `v2.0/user/apps/t_red.c` (regresion `dhcp-tiempo-*`)
- `docs/network/{TCP,DHCP,PRUEBAS,LIMITACIONES}.md` + este informe
- ISO: `v2.0/build/iso/FacilOS-v2.3-red-f5.iso` reconstruida
  (mismo nombre que F5; contenido F6-B verificado en arranque)

## Resultados
- KAT: `udp OK`, `tcp OK` con/sin adaptador.
- Sin adaptador: `t_red` 34/0.
- Vivo: `t_red` 98 pruebas 1 fallo (solo `rx-arp-puerta` flaky
  ambiental; `arp-nucleo-resuelve` pasa en la misma ejecucion).
- pcap `f6blive.pcap`: SYN=154 SYNACK=8 FIN=17 RST=65 DATA=8,
  pasiva 6101 completa en cable.
- v2.2: t1_lib 258/0, tutf8 26/0, mem32 15/0.
- Pendiente (no P0, no abordado): P1, CNAME, DNS/TCP, API.
