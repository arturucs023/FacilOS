# DNS (v2.3-red F4 + F6-D)

Cliente como aplicacion Ring3 (`user/apps/dns.c`, programa `dns`)
sobre UDP propio (58-60) con fallback TCP (syscall 61). Tipo
A/IN al servidor de la configuracion (DHCP de F3 o manual con
`configurar_ip dns`). Sin DNSSEC, DoH/DoT (fuera de F6).

- Consulta: xid aleatorio (pid+ticks+intento), RD, QDCOUNT 1,
  QNAME en etiquetas, QTYPE A, QCLASS IN. 3 XID x 3 s por
  nombre; dentro del intento ignora lo ajeno y el ruido.
- Respuesta: exige xid y pregunta eco exacta de la enviada,
  QR=1, opcode 0, **Z=0** (F6-D), sin basura final (F6-D);
  comprension de nombres con validacion de limites (punteros
  jamas a la cabecera, F6-D) y guarda contra bucles (16).
  Los A cuentan solo si su NAME es la pregunta o un destino
  CNAME del mismo mensaje (F6-D; los ajenos se ignoran).
- CNAME (F6-D): se sigue la cadena hasta el A final (maximo
  8 saltos, anti-ciclos, TTL minimo de la cadena). Si la
  respuesta ya trae el A final no hay consultas extra. Sin
  CNAME y sin A = NODATA.
- Codigos: 0 con >=1 A = resuelta; 3 NXDOMAIN (no se cachea);
  2/4/5/otros = error del servidor; 0 sin A ni CNAME =
  "sin registros A"; sin nada en plazo = timeout (rc=1).
  Muestra dominio, servidor, IP (+N mas), TTL, origen
  (cache/red), transporte (UDP/TCP), cadena CNAME si la hay,
  tiempo (ms, resolucion 10 ms) y tipo.
- Cache F6-D (por proceso, en `ulib`, 32 entradas,
  ~5.6 KB fijos): clave nombre+servidor+puerto; TTL con tope
  de 86400 segundos (1 dia); TTL 0 = no cacheable; sustitucion primera libre,
  luego caducada, luego round-robin; sin negativos
  (evaluado: sin SOA no hay TTL negativo fiable). Cambiar el
  DNS deja las entradas antiguas inaccesibles (miss natural).
- TCP (F6-D): si UDP responde TC=1, se abre TCP al mismo
  servidor:53, se envia la consulta con prefijo BE16 y se lee
  prefijo + mensaje por partes (maximo operativo 4096 B).
  XID y pregunta se revalidan igual. Cierre con RST tras la
  respuesta completa (request/response; evita esperar el FIN
  del servidor); en cualquier error se aborta (sin TCB
  colgados). Respuesta TCP malformada o sin respuesta =
  error especifico (rc=4 "TCP fallo").
- Sin respuesta: `dns: sin respuesta (revisa la red)` (rc=1).
  rc: 0 resuelta, 1 timeout/red, 2 uso/dominio, 3 NXDOMAIN,
  4 error DNS/truncada/sin A/TCP/cadena, 5 sin red/config.
- Salida `/dnssal.txt` para `consultar_dns` (mismo patron que
  `/dhcsal.txt`; sin pipes en la ABI): dominio, servidor, IP,
  TTL, tiempo, origen, transporte y cadena si la hay.

El codec (`u_dns_armar`/`u_dns_leer`/`u_dns_leer2` en `ulib`)
lo comparten `dns` y `t_red`: el segundo prueba el parser
con vectores deterministas (A, CNAME solo/cadena/ajeno,
Z, basura final, punteros, RDLEN, tipo desconocido, bucles,
multi-A, cache incl. wraparound) sin red.

`u_dns_resuelve3()` (`ulib.h`) es la API reutilizable:
cache + UDP + CNAME + TCP con deadline global acotado
(30 s por defecto, tope 60 s). `u_dns_resuelve()` es el
atajo simple. Sin cambios ABI/syscalls (todo Ring3).

Demostrado con QEMU user-net: stub DNS controlado en el
anfitrion (10.0.2.2:5353 UDP+TCP, `tools/dns_stub.py`) para
CNAME/cadenas/ciclos/limites 8-9/NXDOMAIN/NODATA/SERVFAIL/
REFUSED/TC->TCP/TTL/eviccion/servidor/timeout; mas DNS real
(10.0.2.3): `example.com`, `google.com` resueltos con TTL y
tiempo reales; NXDOMAIN con nombre inventado.

Limites F6-D: nombres cacheables <=127; TTL 0 no se guarda;
sin negativos; una pregunta por consulta; sin DNSSEC;
sin PTR/reversa; respuestas >512 solo via TCP; sondas con
puerto/UDP distintos no comparten cache.
