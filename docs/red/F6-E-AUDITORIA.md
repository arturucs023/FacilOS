# F6-E-A — Auditoría de interfaces y contratos de red

Solo lectura. Sin cambios de código, sin commits. Rama `v2.3-red`
@ `9ab7bbe`, sincronizada con `origin/v2.3-red`, árbol limpio,
`main` en `0b0532a` intacta, `stash@{0}` del compositor intacto.
Inspeccionados: `v2.0/user/lib/{ulib.h,ulib.c,sys.h}`,
`v2.0/src/kernel32/{exec.c,proc.c,red*.{h,c},drv_red.h}`,
`v2.0/user/apps/{dhcp.c,dns.c,t_red.c,terminal.c}`,
`docs/network/*`. Tick = 10 ms (`irq.h:17`).

## 1. Estado real de cada servicio

- L2/RTL8139 (F1): PIO, Tx 1 en vuelo, anillo RX 16 KiB,
  sondeo (sin IRQ útil). Sano; KAT + `t_red` 7/0 en vivo.
- ARP/IPv4 (F2): caché 16, caducidad 60 s, resolvedor con
  reintento 0,5 s; rutas derivadas (conectada + defecto).
- ICMP (F3): solo eco; KAT + eco real a puerta.
- DHCP (F3, Ring3 crudo): obtener / renovar (INIT-REBOOT si
  hay IP, si no obtener) / liberar (descubre, release si hay
  servidor, limpia local siempre). Lease BE32 bien
  interpretado pero solo informativo; sin temporizador de
  renovación (documentado F6-B).
- UDP (F4): 4 ligas x cola 4 x 512 B, desalojo de vacía más
  antigua, sin `desligar`.
- DNS (F6-D, Ring3 en `ulib`): A/IN, CNAME ≤8 + anti-ciclos,
  caché 32 por proceso (TTL tope 1 día, 0 = no cachear,
  clave nombre+servidor+puerto, sin negativos), fallback
  TCP (prefijo BE16, tope 4096 B, lecturas parciales, RST
  tras respuesta completa). `dns` es CLI fino; `consultar_dns`
  ejecuta `dns` y muestra `/dnssal.txt` (borrado tras
  mostrar). `u_dns_resuelve3` (deadline global, defecto
  30 s, tope 60 s) y `u_dns_resuelve` (15 s, puerto 53).
- TCP (F5/F6-B/F6-C): 4 TCB + backlog 1, stop-and-wait
  536 B, RTO 500 ms x2 (tope 4 s, 4 reintentos), ventana
  2048 B, cuarentena de tupla 6 s (sin TIME_WAIT con slot),
  limpieza por pid con RST no bloqueante.

## 2. Interfaces públicas existentes

Contrato ABI (nunca cambia de número): 52-61 y 63 en uso,
62 libre (`exec.c:1821`, `sys.h:15`, `SC_N`=64; 62 sin
`sc_` asociada devuelve -1 genérico).

Ring3 (`ulib.h/c`, envoltorios finos sobre `sys_llama`):

| Interfaz | Firma | Retorno / errores |
|---|---|---|
| `u_red_interfaz` | `(red_if_t *f)` 32 B | 0, -1 sin adaptador. No bloquea. |
| `u_red_trama_enviar` | `(marco, tam)` 14..1514 | tam, -1/-2/-3. No bloquea (Tx acotada). |
| `u_red_trama_recibir` | `(marco, max, espera_ms)` | >0 tam, 0 agotada, -1/-2. Bloquea hasta `espera` (tope 10 s). |
| `u_red_configurar_leer` | `(red_cfg_t *c)` 16 B | 0 / negativo. No bloquea. |
| `u_red_fijar` | `(campo 1..4, valor)` | 0 / negativo. No bloquea. Fijar IP≠0 emite ARP gratuito. |
| `u_red_consultar` | `(tipo, ptr)` t0 ARP 12 B / t2 sonda 8 B | t0: 0+MAC, 1 agotada; t2: 0 enviado; -1/-2; t1 DNS: -2 no implementado. t0 bloquea (reintentos ARP). |
| `u_red_rutas` | `(buf, max)` rutas 12 B | nº bytes, 0 vacío / -2. Solo op 0 (leer). No bloquea. |
| `u_red_comprobar` | `(pet 12 B, resp, max)` n=1..10 | nº bytes (6/eco), -1/-2/-3/-4/-5. Bloquea n×espera (100..10000 ms/eco). |
| `u_red_udp_ligar` | `(puerto)` 0=efímero | puerto, -1/-2/-6 (ajeno o tabla llena con pendientes). No bloquea. Reserva liga del proceso; sin `desligar`. |
| `u_red_udp_enviar` | `(e 8 B, datos, len≤1472)` | len, -1..-5. No bloquea (ARP interno ≤3 s). Requiere liga previa. |
| `u_red_udp_recibir` | `(pet 4 B, buf, max≥8)` | bytes (cabecera 8+datos), 0 agotada, -1/-2/-6. Bloquea hasta espera (tope 10 s). Consume un datagrama. |
| `u_tcp_abrir` | `(ip, puerto, local)` | id>0, -1/-2/-3/-4/-5/-6/-7/-9. Bloquea ~10 s. Reserva TCB. |
| `u_tcp_escuchar` | `(puerto)` | id>0, -6 en uso. No bloquea. Reserva oyente. |
| `u_tcp_aceptar` | `(id, espera_ms)` | hijo>0, 0 agotada, -8/-2/-6. Bloquea hasta espera; 0 = poll (un bombeo, sin dormir). |
| `u_tcp_enviar` | `(id, datos, len≤536)` | len, -2/-8 (estado o ventana)/-5/-7/-9. Bloquea hasta ACK (~12 s). Siempre bloqueante (sin espera en ABI). |
| `u_tcp_recibir` | `(id, buf, max≤2048, espera)` | bytes, 0 agotada, -12 EOF, neg. Bloquea hasta espera; 0 = poll. Consume stream. |
| `u_tcp_cerrar` | `(id)` | 0, neg. Bloquea ~5 s (drena 3 s; con hueco: RST + -7). Libera siempre. |
| `u_tcp_abortar` | `(id)` | 0 (sticky: libera). No bloquea (RST). Libera inmediato. |
| `u_tcp_estado` | `(id)` | 1..6, neg. No bloquea. |
| `u_dns_armar` | `(dom, xid, sal, max)` | long≥12, -1 args, -2 dominio, -3 sin sitio. No bloquea. LDH estricto (sin `_`, sin guion en borde=>-2 por etiqueta). |
| `u_dns_leer/leer2` | `(m, n, xid, preg, npreg, ips, ttls, maxips, rcode[, cname, cnmax, cttl])` | #A, -1 malformada (incl. Z≠0, puntero a cabecera, basura final, NAME ajeno rechazado=>0), -2 no nuestra, -4 TC. No bloquea. |
| `u_dns_resuelve3` | `(dom, srv 0=config, pto 0=53, r, espera 0=30 s, tope 60 s)` | 0 + `u_dns_res_t` (ip/ttl/rc/rcode/nrec/saltos/via_tcp/de_cache/cname[256]); -1..-8 (args, nombre, NXDOMAIN, rcode, timeout/red, TCP, ciclo/profundidad, NODATA). Bloquea acotado por deadline. 1 liga UDP por proceso (reutilizada). |
| `u_dns_resuelve` | `(dom, servidor, *ip, *ttl)` | Como resuelve3 (pto 53, 15 s). |
| `u_dns_cache_{limpiar,n,viva}` | — | Gestión/test de la caché por proceso. No bloquean. |
| `u_ejecutar/u_ticks/u_dormir/u_mi_pid` | — | Hijo+espera / ticks / bloqueo ms / pid. Solo `u_ejecutar` bloquea (hasta fin hijo). |

Internas de núcleo relevantes: `red_tcp_reloj()` (RTO en cada
`red_bombea`), `proc_fusiona()` → `*_limpia_proceso(pid)`.

## 3. Tabla de errores

Capa syscall (núcleo, estable desde su fase):
`-1` sin adaptador (52/53/54/57/58/60/61), `-2` args,
`-3` Tx agotada (53) / sin ruta (IP) / sin IP local (57),
`-4` ARP agotado, `-5` fallo Tx, `-6` en uso/tabla llena
(58/61; 61 también tupla en gracia F6-C), `-7`
timeout/conexión (61; cerrar con hueco), `-8` estado
inválido (61; incluye ventana cerrada F6-C y aceptar no
escucha), `-9` abortada/sticky (61), `-12` EOF (61),
`1` espera ARP agotada (56 t0 — positivo, no negativo),
`0` espera agotada (54/60/61-aceptar/recibir).
Ring3 DNS (`u_dns_resuelve3`, no ABI): 0, -1 args,
-2 nombre, -3 NXDOMAIN, -4 rcode (en `r.rcode`),
-5 timeout/red, -6 TCP, -7 ciclo/profundidad, -8 NODATA.
App `dns` los traduce a rc 0/1/2/3/4/5 (compatibles F4).

Incompatibilidades confirmadas (documentación, no código):
(a) `API-FACILOS32.md:56` dice "ARP/DNS", pero tipo 1
devuelve -2 (`exec.c:1376-1378`); (b) `API-FACILOS32.md:63`
y `COMANDOS.md` sugieren definir rutas y `trazar_ruta`,
pero sc_63 solo lee (op 0) y `trazar_ruta` no existe;
(c) `ulib.h:272-273` documenta errores TCP -1..-9 sin las
acepciones F6-C (-8 ventana, -6 gracia); (d) convenio
`1` (ARP, espera agotada) frente a `0` (resto) para el
mismo concepto; (e) `COMANDOS.md:mostrar_dns` dice "sin
cache" pero `terminal.c:2119-2121` ya describe la cache
F6-D. Estrategia: NO cambiar códigos (compatibilidad);
corregir solo textos.
Descartado cambiar: `1` vs `0` de ARP (rompería
`t_red`/apps), `sc_57` (devolver parcial en vez de
abortar la serie alteraría `comprovar_conexion`).

## 4. Tabla de bloqueos y tiempos de espera

Bloqueantes acotados (núcleo monohilo, IF=0 en syscall;
despiertan por PIT o datos; ningún `while(1)` sin tope):
54 (≤10 s), 56-t0 (≤10 s + reTx 0,5 s), 57 (hasta
10 ecos × 10 s ≈ 100 s peor caso; `comprovar_conexion`
usa n≤10, espera 1000 ms), 60 (≤10 s), 61-abrir (~10 s),
61-enviar (~12 s), 61-recibir/aceptar (hasta espera),
61-cerrar (~5 s + drenar 3 s), DNS (deadline 30/60 s;
UDP 3×3 s por nombre; TCP parcial con tope 500 ms por
tramo), DHCP (3×4 s + 3×4 s por fase), `u_ejecutar`
(hasta fin hijo), `u_dormir` (ms pedidos).
No bloqueantes: 52, 53 (Tx con espera activa acotada),
55, 56-t2, 58, 59 (salvo ARP ≤3 s), 63, 61-escuchar/
abortar/estado, armar/leer/cache.
Poll real (sin dormir, verificado KAT dt≤1 tick y en
vivo): `aceptar/recibir` con espera 0. Cuasi-poll:
consultar ARP con espera 0 (~1 tick si no hay cache).
`u_tcp_enviar` no admite espera (ABI): siempre bloquea
hasta ACK o error; documentado, sin cambio propuesto.
Retransmisiones TCP con límites (5 envíos, backoff ≤4 s);
DNS respeta deadline global; DHCP agota intentos y
devuelve 1 (nunca pendiente).
Diferencias fijadas: bloqueante = espera con tope y
reloj activo; no bloqueante = retorna tras como mucho
un bombeo; poll = espera 0 sin dormir; timeout = 0/1/-7
según llamada (documentado arriba); no existe
cancelación asíncrona (solo `abortar` para TCP y
expiración de esperas).

## 5. Gestión de recursos por proceso

Crear: ligar UDP ocupa 1 de 4 ligas (+cola 4×512 B);
abrir/escuchar TCP ocupa 1 de 4 TCB (+snd 536 B,
rcv 2048 B); DNS liga 1 UDP por proceso (reutilizada).
Esperando: el proceso queda bloqueado con rodajas +2
ticks; sus datos viven en estructuras del núcleo.
Pendientes: colas UDP, rcv/snd TCP, tap global (4×1516,
FIFO con descarte + contador), anillo RX 16 KiB.
Finalizar (normal/error): `proc_muere` marca FIN; en
cada IRQ `proc_fusiona` limpia IPC + UDP (todo, incluso
pendientes) + TCP (RST si ruta+ARP en caché, si no
silencio; CERRADO+sticky también; cuarentena de tupla;
`gen` impide alias) + devuelve marcos/pila/VMA.
Sin `kill` en ABI: la terminación desde sistema no
existe como primitiva (solo fin propio o falta).
Huérfanos analizados: hijo pendiente de escucha muerta
se libera sin RST ni cuarentena (el par reintenta y
obtiene RST de puerto cerrado: convergente); tap/ARP/
contadores globales sobreviven (por diseño, con
contadores de descarte); gracia de tupla expira sola
(4 entradas, desalojo de la más antigua: best-effort).
Sin fugas confirmadas en F6-B/F6-C/F6-D (KAT D2 +
`limpia==4` + reutilización probadas).

## 6. Problemas encontrados

P1. Doc `API-FACILOS32.md` 56/63 y `COMANDOS.md`
`mostrar_dns` desactualizados (apdo. 3). Solo texto.
P2. `ulib.h` TCP no documenta acepciones F6-C de -6/-8.
Solo texto.
P3. `mostrar_interfaz` imprime `TTL: 64` literal
(`terminal.c:1696`): es el defecto, no estado vivo.
Solo texto (aclarar etiqueta).
P4. `consultar_dns` admite un solo argumento
(`terminal.c:2079`): sin selección de servidor/puerto
(para el stub solo vía API directa en `t_red`).
Comportamiento, no defecto; documentar.
P5. Sin `desligar` UDP: proceso longevo que ligue muchos
puertos agota las 4 ligas hasta morir. Requiere syscall
nueva: NO se propone (evicción + reutilización cubren
los usos actuales; cambio con autorización explícita).
P6. Caché DNS por proceso + `dns` monodisparo =
caché casi siempre fría para `consultar_dns` (cada
invocación es un proceso nuevo). Arquitectura, no error:
beneficia a procesos longevos y a `t_red`/futuras apps.
Se documenta; persistencia en fichero o demonio quedan
fuera de F6-E (requieren diseño y autorización).
P7. `sc_57` peor caso ~100 s (10×10 s). Acotado pero
largo; el comando usa 1×1 s por defecto. Documentar;
no cambiar (alteraría `comprovar_conexion`).
P8. `trazar_ruta`, `mostrar_conexiones`, `solicitar_web`
no existen (COMANDOS.md los marca reservados; solo un
comentario en `red_icmp.h:6` menciona F6). No crearlos
en F6-E (prohibido por el alcance).
P9. `rx-arp-puerta` flaky ambiental (SLIRP pierde la
trama de un solo disparo; la vía núcleo con reintentos
pasa). Seguir aislado, no contarlo.
P10. `u_dns_resuelve` simple fija puerto 53 y 15 s:
adecuado para uso normal; el stub necesita
`resuelve3`. Sin cambio.

## 7. Riesgos de regresión

Tocar `exec.c`/`red_*.c`/`ulib.c` puede romper KAT,
`t_red` (152 pruebas + stub), DHCP/DNS vivo y v2.2.
`ulib` estático por proceso (~14 KB BSS: caché, visores,
bufs DNS/TCP): cambiar tamaños mueve la memoria de
todas las apps (re-verificar arranque + KAT). El orden
de evaluated `fin` en esperas usa aritmética con signo
(wraparound a 497 días: correcto por diseño). No
aumentar tablas para "probar" (ocultaría presión real:
4 TCB/ligas y gracia 6 s ya se ejercitan al límite en
`desalojo`, `tupla-gracia`, `cache-llena`).

## 8. Propuesta de cambios mínimos (requiere aprobación)

D1 (docs, riesgo nulo): corregir `API-FACILOS32.md`
(56 tipo 1 → -2; 63 solo lectura; 62 libre),
`COMANDOS.md` (`mostrar_dns` con caché F6-D),
`ulib.h` (acepciones -6 gracia / -8 ventana) y
`TCP.md`/`DNS.md` si falta el convenio 1-vs-0 y el
peor caso de `sc_57`. Sin tocar código.
D2 (opcional, se desaconseja): `sc_57` con resultados
parciales en vez de abortar la serie. Cambia
comportamiento observable: NO aplicar sin orden
expresa; la auditoría recomienda mantenerlo.
Sin propuesta de código obligatoria: el sistema es
coherente y compatible; el resto son límites
documentados, no defectos.

## 9. Pruebas necesarias (tras aprobación)

Si solo D1: relectura de docs + `grep` de
consistencia + arranque KAT + `t_red` sin red
(pendiente: confirmar 52/0 con los cambios F6-D) y
una pasada viva de humo (DHCP + `consultar_dns` +
`mostrar_*`). Si D2: además serie `-c 3` con eco
forzado a fallar a mitad y `t_red` completo.
En todo caso, antes de cerrar cualquier Etapa B:
KAT UDP/TCP/IP/ICMP/ARP con y sin adaptador,
`t_red` completo, DHCP obtener/renovar/liberar, DNS
A/CNAME/TC, TCP activo/pasivo/cierres, v2.2
(258/0, 26/0, 15/0) y `funcion.f32` ejecutado de
verdad con resultado documentado.

## 10. Elementos que no deben modificarse

`main`, stash del compositor, números y semántica de
syscalls 52-63 (62 queda libre), drivers
(`drv_red`/RTL8139), núcleo `red_*` (salvo bug
reproducido con autorización), KAT y sus expectativas,
asserts de `t_red` (no debilitar para pasar), tablas y
límites, ISOs anteriores, nombres/comportamiento de
comandos existentes, `rc` de `dns`/`dhcp`.
