# F6-C — Estabilidad TCP y gestion de conexiones

Base: `b35f019` (F6-B). Rama: `v2.3-red`. Sin cambios ABI (62
sigue libre), sin tocar `drv_red`, `main` ni el stash.

## 1. Estado inicial

`v2.3-red` @ `b35f019`, sincronizada con origen, arbol limpio,
`main` en `0b0532a`, stash del compositor intacto.

## 2. Auditoria (Etapa A)

Inspeccion directa de `red_tcp.c/h`, `exec.c` sc_61, `red.c`,
`proc.c`, KAT y par Python, mas 4 subagentes de lectura.

Reproducidos (codigo + lectura exacta):
- D6: datos en FIN_TX descartados (`entrada` 587-590) mientras
  `recibir` admite FIN_TX: half-close asimetrico.
- D10: RST a puerto cerrado siempre seq 0 + RST+ACK, aun con
  ACK en el entrante.
- D12: `wnd_par` escrita pero nunca leida; SYN_TX retorna
  antes de aprender la ventana del SYN-ACK.
- D13: `espera=0` bloquea ~20-30 ms (`fin=now+1` + `duerme(2)`)
  en aceptar/recibir; enviar sin espera por ABI.
- D14: `cerrar` tira `snd_len` tras 300 ticks y emite FIN con
  hueco (el par lo ignora; ambos lados colgados).
- D15: sin TIME_WAIT (documentado); tardios -> RST.
- D9-parcial: controles saneados (F6-B); residuo forense en
  `snd[]/rcv[]` + `uso` muerto.
- Reloj: `tcp_reloj` solo avanza con syscall TCP (entrada o
  bucles bloqueantes); `red_bombea` no lo llamaba.

No reproducidos (comportamiento correcto verificado):
- D7: la sonda de 1 B con ventana llena SI responde ACK
  wnd=0 (ACK incondicional si `seq==rcv_nxt`). Solo faltaba
  KAT + comentario.
- D9-funcional: sin fuga protocolar (gating por
  `snd_len`/`rcv_n`).

Riesgos residuales (sin cambio, documentados):
- Re-Tx con ARP sin resolver bloquea hasta ~3 s (acotado).
- Reentrada IRQ->fusiona->limpia con puntero crudo en
  syscall: teorica (monohilo IF=0, sin kill en ABI).
- Sin persist-timer emisor (el RTO hace de sonda).

Alcance decidido: D6, D9-memset, D10 (`rst_solo`), D12, D13
(poll), D14 (RST+-7), D15 (cuarentena de tupla, no TIME_WAIT
clasico: 25% de tabla por cierre es DoS por cierre),
reloj-en-bombea. NO: ventana deslizante, OOO, TIME_WAIT con
slot, ARP-nb, cambios ABI.

## 3. Correcciones (Etapa B, minimas)

- D6: datos admitidos en FIN_TX si `!fin_rx` (datos+FIN del
  mismo segmento tambien).
- D7: sin cambio funcional; comentario + KAT.
- D9: `libc_memset` de `snd`/`rcv` en `nuevo`/`libera`.
- D10: `rst_band` (0x14/0x04); puerto cerrado con ACK ->
  RST pelado `seq=ackn`; sin ACK -> RST+ACK como antes.
- D12: `envia_nb` -8 si `wnd==0` o `n>wnd`; SYN_TX aprende
  la ventana del SYN-ACK.
- D13: `espera==0` = un bombeo + consulta, sin dormir, en
  aceptar/recibir. Enviar documentado bloqueante (ABI).
- D14: `cierra_descarta` (RST + -7, sin FIN con hueco) si
  `snd_len` tras drenar; helper testeable en KAT.
- D15: cuarentena best-effort (4 tuplas, 6 s, caducidad
  perezosa): guarda en cerrar-exito/timeout-con-ACK,
  abortar y limpia; control en `abre_nb` (efimero salta,
  explicito -6) y SYN pasivo (gota). Sin slot ocupado.
- Reloj: `red_tcp_reloj()` al final de `red_bombea()`.

## 4. Archivos

- `v2.0/src/kernel32/red_tcp.c` (correcciones + KAT 13..21)
- `v2.0/src/kernel32/red_tcp.h` (decl. `red_tcp_reloj`)
- `v2.0/src/kernel32/red.c` (reloj en `red_bombea`)
- `v2.0/user/apps/t_red.c` (+12 pruebas vivas)
- `docs/network/{TCP,PRUEBAS,LIMITACIONES}.md` + este informe
- ISO: `v2.0/build/iso/FacilOS-v2.3-red-f6c.iso` (copia
  verificada de la reconstruccion)

## 5. Estructuras y temporizadores

Sin cambios ABI/syscalls. Internos: `katreg_t.wnd` (solo
KAT), tabla `tups[4]` (48 B), `red_tcp_reloj()`; `rst_band`
parametriza `rst_para`. Temporizadores iguales (RTO 0.5..4 s
x4, conectar 10 s, cerrar 5 s, drenar 3 s) mas gracia 6 s.

## 6. Pruebas

KAT (con/sin adaptador): `tcp OK` (F5+F6-B+13..21). Repro:
13-21 nuevos sobre F6-B -> FALLO; tras corregir -> OK.
(2/9 exigieron corregir el test, no el codigo:
KAT-15 pedia ip_rem==0 en TCB recien abierto;
KAT-20 necesitaba tabla limpia al inicio.)

Vivo `t_red`: 110 pruebas 1 fallo (solo `rx-arp-puerta`
flaky; via nucleo OK). 12 nuevas: poll aceptar/recibir
(dt<=1 tick), tupla (abrir/eco/cerrar, -6 en gracia, reuso
tras 7 s). F1-F6-B intacto.

PCAP `f6clive.pcap`: SYN=165 SYNACK=11 FIN=21 RST=72 (14 sin
ACK = D10), DATA=10, pasiva 6101 completa en cable.

DHCP: obtener + renovar + liberar en vivo (86400 BE).

v2.2: t1_lib 258/0, tutf8 26/0, mem32 15/0. `funcion.f32`
pendiente (sin cambios que le afecten).

## 7. Limitaciones que permanecen

Ver `LIMITACIONES.md`: sin OOO/deslizante/persist-timer,
enviar bloqueante, gracia best-effort 6 s, RST tardio con
gracia consumida, ARP-bloqueante acotado, `rx-arp-puerta`
flaky ambiental, `funcion.f32` sin vivo.
