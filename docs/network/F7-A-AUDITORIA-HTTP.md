# F7-A — Auditoría de viabilidad: cliente HTTP/1.0 en Ring 3

Solo lectura. Sin cambios de código, syscalls, ABI ni ISOs.
Rama `v2.3-red` @ `9368132`, sincronizada con
`origin/v2.3-red`, árbol limpio, `main` en `0b0532a`,
stash del compositor intacto. Inspeccionado:
`v2.0/src/kernel32/{red_tcp.{h,c},exec.c,red.c}`,
`v2.0/user/lib/{ulib.h,ulib.c,sys.h}`,
`v2.0/user/apps/{t_red.c,terminal.c,dhcp.c,dns.c}`,
`v2.0/tools/{qrun.py,tcp_par.py,dns_stub.py}`,
`docs/network/{TCP,DNS,API-FACILOS32,PRUEBAS,HTTP,TLS}.md`.

Leyenda: [I] confirmado por inspección del código,
[P] confirmado mediante pruebas en QEMU, [H] hipótesis
razonada, [V] pendiente de validación.

## 1. Estado inicial del repositorio

- HEAD `9368132` (F6-E documental). F1–F6-E cerradas con
  ISO por fase (`f1,f3,f4,f5,f6b,f6c,f6d` intactas).
- No existe `v2.0/user/apps/http.c` ni cliente HTTP alguno
  (verificado: el fichero no existe).
- `docs/network/HTTP.md` y `TLS.md` son esqueletos que
  reservan estructura; no describen implementación.
- `trazar_ruta`, `mostrar_conexiones`, `solicitar_web` no
  existen en `terminal.c` ni en el repo (F6-E-A §8).

## 2. Arquitectura TCP y DNS existente

TCP propio (`red_tcp.{h,c}`), sin sockets, Ring 0; Ring 3
habla por syscall 61 op-multiplexada 0..7 y envoltorios
`u_tcp_*` (`ulib.h:291-299`, `ulib.c`). 4 TCB con dueño
pid; stop-and-wait (1 segmento ≤536 B sin ACK a la vez,
`red_tcp.c:885-904`); Rx circular 2048 B con acuse
inmediato; OOO se descarta y el par retransmite; RTO fijo
500 ms x2 (tope 4 s), 4 reintentos (`red_tcp.c:14-18`);
ventana anunciada respetada al enviar (-8 si 0 o
insuficiente, `red_tcp.c:893`); cierre ordenado con FIN
duplicado re-acusado; sin TIME_WAIT clásico: cuarentena
de tupla 6 s (`TUP_GRACIA`, `red_tcp.c:198`).
DNS Ring 3 (`ulib.c` + `dns.c`): A/IN, CNAME ≤8 con
anti-ciclos, caché 32 entradas por proceso con TTL
tope 86400 (`DNS_TTL_MAX`), sin negativos, fallback TCP
(prefijo BE16, tope 4096 B `DNS_TCP_MAX`, lecturas por
partes, RST tras respuesta) ante TC=1, deadline global
30 s por defecto (tope 60 s).

## 3. Contratos Ring 3 reutilizables (tal cual, sin cambios)

| Operación | Cómo | Retorno / errores | Bloqueo / tope |
|---|---|---|---|
| Abrir | `u_tcp_abrir(ip,puerto,0)` (local 0 = efímero; salta tuplas en gracia, `red_tcp.c:779-786`) | id>0; -1/-2/-3/-4/-5/-6/-7/-9 (`red_tcp.h:37-42`) | Bloquea ~10 s (`CONEX_ESP`, `red_tcp.c:809`) |
| Enviar | `u_tcp_enviar(id,buf,len≤536)` (`exec.c:1764`) | len o neg; -8 ventana/uno-en-vuelo; -7 tras ~12 s sin ACK (`red_tcp.c:913-920`) | Bloquea hasta ACK; NO admite espera parcial ni poll |
| Recibir | `u_tcp_recibir(id,buf,max≤2048,espera)` (`exec.c:1718`) | bytes; 0 si vacía y (espera agotada o `espera==0`); **-12 EOF** si FIN ajeno con todo drenado (`red_tcp.c:957`); neg | Bloquea hasta `espera` (tope 10 s); `espera=0` = un bombeo sin dormir [P F6-C] |
| Cerrar | `u_tcp_cerrar(id)` | 0 (FIN/ACK + FIN ajeno + ACK final) o -7 (sin ACK: con FIN ajeno visto; sin él: `cierra_descarta` con RST, `red_tcp.c:989-998`); -8 estado | Bloquea ~5 s + dreno 3 s |
| Abortar | `u_tcp_abortar(id)` | siempre 0 (o -2/-6 titularidad) | No bloquea |
| Resolver | `u_dns_resuelve3(dom,0,0,&r,ms)` (srv 0=configurado, pto 0=53) | 0 + `u_dns_res_t` (ip/ttl/rc/rcode/nrec/saltos/via_tcp/de_cache/cname[256]); -1..-8 (args/nombre/NXDOMAIN/rcode/timeout-red/TCP/ciclo/NODATA) | Deadline global acotado |
| Enviar-recibir UDP | `u_red_udp_{ligar,enviar,recibir}` | análogo F4; una liga por proceso reutilizada en DNS | Acotado (tope 10 s) |

Distinciones Clave para HTTP:
- EOF (-12) vs timeout (0 con espera>0 agotada): el código
  los separa explícitamente (`red_tcp.c:949-959`) [I][P:
  `tcp-eof-*` vs `tcp-negro-acotado` en `t_red`].
- Respuestas >2048 B: bucle `recibir` drenando `rcv_n`;
  el núcleo trunca al hueco y re-acusa; el par reenvía
  lo no confirmado (ventana receptora estándar) [I][V:
  sin prueba multikilobyte].
- Cierre correcto: `cerrar` tras consumir hasta -12;
  si el servidor cierra primero, su FIN llega como
  datos+FIN o FIN solo y se acusa ([I] `red_tcp.c:703-731`,
  [P] `tcp-eof-fin/huesped`).
- Servidor que no responde: RTO agota → -7 [P:
  `tcp-negro-*`].
- Servidor que RSTea con datos pendientes en el par:
  RST en ESTABLECIDO mata con -9 sticky; si la app ya
  tiene el cuerpo completo por Content-Length, puede
  considerar éxito (decisión de diseño F7-B).
- Proceso terminado a mitad de conexión:
  `proc_fusiona` → `red_tcp_limpia_proceso` (RST si
  ruta+ARP en caché, sin bloquear; libera + cuarentena)
  [I][P KAT D2 F6-B].
- Límites por operación: envío 1..536 (`exec.c:1764`
  rechaza >536 con -2 [P: `tcp-enviar-largo`]);
  recepción 1..2048 por llamada (`exec.c:1718`);
  sin ventana deslizante emisora (un segmento en vuelo);
  ventana receptora 2048 anunciada (sondas de 1 byte
  respondidas [P F6-C D7]).

## 4. Viabilidad de HTTP/1.0

Veredicto: **viable con limitaciones** (detalle §11).
- Transferencias multikilobyte con stop-and-wait: VIABLE
  POR DISEÑO [H con base [I]]. Cada `enviar` se confirma
  antes del siguiente; cada `recibir` drena hasta 2048 B
  y el núcleo anuncia ventana restante (incluido 0 con
  respuesta a sonda). Un servidor conforme respeta la
  ventana y retransmite lo no acusado. Lentitud esperada
  (~4 segmentos en vuelo como máximo), no incorrección.
  Peticiones GET típicas caben en 1 segmento; respuestas
  grandes requieren bucle de recepción con tope.
- Lo NO probado [V]: respuestas >2048 B (nadie ha movido
  más de ~536 B útiles por segmento en pruebas: eco,
  DNS ≤512, "hola"). La primera prueba F7-B debe ser un
  cuerpo de, p. ej., 8 KiB con Content-Length exacto.
- Riesgo conocido: respuestas que superen la ventana sin
  drenaje diligente de la app → la app DEBE leer en
  bucle con deadline; si no, el servidor se queda
  esperando ventana (cuelgue aparente, no error). El
  diseño debe imponer lectura continua + timeout global.
- Servidores que ignoran la ventana o envían segmentos
  >536 sin MSS: el demux trunca a hueco y re-acusa;
  converge por retransmisión [H]. Validar contra stub
  propio primero, no contra Internet.

## 5. Diseño propuesto (Ring 3, sin decidir ficheros)

`http.c` como app fina + parser reutilizable en `ulib`
(precedente DNS: codec compartido `dns`/`t_red`; mismo
patrón facilita el futuro navegador sin duplicar).
Reutilizar tal cual: `u_dns_resuelve3`,
`u_tcp_{abrir,enviar,recibir,cerrar,abortar}`.
Petición: `GET <path> HTTP/1.0`, `Host:`, `Connection:
close`, cabeceras básicas configurables; construcción
en buffer acotado con validación de URL (esquema
`http://`, host LDH/IP, puerto opcional 1..65535,
path por defecto `/`; rechazar `https://`, usuario@,
controles y longitudes).
Respuesta: línea de estado (`HTTP/1.0 NNN motivo`),
código, cabeceras `Nombre: valor` (límite nº+longitud,
plegados rechazados como malformados), cuerpo según:
(a) Content-Length válido → N bytes exactos (más bytes
= error, menos + EOF prematuro = truncada);
(b) sin Content-Length → hasta EOF (-12, cierre del
servidor); (c) `Transfer-Encoding:` presente (incluido
`chunked`) → error "no soportado", NUNCA silencioso.
Cuerpo mayor que el tope → error antes de desbordar
(o streaming a fichero tipo `/dnssal.txt`; decidir en
F7-B).
Redirecciones 301/302/303/307/308: SOLO documentar
(`Location`, y que HTTPS no está disponible: cualquier
`Location: https://` es error terminal, no seguimiento).
No implementar seguimiento en F7-B salvo aprobación.

## 6. Límites de memoria y tiempo (propuesta)

- URL ≤512 B; línea de petición+headers a enviar ≤2048 B
  (múltiples `enviar` si excede; típico <256 B).
- Sección cabeceras ≤8 KiB; máximo 32 cabeceras;
  línea >512 B o sección excedida → error.
- Cuerpo: tope operativo propuesto 64 KiB en RAM
  (`sys_memoria`/BSS por confirmar en F7-B) o streaming
  a fichero con tope 1 MiB; por encima → error.
  (RAM útil ~24 MiB; t_red usa bufs de 600 B; la cifra
  exacta se fija midiendo `sys_memoria` en F7-B.)
- Timeouts: conectar ≤10 s (núcleo); enviar ≤12 s
  (núcleo); recepción por llamada ≤ espera (p. ej. 5 s);
  global por petición p. ej. 60 s (cubre DNS 30 s +
  conexión + transferencia); sin reintentos a nivel
  HTTP (los del núcleo bastan).
- Content-Length no numérico/negativo/desbordado →
  error; `Transfer-Encoding` cualquiera → error;
  datos tras EOF → ignorar (conexión liberada);
  respuesta sin delimitación y sin cierre antes del
  deadline → timeout; servidor que no cierra →
  timeout, nunca espera infinita.

## 7. Riesgos técnicos

R1. Ventana 2048 B + servidor agresivo: convergencia
lenta pero correcta [H]; mitigación: drenar siempre en
bucle (no una sola lectura).
R2. RST del servidor tras respuesta completa (algunos
cierran con RST): llegaría -9 en vez de -12; el diseño
debe aceptar cuerpo-completo+RST como éxito con aviso.
R3. Cuarentena de tupla 6 s: conexiones HTTP/1.0 usan
puertos efímeros distintos (el núcleo salta tuplas en
gracia [P F6-C]), sin impacto salvo reutilización del
mismo puerto local <6 s (la app usará local=0).
R4. `enviar` sin poll ni cancelación: una ventana
cerrada prolongada devuelve -8 inmediato (reintentar
con backoff propio acotado) [I][P].
R5. DNS comparte liga UDP por proceso y TCB por
conexión: fugas solo si la app no cierra/aborta;
el diseño debe cerrar/abortar en TODAS las ramas
(precedente `dns_tcp_xchg` F6-D).
R6. Riesgo navegador: parser permisivo hoy =
XSS/SSRF mañana; exigir estrictez desde F7-B
(rechazar antes que adivinar).

## 8. Plan de pruebas (entorno QEMU existente, sin tocar
`qrun.py`: usar `QRUN_RED=1`, `QRUN_FWD` si hiciera
falta pasivo — innecesario: el invitado inicia hacia
10.0.2.2:puerto, NAT automático probado con
`tcp_par.py:5001` y `dns_stub.py:5353)

`tools/http_stub.py` (nuevo, anfitrión, patrón
`dns_stub.py`/`tcp_par.py`): respuestas 1.0 normales,
con Content-Length, fragmentadas (1 byte/segmento),
terminadas por cierre, chunked (debe dar error),
cabeceras malformadas, Content-Length incorrecto
(mayor/menor), cuerpo vacío, 404/500, redirecciones
(solo validar detección+`Location`), timeout (acepta
y calla), cierre prematuro a mitad de cuerpo.
Matriz KAT-equivalente en app de prueba o `t_red`:
vectores de parser (línea de estado, headers,
plegados, límites) + vivo contra stub + una real
(`example.com`) como humo, no como dependencia.
PCAP con `QRUN_PCAP` para handshake, re-Tx, FIN/RST.

## 9. Cambios necesarios en F7-B (propuesta, pendiente
de aprobación)

- NINGÚN cambio de núcleo/syscalls/ABI/tablas/límites:
  todo cabe en Ring 3 con contratos actuales.
- Nuevo: `v2.0/user/apps/http.c` (+ posible
  `u_http_*` en `ulib` si el parser debe reutilizarse;
  decidir ubicación según `ulib` existente, sin capa
  de abstracción innecesaria).
- Nuevo: `tools/http_stub.py` (solo anfitrión).
- Docs: `docs/network/HTTP.md` (contrato real),
  `PRUEBAS.md`, `LIMITACIONES.md`.
- Verificar en F7-B: tamaño BSS/heap por proceso para
  fijar el tope de cuerpo; `sys_memoria` si hiciera
  falta más RAM que BSS.

## 10. Elementos que no deben modificarse

`main`, stash, ABI/syscalls 52-63, `red_*`, `drv_red`,
`ulib` salvo adición (no reescritura), KATs, asserts
de `t_red`, ISOs, `qrun.py` (comportamiento por
defecto), `dns`/`dhcp`/terminal, `rc` existentes.
Nada de HTTPS/TLS/IPv6/FacilBrowser/sockets.

## 11. Conclusión: VIABLE CON LIMITACIONES

HTTP/1.0 `GET` con `Connection: close` es viable
íntegramente en Ring 3 con los contratos actuales:
apertura/envío/recepción/EOF/cierre/timeouts existen
y están probados para mensajes pequeños; la extensión
a varios segmentos es mecánicamente la misma (bucle +
deadline) [H sólida]. Aplazables: chunked (error),
redirecciones (detectar), keep-alive/pipelining,
cuerpos gigantes (tope + posible streaming a fichero),
HTTPS/TLS (fuera de alcance), navegador.
Riesgo principal antes de un navegador: solo la falta
de prueba multikilobyte [V] y la disciplina de
cerrar/abortar en todas las ramas. Sin cambios de
núcleo previstos.
