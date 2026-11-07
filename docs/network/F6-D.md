# F6-D — Ampliacion DNS (resolucion, cache, TCP)

Base: `6a7ed57` (F6-C). Rama: `v2.3-red`. Sin cambios ABI ni
syscalls nuevas; Ring3 para logica DNS; nucleo intacto salvo
F6-C (el transporte UDP/TCP se reutiliza tal cual).

## 1. Estado inicial y auditoria (Etapa A)

`v2.3-red` @ `6a7ed57`, sincronizada, arbol limpio, `main`
en `0b0532a`, stash intacto. Inspeccionados `dns.c`,
`ulib` (codec), `red_udp` (58-60, datagrama <=512+8),
`red_tcp` (61), `terminal` (`consultar_dns` ejecuta `dns`
y muestra `/dnssal.txt`), vectores `t_red`, `DNS.md`.

Confirmados: (1) CNAME solo se salta (solo-CNAME = 0,
indistinguible de NODATA); (2) A aceptado sin comparar
NAME (spoof intra-respuesta); (3) punteros a cabecera
aceptados (`d<12`); (4) basura final aceptada; (5) TTL sin
clamp; (6) Z ignorado; (7) rcode 6-15 propagados;
(8) `dns.c` monodisparo sin cache/TCP/CNAME; (9) UDP
limite 512 en nucleo. Potenciales fail-closed que se
conservan: eco exacto (incl. mayusculas), QD comprimido
-> -2, `maxips` a cargo del llamador.

## 2. Parser robusto (Etapa B)

`dns_salta`: rechaza `d<12`. `u_dns_leer2` (nuevo;
`u_dns_leer` = envoltorio intacto): Z==0, `o==n` final,
A contado solo si NAME == pregunta o destino CNAME del
mensaje, CNAME validado (RDATA dentro del mensaje) con
expansion a texto y TTL minimo. `armar` intacto
(incluidos sus codigos -2/-3, para no romper tests).

## 3. CNAME (Etapa C)

Iterativo (no recursivo): hasta 8 saltos, anti-ciclos por
visitados + tope, cadena "a -> b -> c" (<=255) y TTL
minimo de la cadena. Si la respuesta ya trae el A final,
cero consultas extra. NODATA si k==0 sin CNAME;
NXDOMAIN/SERVFAIL sin reintento ni cache.

## 4. Cache (Etapa D)

Por proceso en `ulib` (32 entradas, ~5.6 KB fijos):
clave nombre-minusculas+servidor+puerto; TTL tope 86400,
0 = no cachear; sustitucion libre -> caducada ->
round-robin; sin negativos (evaluado: sin SOA no hay TTL
negativo fiable); cambio de DNS = miss natural.
`u_dns_cache_{limpiar,n,viva}` para operar y probar
(incl. wraparound con aritmetica con signo).

## 5. DNS sobre TCP (Etapa E)

Ante TC=1: TCP al mismo servidor:53, consulta con
prefijo BE16 (una escritura, <=302 B), lectura del
prefijo y del cuerpo por partes (maximo 4096 B),
revalidacion XID+pregunta, cierre con RST tras respuesta
completa (evita esperar el FIN del servidor); en error,
abortar siempre (sin TCB colgados). Deadline global lo
acota todo; peor caso acotado por operacion (<=12 s).

## 6. API (Etapa F)

`u_dns_res_t` {ip, ttl, rc, rcode, nrec, saltos, via_tcp,
de_cache, cname[256]}; `u_dns_resuelve3(dom, srv, pto,
r, espera_ms)` (0 = configurado/53/30 s, tope 60 s);
`u_dns_resuelve()` atajo. Retornos: 0, -1 args, -2
nombre, -3 NXDOMAIN, -4 rcode, -5 timeout/red,
-6 TCP, -7 ciclo/profundidad, -8 NODATA. `dns` muestra
nombre/IP/cadena/TTL/origen/transporte/tiempo y escribe
`/dnssal.txt` (+Origen/Transporte/Cadena). `rc` legacy
(0/1/2/3/4/5) intacto para `consultar_dns`.

## 7. Archivos

- `v2.0/user/lib/{ulib.h,ulib.c}` (leer2, resolver, cache)
- `v2.0/user/apps/dns.c` (CLI fino, mismo rc/uso)
- `v2.0/user/apps/terminal.c` (texto `mostrar_dns` F6-D)
- `v2.0/user/apps/t_red.c` (+18 vectores, +34 vivas)
- `v2.0/tools/dns_stub.py` (UDP+TCP 5353/5354, zonas)
- `docs/network/{DNS,TCP,PRUEBAS,LIMITACIONES}.md` + este
- ISO `v2.0/build/iso/FacilOS-v2.3-red-f6d.iso`

## 8. Limites y timeouts

Cache 32x~176 B; cadena 8/9 consultas; global 30 s
(param, tope 60 s); UDP 3x3 s; TCP msg <=4096;
enviar <=536/llamada, recibir <=2048/llamada;
espera recv 100/500 ms con deadline; TTL clamp 86400.

## 9. Pruebas

KAT: `udp OK`, `tcp OK` con/sin adaptador (nucleo
intacto). `t_red` sin red 52/0. Vivo 152/1 (solo
`rx-arp-puerta` flaky): 34 DNS con stub (directo+hit,
alias-full, cadena+texto, ciclo, 8 ok, 9 fallo,
NXDOMAIN, NODATA, SERVFAIL, REFUSED, TC->TCP, multi,
ttl0(+nocache), ttl-tope, timeout, otro-srv,
srv-cache, 33 miss, llena 32, desalojo, conserva) mas
F1-F6-C intacto. `consultar_dns` real: example/google
(IP/TTL/origen red/transporte UDP/tiempo) y NXDOMAIN.
PCAP: 138 UDP + 10 TCP a 5353 en `f6dfinal.pcap`.
v2.2: 258/0, 26/0, 15/0. `funcion.f32` pendiente.

Incidencias de prueba: 2 fallos transitorios de
`tcp-tupla-reuso` en pasadas con huerfanos del anfitrion
(limpieza con WQL invalida + binds en conflicto) y
slirp cargado; verde en pasada limpia (dt=701, gracia
-6). `dns: alias:`/`Cadena:` solo visibles en VGA
(igual que todo `pon()` del terminal), validados via
`cname` byte-exacto en `t_red`.

## 10. Limitaciones que permanecen

Ver `LIMITACIONES.md` y `DNS.md`: sin DNSSEC/DoT/DoH/
mDNS/PTR, sin negativos, nombres cacheables <=127,
una pregunta por consulta, respuestas UDP >512 solo
via TCP, eco exacto, `funcion.f32` sin vivo.
