# F7-C-A — Auditoría y diseño de requisitos de FacilBrowser

Fase de auditoría y diseño. No implementa el navegador ni crea código.
Solo estudia el sistema existente y documenta una arquitectura viable.
Rama `v2.3-red`.

## 1. Estado inicial del repositorio

Verificado el 2026-11-08 antes de actuar:

- Rama `v2.3-red`, HEAD `d273384` ("F7-B.1: auditoria de
  estabilizacion y regresion (sin cambios de codigo)"),
  sincronizada con `origin/v2.3-red` (`fetch` limpio, sin divergencia).
- Árbol de trabajo limpio (`git status` vacío).
- `main` en `0b0532a` (intacta).
- Stash preexistente intacto (`stash@{0}`: "compositor:
  limpieza al soltar y en ACK tardio (pendiente verificar)").
- Leído: `docs/network/F7-B1-ESTABILIDAD.md`,
  `docs/network/F7-B-HTTP.md`,
  `docs/network/F7-A-AUDITORIA-HTTP.md`,
  `docs/network/API-FACILOS32.md`, `docs/network/COMANDOS.md`.
- Inspeccionado directamente (solo lectura):
  `v2.0/user/apps/{servidor.c,terminal.c,explorador.c,editor.c,menu.c,configura.c,http.c,wdemo.c,ugfx32.c}`,
  `v2.0/user/lib/{sys.h,ugfx.h,ugfx.c,utext.h,utext.c,tema.h,ulib.h}`,
  `v2.0/src/kernel32/{gfx.c,teclado.h,exec.c,red_tcp.h}`,
  `v2.0/tools/http_stub.py`.

Conclusión: el estado coincide con el obligatorio. Se procede.
Nada de lo afirmado abajo depende de memoria histórica: cada
límite remite a fichero y línea inspeccionados en esta fase.

## 2. Arquitectura gráfica actual

### 2.1 Compositor / escritorio (`v2.0/user/apps/servidor.c`)

- Aplicación Ring 3 (no es núcleo): posee cada ventana (`x,y,w,h`),
  compone fondo + ventanas + barra + menú + cursor, gestiona foco,
  orden Z, arrastre y botones. El cliente solo dibuja su contenido.
- Protocolo IPC por buzones, mensajes de texto `<=64 B`:
  C→S `REG w h titulo / BYE / ACK / SYNC / INF / TICK s /
  EDIT 0|1 / MODAL 0|1 / FONDO n / VIDEO`;
  S→C `ID n / DRAW x y w h / FOCUS 0|1 / KEY code /
  MOU x y bot giro / QUIT / INF W H`.
- Registro: `registra()` centra con desplazamiento y sujeta a
  `PANT_W/AREA_H`. Cromo `CROMO_H 20` (`servidor.c:22`): barra de
  título con cerrar (`w-20`), minimizar (`w-40`), maximizar (`w-60`),
  resto arrastre; agarre de redimensión `14 px`.
- Límites: `NVENT 16` (`servidor.c:15`), `VENT_MIN_W 320 /
  VENT_MIN_H 200` (`servidor.c:261-262`), `titulo[24]`
  (truncado a 23 B + NUL, `servidor.c:36`), barra inferior `30 px`
  (`T_BARRA_H` en `tema.h:53`, `AREA_H = H-30`).
- `TICK` (refresco periódico): el cliente pide `TICK s` (`1-60 s`,
  `0` cancela) y el servidor le manda `DRAW` al vencer
  (`servidor.c:60-61,865`). `MODAL 1/0` suspende
  entrada y pintado para un hijo con pantalla+teclado propios
  (usado alrededor de `u_ejecutar`, ver §3.4).
- `ACK` tardío reanuda y marca sucia; `QUIT` es fire-and-forget.
  Sin `ACK` el servidor no avanza esa ventana (el cliente debe
  confirmar cada `DRAW`).

### 2.2 Framebuffer y modos (`v2.0/src/kernel32/gfx.c`)

- Solo `32bpp XRGB`: `modos_video` = 640x480, 800x600, 1024x768,
  1920x1080 (`gfx.c:320-322`). Arranque persistente a 1920x1080
  si CMOS `0x40==3`; `sys_video_modo(w,h)` devuelve `0 ok,
  -1 sin soporte, -2 sin memoria`; `sys_video_info` devuelve
  `[W,H,paso,prof]`.
- Consola siempre en frente; dorso para componer (`DORSO_BASE`
  2 MB; bloque alto para 1920x1080x32 ≈ 8.3 MB). `u_destino(1)` +
  `u_presentar(x,y,w,h)` vuelcan dorso→frente con clipping.
- Solo el dueño gfx dibuja; `texto` toma lease implícito, el resto
  exige ser dueño (si no, `-1`). UGFX ya gestiona el lease (§3.2).

### 2.3 Fuentes y texto

- Fuente BIOS `8x16` (`UG_FW 8 / UG_FH 16`, `ugfx.h:17-18`).
  Celda fija: todo el layout del sistema son celdas de 8x16.
- Ring 3: `utext.{h,c}` — UTF-8 estricto (`ut_dec/enc/valido/
  largo/anterior/siguiente/compone/nfc/ancho`, `U+FFFD` si
  inválido, `ut_nfc` solo latinos ES/CA). Tablas Latin-1
  verificadas contra `build/font_*.png`; resto `?`.
- Núcleo tolerante por compatibilidad (byte suelto → CP437),
  pero el navegador debe validar en Ring 3 con `ut_*`.

### 2.4 Teclado y ratón

- `teclado.h:8-18`: ASCII + `8/13/9/27`; extendidas `0x101 Arriba,
  0x102 Abajo, 0x103 Izq, 0x104 Der, 0x105 Menú, 0x106 Supr,
  0x107 Inicio, 0x108 Fin, 0x109 RePag, 0x10A AvPa` (réplicas
  `*_U` en `ulib.h:62-72`).
- Modificadores codificados en la tecla (`teclado.h:26-32`):
  `0x200 May, 0x400 Ctrl` sobre `0x101-0x10A`; `Ctrl+letra
  0xE100-0xE119` (`0xE200-0xE219` con May); `Ctrl+Tab 0xE120/
  0xE220`; `May+Tab 0xE121`; `Ctrl+Retroceso 0x7F`.
  Syscall `49` queda como lectura complementaria.
- El servidor filtra `TAB/ESC`: solo llegan al foco si `EDIT==1`;
  si no, `TAB` cicla foco y `ESC` cierra menú/sale. El navegador
  deberá usar `ug_edit(v,1)` para que la barra de direcciones
  reciba `TAB/ESC`.
- Ratón PS/2 con rueda; `sys_raton` bloquea con `v4=[x,y,bot,
  rueda]`; el servidor reparte `MOU x y bot giro` en coords de
  pantalla absoluta; `ug_clic(antes,ahora)` detecta flanco del
  izquierdo. Rueda loteada por ráfaga.

### 2.5 Memoria por proceso (`exec.c:31-32`)

- Heap de usuario `UBRK_INI 0x300000 – UHEAP_FIN 0x480000` =
  **1.5 MB** sbrk por proceso. Código en `0x200000+plano+bss`,
  pila `0x2E0000-0x2EFFFF`, args `<=255 B`.
  Conclusión: el navegador dispone de ~1.5 MB dinámicos + BSS;
  cualquier diseño con páginas/DOM debe caber ahí (ver §5-6).

### 2.6 Qué puede reutilizar una app Ring 3 sin tocar el núcleo

Todo lo anterior es reutilizable sin cambios de núcleo/ABI:
ventanas UGFX, controles, fuentes, teclado/ratón, video info,
heap sbrk, ficheros y red (§3-4). No se necesita framebuffer
directo ni syscalls nuevas.

## 3. Interfaces disponibles desde Ring 3

### 3.1 Syscalls usables (`v2.0/user/lib/sys.h`)

- Salida/gestión: `1 imprimir, 2 ticks, 7 memoria (sbrk),
  17 dormir, 25 ejecutar, 26 terminal, 27 hay_tecla`.
- Ficheros: `3 abrir, 4 leer, 5 escribir, 6 cerrar, 9 crear,
  15 buscar, 16 borrar, 24 truncar`, dirs `41/42/43`, reloj `44`.
- Gráficos lease: `10 pixel, 11 linea, 12 rectangulo, 13 texto
  (len>256 recortado en núcleo), 14 limpiar, 18 destino,
  19 presentar, 20/21 sprite(_t), 22 blit, 23 leer_pixel`.
- Procesos/IPC: `28-33,36-37` info/ps; `34/35 enviar/recibir,
  40 hay`; ratón `38/39`; teclado `46 dist, 49 mod`.
- Video: `45 cmos, 47 modo, 48 info`; energía `50/51`.
- Red: `52 interfaz, 53/54 trama cruda, 55 configurar,
  56 consultar, 63 rutas, 57 comprobar, 58-60 UDP,
  61 TCP multiplexado (op 0..7)` (`sys.h:274-329`).
  `62` reservada. La ABI solo crece, nunca cambia.

### 3.2 UGFX (`ugfx.h:1-106`, impl. `ugfx.c`)

Modelo: el servidor posee la geometría; el cliente dibuja solo
el contenido bajo el cromo al recibir `DRAW` y confirma con
`ACK`. `ug_win_t{srv,id,w,h,x,y,foco,vivo,ox,oy}`,
`ug_ev_t{tipo,x,y,w,h,foco,tecla,rx,ry,rbot,rueda}`,
`UG_EV_NADA/DRAW/FOCO/TECLA/RATON/SALIR` (`ugfx.h:20-26`).

- Sesión: `ug_abre(v,srv,w,h,titulo)` (envía `REG`, espera `ID`),
  `ug_cierra` (`BYE`), `ug_evento(v,ev,bloquear)`,
  `ug_pide_info` (`INF` → pantalla, no ventana),
  `ug_sync` (reframe tras hijo), `ug_tick(v,s)`,
  `ug_edit(v,on)`, `ug_modal(v,on)`.
- Dibujo (coords relativas, solo entre `ug_comienza/ug_fin`):
  `ug_pixel/linea/rect/relleno/texto/limpia`, `ug_comienza`
  (lease + origen `ox,oy=x,y+20`), `ug_fin` (`presentar + ACK`).
  `ug_texto` trocea por `\n`, recorte horizontal en celdas sin
  partir multibyte; sin wrap vertical automático.
- Controles: `ug_dentro, ug_etiqueta, ug_boton (+estado
  OFF/PULSADO/HOVER/NORMAL) + ug_clic (flanco),
  ug_panel, ug_check, ug_campo + ug_campo_tecla
  (UTF-8 por `ut_anterior/siguiente/enc`, ignora
  `0xE100-0xE219/0x7F/ctrl+especial`),
  ug_lista + ug_lista_tecla (arriba/abajo) + ug_lista_clic`.
  Sin scrollbar propio, sin multiline, sin columnas, sin tabs,
  sin hipervínculo: el navegador los compone en Ring 3 (§7).

### 3.3 Identidad visual (`tema.h:1-80`)

Tema "marino" propio: fondo degradado `#1B2A4A→#0C1428`,
ventana `#141C28`, título activo `#2E5FA3`, acento `#3D7DD8`,
selección `#2E5FA3`, botón cara `#3A4A5E`, cerrar `#C0392B`,
texto `#F2F5F9`. Iconos 16x16 propios por letras
(`T_ICON_PAL "KWRGBNYCOPg.b"` + `t_icon_rgb`). El navegador
debe usar esta paleta y dibujar sus propios iconos
(atras/adelante/recargar/detener/ir), sin copiar Chrome/Firefox.

### 3.4 Patrones reutilizables (terminal/explorador/editor)

- Bucle canónico (`terminal.c`, `explorador.c:757-979`,
  `editor.c`, `configura.c:65-235`):
  `ug_abre → ug_edit(1) si hay campo → bucle
  ug_evento(bloqueante) → DRAW (ug_comienza/…/ug_fin) /
  TECLA / RATON (restar cromo) / FOCO / SALIR`.
  Tamaños típicos `440-700 px` (≈50-80 cols x 18-28 filas).
  Redibujo total por `DRAW` (no hay daño parcial).
- Campo de una línea: `ug_campo + ug_campo_tecla`
  (diálogos de explorador/editor; terminal tiene editor propio
  más rico con historial y `RePag/AvPag/rueda*3`).
- Listas + scroll: `ug_lista*` para una columna; scroll
  manual (`sel/scroll`, rueda `±2/3`, clic `(ly-y0)/16+scroll`,
  doble-clic por `ticks<50`) y scrollbar custom (`editor.c`
  con `tam/pos` proporcionales + arrastre).
- Barras: pie de 1-2 líneas (estado/ruta/mensaje, error en
  rojo) + toolbar de iconos 16x16 con hover + tooltip +
  atajos por `TECLA` (no hay aceleradores en núcleo).
- Red en UI: `terminal.c:2112-2145 op_http` — `ug_modal(1) →
  u_ejecutar("http",args) → ug_modal(0) →
  muestra_sal_fich("/httpsal.txt") → rc + mensaje`.
  Mismo patrón sirve para el navegador (fetch sin ensuciar)
  o, mejor, `u_http_trae` directo en el proceso navegador.

## 4. Auditoría HTTP/TCP/DNS

### 4.1 Cómo pide una página la app actual (`http.c:1-291`)

`hcuerpo[U_HTTP_MAX_CPO+1]` (64 KiB + NUL). Flujo `main`:
valida URL (`u_http_url`), exige adaptador (`u_red_interfaz`) e
IP (`u_red_configurar_leer`), `k=u_http_trae(url,&r,hcuerpo,
65536,60000)`, imprime IP/puerto/versión/código/motivo +
`Content-Type/Length/Connection/Location("sin seguir")` +
`cuerpo: N bytes en M ms chk suma` (cuerpo si `es_texto`,
si no `[binario]`), vuelca `/httpsal.txt` en trozos de 512 B.
Códigos `http.c:99-130`: `-2→2` URL, `-3→3` NXDOMAIN,
`-5→1` sin respuesta, `-4→4` DNS/forma, `-6→4` malformada,
`-7→4` no soportada, `-8→4` truncado, otro `→1`.

### 4.2 `u_http_trae` (`ulib.h:416-425`)

`int u_http_trae(url,r,cuerpo,capcuerpo,espera_ms)`:
`cap clamp ≤65536`; `espera 0=60000, tope 120000`;
`fin=ticks+espera/10`. Pasos: `u_http_url` (→-2); `res_inicia`;
DNS si no es IP literal (`u_dns_resuelve3(host,0,0,dr,tmo)`,
`tmo=min(rest*10,30000)`, `-3→-3, -5→-5, otro→-4`);
`u_tcp_abrir(ip,puerto,0)` (→-5); `u_http_pide` + envío en
trozos `≤536` (`-8` ventana → `dormir(100)` + reintento);
bucle `u_tcp_recibir(tb,2048,espera≤5000)`: `>0→res_datos`
(`-7/-8/-6→abortar`), `0→continue`, `-12 EOF→res_fin`,
otro neg→abortar→-5; extra-check post-completo (`recv 64/500`:
`>0→-8`, `EOF→cerrar→0`); éxito siempre `u_tcp_cerrar (FIN)`,
error siempre `u_tcp_abortar (RST)`.

Petición (`u_http_pide`): `GET ruta HTTP/1.0 + Host(+:puerto
si ≠80) + Connection: close + User-Agent: FacilOS32-HTTP/0.1
+ Accept: */* [+extra validado]`. Sin POST/HEAD/keep-alive.

### 4.3 Recepción fragmentada y parser

`u_http_res_{inicia,datos,fin}` (`ulib.h:408-411`), estados
`LINEA/CAB/CUERPO/OK/ERR` (`ulib.h:363-367`), `*nbody`
sincronizado por llamada. Cabeceras en estático `hbuf[8192]`,
consumo byte a byte hasta `CRLFCRLF`; el resto del trozo es
cuerpo (cabecera dividida OK). `≥8192` sin terminador → `-6`.
Línea `≤512`, `token: valor` (`nombre [A-Za-z0-9-] 1..63`,
valor `0..255` trim, `32..126`), sin continuaciones; cualquier
`Transfer-Encoding` → `chunked=1 → -7`. `1xx/204/304` o `CL:0`
→ completo inmediato. Cuerpo con CL: `rest>need / n+rest>cap /
==CL→1` si no `0`; `OK+n>0 → -8`. Sin CL: acumula hasta EOF.
Probado: stub byte-a-byte `/fragmentado`, 8k en trozos 128 B
con `chk 1044480` (F7-B/F7-B.1).

### 4.4 Límites (`ulib.h:356-362`)

`URL 512, SEC 8192, LIN 512, CAB 32, CPO 65536,
NOM 64, VAL 256`. URL: `http://` case-insens (nunca `https`),
host `[A-Za-z0-9.-]` minúsculas sin `.` extremos ni `..`,
IPv4 estricta sin ceros a la izquierda, puerto `1..65535`
(80 defecto), ruta `/…` con `?` (corta en `#`), `ruta[256]`.
Estado: solo `1.0/1.1`, código `100..599`, motivo `32..126`.
`CL` decimal estricto `0..4294967295` (duplicado igual ok,
distinto mal). Todo exceso → `-6/-8`, nunca silencioso.

### 4.5 CL frente a cierre

- Con CL: espera exactamente CL. `CL:0` completa sin cuerpo.
  Corto/exceso/corte → `-8`; `fin()` en `CUERPO+CL` → `-8`.
- Sin CL (cierre-delimitado): acumula hasta `EOF -12`.
  EOF en `LINEA/CAB` o sin cabeceras → `-6` (p. ej. `/lento`).
  `1xx/204/304` nunca tienen cuerpo (bytes extra → `-8`).

### 4.6 Errores y timeouts

`trae`: `0` ok (cualquier código válido), `-1` args,
`-2` URL, `-3` NXDOMAIN, `-4` DNS, `-5` timeout/red,
`-6` malformada, `-7` TE, `-8` truncada/excedente/tope.
Timeouts: global `def 60 s tope 120 s`; `recv` por llamada
`≤5 s`; DNS por `trae ≤30 s`; UDP `3×3 s`; TCP-DNS `500 ms`
por tramo; TCP `abrir ~10 s, cerrar ~5 s, enviar ~12 s`
(`CONEX_ESP/CIERRE_ESP`, `red_tcp.c`).

### 4.7 Cierre, TCB, TCP (`red_tcp.h:13-35`)

`TCP_MSS 536, TCP_RCV 2048, TCP_N 4`. Estados
`CERRADO/ESCUCHA/SYN_TX/SYN_RX/ESTABLECIDO/FIN_TX/FIN_RX`.
Ops syscall 61: `0 ABRIR, 1 ESCUCHAR, 2 ACEPTAR, 3 ENVIAR,
4 RECIBIR (B/0/-12 EOF), 5 CERRAR, 6 ABORTAR, 7 ESTADO`.
Errores: `-6` tabla llena/en uso **y tupla en gracia**,
`-7` timeout/RST-apertura, `-8` estado/ventana (`0` o
`n>wnd`, reintentable), `-9` RST en establecida.
`cerrar` drena+F IN+espera ACK+FIN+ACK final y libera;
`abortar` RST+libera; al morir el proceso, RST solo si hay
ruta+ARP (no bloquea). Cuarentena de tupla `4×6 s`
(no es TIME_WAIT, no consume TCB). Efímero `49152+` salta
tuplas en gracia. Evidencia F7-B.1 §5: 6-7 `http` seguidos
con 4 TCB + secuencias con abortos sin `-6` espurio → sin
fugas; cerrar/abortar liberan para reutilización inmediata.
Restricciones: stop-and-wait (1 segmento sin ACK), sin
opciones, `OOO` descartado (recupera por retransmisión),
sin `2MSL/keepalive`, `half-close` solo en `FIN_TX`.

### 4.8 DNS (`ulib.h:304-352`)

`u_dns_resuelve3(dom,0,0,&r,ms)`: `0` ok + `u_dns_res_t
{ip,ttl,rc,rcode,nrec,saltos,via_tcp,de_cache,cname[256]}`;
`-1..-8` (args/nombre/NXDOMAIN/rcode/timeout/TCP/ciclo/NODATA).
Servidor `0`=configurado, puerto `0`=53, espera `0`=30000
(tope 60000). Caché **32 entradas por proceso**, TTL tope
86400, TTL0 no cacheable, CNAME `≤8` saltos con anti-ciclos,
fallback TCP si `TC=1`. `u_dns_resuelve` = envoltorio 53/15000.

### 4.9 Pedir recursos adicionales

Secuencial, 1 conexión por `trae()`: cada objeto (HTML y luego
cada recurso) = nuevo `u_http_trae(url_abs,&r,buf,cap,timeout)`
con su propio DNS+TCP+`cerrar`. No hay reuso/keep-alive ni
paralelo (con 4 TCB basta secuencializar; 6-7 seguidos
probados). Construir URL absoluta desde `href/src` con
`u_http_url` (mismo host o nuevo; `es_ip` evita DNS; la caché
acelera el resto). Cada recurso necesita su `buf≤65536`;
`CL>cap` → `-8` (límite F7-B, no negociable en esta fase).
`301/302`: solo leer `Location` vía `u_http_cabecera` y emitir
otro `trae()` si la política del navegador lo permite (§5:
`https:` es error terminal, no seguimiento).

## 5. Diseño propuesto del parser HTML mínimo

Objetivo: HTML propio, pequeño y progresivo, en Ring 3, sin
motores externos ni código copiado, sin JS ni CSS completo.
No hay parser previo en el repo (búsqueda `html|href|src=`
vacía): es terreno nuevo, pero el precedente del codec DNS
y del parser HTTP incremental marca el patrón (estricto,
acotado, sin malloc descontrolado, errores explícitos).

### 5.1 Alcance de etiquetas y construcciones

Soportar en la primera versión:

- Estructura: `DOCTYPE` (ignorar, solo detectar),
  `html/head/body`, `title` (para la barra/título ventana).
- Bloques: `h1–h6, p, br, hr, div, span` (span = inline
  neutro), `ul/ol/li` (un nivel; anidado se aplana con
  sangría fija, ver §6).
- Inline: `strong/b` (brillo), `em/i` (color distinto),
  `a href` (enlace), `img src alt` (placeholder, ver §6).
- Texto, entidades, comentarios `<!--…-->` (ignorar),
  `script/style` (ignorar contenido íntegro por seguridad
  y simplicidad: sin JS ni CSS).

### 5.2 Reglas de robustez (permisivo con el contenido, estricto con los límites)

- Desconocidas: ignorar el tag, conservar el contenido
  (salvo `script/style/object/embed` cuyo contenido se salta).
- Mal cerradas: auto-cierre en frontera de bloque (`</p>` sin
  `<p>`, `<li>` sin `</li>`, `<p>` sin cerrar antes de `h1/p/ul`);
  tag sin `>` antes de 512 B o fin de buffer → tratar el `<`
  como texto y seguir (nunca colgarse).
- Incompleto: EOF con pila abierta → cerrar todo y aceptar
  lo acumulado (como el cierre-delimitado HTTP).
- Anidamiento: pila fija (p. ej. 16 niveles); más allá, ignorar
  tags nuevos pero conservar texto. Cruces (`<b><i></b></i>`)
  se resuelven cerrando hasta el coincidente.
- UTF-8: decodificar con `ut_dec`; inválido → `U+FFFD`;
  contar columnas con `ut_anterior/siguiente` (nunca partir
  multibyte, como `ug_campo/ug_texto`).
- Atributos: `nombre[=valor]` con valor entre `"…"`, `'…'` o
  desnudo hasta espacio/`>`; tolerar espacios alrededor de `=`;
  nombres case-insens; solo se interpretan `href/src/alt/title`;
  el resto se ignora. Comillas sin cerrar → hasta `>` o 256 B.
- URLs: absolutas `http://…` vía `u_http_url`; relativas
  (`/ruta`, `pagina`, `../x`, `?q=…`, `#frag` descartado) se
  resuelven contra la URL base (mismo esquema/host/puerto;
  `https:` se conserva como enlace no navegable con aviso).
  `mailto:/javascript:` no se navegan (estado, no error).

### 5.3 Entidades

Subset inicial: `&amp; &lt; &gt; &quot; &nbsp;` +
numéricas `&#NNN; / &#xHHH;` (0..0x10FFFF, si no → U+FFFD).
Desconocida (`&foo;`) se deja literal. Sin DTD ni entidades HTML
completas en la primera versión.

### 5.4 Estrategia incremental sin almacenar páginas enteras innecesariamente

- La página ya vive acotada en `cuerpo≤65536` (límite F7-B).
  El parser no necesita streaming TCP propio: consume ese
  buffer en una pasada con cursor, sin copiar la página.
- Salida a arena fija en heap (1.5 MB, §2.5): lista plana de
  bloques (no árbol DOM con punteros dispersos):
  `struct { tipo, nivel, href_idx|img_idx, ini, len, … }`
  con texto referenciando el buffer (offsets, no copias) +
  tabla de enlaces (URL resuelta `≤512`, texto ancla).
  Tope operativo: p. ej. 512 bloques / 128 enlaces / 64 imgs;
  al superar, se trunca con aviso (como `-8` del HTTP: explícito,
  nunca silencioso).
- Una pasada tokenizer→bloques; segunda pasada solo para
  layout (§6). Sin recursión, sin `malloc` por nodo (arena +
  índices), sin regex. Complejidad lineal en el cuerpo.
- Futura extensión (fuera de F7-C): si una página supera 64K,
  el fetch ya falla `-8`; no se propone streaming a fichero
  para HTML en la primera versión (el fichero `/httpsal.txt`
  demuestra el patrón si algún día hace falta).

## 6. Diseño propuesto del motor de representación

Objetivo: convertir la lista de bloques en filas visibles de
celdas 8x16, con scroll vertical. Sin reproducir un navegador
moderno: mostrar bien páginas sencillas.

### 6.1 Modelo

- Hoja de estilo por defecto cableada (no CSS): márgenes fijos,
  interlineado por líneas en blanco, encabezados por espaciado
  + color (no hay otra fuente: todo es 8x16):
  `h1`: 1 línea en blanco + texto brillo + subrayado con `─`;
  `h2`: blanco + línea; `h3-h6`: amarillo→gris con 1 línea.
  `p/div`: 1 línea en blanco entre bloques. `br`: salto.
  `hr`: fila de `─` al ancho. `li`: `• `/`N. ` + sangría
  `2*nivel` espacios (aplanado más allá de 2 niveles).
- Inline: `strong/b` → blanco brillante (doble pasada del
  mismo glifo si hace falta énfasis); `em/i` → cyan claro;
  `a` → acento `T_ACENTO` + subrayado (línea de píxeles bajo
  la celda) + índice de enlace; `hover/foco` → fondo
  `T_SELECCION`. Texto base `T_VENT_FONDO`-sobre-`#F2F5F9`.
- `img`: caja placeholder `40x40` (borde + `X`) + `[img: alt
  o src corto]`; sin decodificador (no existe en el repo:
  solo PNGs de build/herramientas, ningún `png/jpeg/gif`
  en runtime). Descargar el binario con `trae()` cabe en la
  API, pero decodificar queda fuera del alcance mínimo (§13).
- Ancho: `cols = ancho_contenido / 8`; wrap por palabras con
  medida en caracteres (`ut_*`), nunca partir palabra si cabe
  en línea, palabra más larga que la línea se corta por celdas.
  Recorte como `ug_texto` (UTF-8-safe, `≤250` chars/llamada).

### 6.2 Viewport y scroll

- Área de contenido = ventana menos cromo (20) menos toolbar
  menos barra de estado (ver §7). Todo se redibuja por `DRAW`
  (patrón del sistema): layout previo a filas (`y` acumulada
  en px, `16 px`/fila), viewport con `desplaza` en px o filas,
  `ini = nfilas>vis ? nfilas-vis-desplaza : 0` (patrón
  `terminal.c/explorador.c`), rueda `±2/3` líneas, `RePag/AvPa`,
  `Inicio/Fin`, clic en scrollbar custom (`editor.c:1859-73`:
  `tam/pos` proporcionales, arrastre) o lista lateral.
- Contenido que excede: scroll vertical obligatorio;
  horizontal no (el wrap lo evita). Sin paginación ni zoom en
  la primera versión.

### 6.3 Recursos y coste

Todo en heap 1.5 MB: cuerpo 64K + bloques (~32 B × 512 ≈ 16K)
+ enlaces + filas layout (recalculable por `DRAW`, no
persistente). Cabe con margen; sin dorso propio (usa el del
sistema vía `ug_comienza/ug_fin`).

## 7. Diseño de la interfaz

Identidad propia FacilOS32 (tema marino, iconos propios de
letras, sin clonar Chrome/Firefox/Edge). Ventana UGFX típica
`640x460` (como el explorador) con este layout:

```
+------------------------------------------------+
| Barra título (servidor): "FacilBrowser - título"|
+------------------------------------------------+
| [<] [>] [R] [X] | [dirección............] [Ir] |  ~44 px
+------------------------------------------------+
| contenido (página renderizada, scroll)          |
| ...                                            |
+------------------------------------------------+
| estado: URL | código/tamaño/tiempo | msg/error  |  1-2 líneas
+------------------------------------------------+
```

- Barra de direcciones: `ug_campo` (una línea, UTF-8, cursor,
  `max 512` = `U_HTTP_MAX_URL`), foco con borde amarillo,
  `Enter`=ir, `ESC`=limpiar/restaurar (requiere `ug_edit(1)`).
- Atrás/adelante: `ug_boton` + estados hover/pulsado; historial
  lineal en memoria (p. ej. 16 URLs + scrollpos por entrada;
  adelantar se descarta al navegar de nuevo, como pila del
  explorador `hatras/hadel`).
- Recargar: re-`trae()` la URL actual. Detener: bandera
  cooperativa entre trozos de `recibir` (la syscall bloquea
  hasta 5 s por llamada: el stop real solo puede acortar el
  bucle, no preempcionar la llamada en curso — documentar).
- Indicador de carga: texto `Resolviendo…/Conectando…/
  Recibiendo N bytes…` en la barra de estado + `ug_tick(1)`
  para animar `…` sin bloquear el bucle (patrón `configura.c`).
- Contenido: §6; clic en enlace (`ug_dentro+ug_clic`) o
  `Enter` sobre enlace enfocado (`Tab` cicla enlaces cuando
  `EDIT==1`); hover cambia color; URL no-http muestra aviso
  en estado (no cuelgue).
- Barra de estado/errores: `URL | HTTP/1.0 cod motivo |
  N bytes en M ms chk | msg`; errores con el vocabulario de
  `http.c/terminal.c` (`sin respuesta / URL invalida /
  NXDOMAIN / no valida o no soportada`) + `rc`.
- Scroll: rueda/teclas/scrollbar (§6.2). Sin tabs, sin menús
  desplegables, sin diálogos salvo aviso centrado (`ug_panel`).
- Encaje: `ug_abre → bucle ug_evento → DRAW/TECLA/RATON`;
  fetch directo con `u_http_trae` en el proceso (mejor que
  `u_ejecutar("http")` + fich, aunque ese patrón queda como
  fallback); `ug_modal` alrededor de resoluciones largas si
  hace falta no ensuciar; `ug_tick` para reloj/carga.

## 8. Estudio de SearXNG

SearXNG es un servicio externo (metabuscador agregador), no un
motor a implementar en el SO. Se analizan dos integraciones.
Fuentes: `https://docs.searxng.org/dev/search_api.html`
(endpoints, parámetros, formatos) y settings por defecto.

### 8.1 Formato de petición y respuesta

- Endpoints: `GET /`, `GET /search`, `POST /`, `POST /search`.
  `GET` con query params, `POST` con form
  (`application/x-www-form-urlencoded`).
- Parámetros: `q` (requerido), `categories`, `language`,
  `pageno` (def. 1), `time_range`, `safesearch`, `theme`,
  `format` (opcional: `json|csv|rss`).
  Ejemplos doc: `GET /search?q=searxng&format=json`.
- Respuesta HTML (defecto): página del tema `simple` con form
  + lista de resultados (título/url/contenido por resultado).
  Respuesta JSON/CSV/RSS: solo si el admin la activó en
  `search.formats` de `settings.yml`; si no, **403 Forbidden**.
  Doc literal: "Requesting an unset format will return a 403
  Forbidden error. Be aware that many public instances have
  these formats disabled."
- Config por defecto documentada: `search.formats: [html]`
  (solo HTML). JSON es opt-in por instancia, no garantizado.
  Reportes de usuarios confirman que JSON viene deshabilitado
  por defecto y hay que buscar instancias que lo activen.

### 8.2 Opción A — navegar a la interfaz HTML

- Petición: `GET /search?q=<query>&language=<lang>&pageno=<n>`
  — encaja con nuestro `GET` + `Connection: close` en
  principio (query URL-encoded en `ruta`, `≤512` total).
- Respuesta: HTML moderno del tema `simple` (CSS + JS
  opcionales para filtros; el núcleo de resultados es HTML).
  Obstáculos para F7-C: (1) **HTTPS**: las instancias
  públicas son `https://` y nuestro `u_http_url` rechaza
  `https` (`ulib.h:392-394`, sin TLS en el repo — `TLS.md`
  es esqueleto); (2) tamaño/complejidad: página de
  resultados real supera con holgura el subset §5 y roza el
  tope 64K/8K-cabeceras/32-cabeceras, con probable
  `Transfer-Encoding: chunked` (→ `-7` terminal, no
  silencioso); (3) dependencia externa no elegible
  arbitrariamente (disponibilidad, limiters, `link_token`
  en instancias públicas).
- Veredicto A: viable **solo** contra instancia propia en
  `http://` LAN con HTML sencillo; inviable contra instancias
  públicas desde FacilOS32 actual.

### 8.3 Opción B — SearXNG como buscador predeterminado (endpoint + UI propia)

- Petición: `GET /search?q=<query>&format=json[&language…]`.
  Respuesta: JSON con `title/url/content/engine` por resultado.
  Ventaja: evita parsear HTML ajeno; render propio (§6-7).
- Requisitos: (1) instancia con `json` en `search.formats`
  (opt-in, 403 si no); (2) `http://` alcanzable (sin TLS);
  (3) **parser JSON nuevo en Ring 3** (no existe; fuera de
  esta fase por restricción — "no implementar un cliente
  JSON ni un buscador en esta fase").
- Veredicto B: arquitectónicamente preferible a medio plazo,
  pero exige instancia propia + parser JSON futuro. No es la
  primera integración.

### 8.4 Veredicto

- No elegir instancia pública arbitraria ni asumir
  disponibilidad: todas son efectivamente HTTPS y con JSON
  deshabilitado por defecto.
- Lo único viable con la infraestructura actual es una
  **instancia propia autoalojada en HTTP plano** (p. ej. host
  LAN vía puerta slirp), con `html+json` habilitados, usada
  primero vía HTML simple (A) y más tarde vía JSON (B).
- F7-C **no puede depender** de HTTPS ni de JSON público.
  Si SearXNG necesita HTTPS o funciones no soportadas, lo que
  lo impide en la primera versión es exactamente: sin TLS →
  sin `https://`; sin parser JSON → sin `format=json`; topes
  64K/8K/32 + sin chunked → páginas grandes/complejas fallan
  con `-7/-8/-6` explícitos. El informe lo deja documentado;
  la hoja de ruta (§12) relega SearXNG a última fase (F).

## 9. Matriz de compatibilidad

| Capacidad | Estado | Evidencia / motivo |
|---|---|---|
| HTTP `GET` 1.0 `close` | Viable ahora | `u_http_trae`, 19/19 rutas + 8k chk (F7-B.1 §4) |
| Enlaces (`<a href>`) | Viable ahora (Ring 3) | Resolver con `u_http_url` + nuevo `trae()`; `Location` ya legible vía `u_http_cabecera` |
| Historial atrás/adelante | Viable ahora (Ring 3) | Pila en memoria + `ug_boton`; patrón `hatras/hadel` del explorador |
| Descarga secuencial de recursos | Viable ahora | 1 `trae()` por recurso; 6-7 seguidos probados con 4 TCB |
| DNS + caché | Viable ahora | 24/24 vivo, 32 entradas, CNAME≤8, TC→TCP |
| HTML subset (§5) | Viable con ampliación Ring 3 | No existe parser: hay que crearlo (F7-C-B); cabida en 1.5 MB |
| Layout texto + scroll | Viable con ampliación Ring 3 | Celda 8x16 + `ug_*` + patrones scroll; hay que programarlo (F7-C-C) |
| Interfaz navegador (URL/hist/estado) | Viable con ampliación Ring 3 | `ug_campo/boton/lista/panel` existen; componerlos es F7-C-D |
| Barras/scroll/tabs propios | Viable con ampliación Ring 3 | Solo custom (como editor/explorador); no hay widget |
| `img` placeholder + `alt` | Viable ahora (sin decode) | Caja + texto con `ug_rect/texto` |
| Imágenes decodificadas | Requiere infra | Sin decodificador en el repo (solo PNGs de build); exige codec nuevo + posible subida de topes |
| CSS | Requiere infra | Sin motor; solo hoja por defecto cableada en primera versión |
| JavaScript | Requiere infra (descartado) | Sin motor ni planes; `script` se ignora por diseño |
| Formularios/POST | Requiere infra | Solo `GET`; `POST`/codificación es trabajo futuro |
| Cookies | Requiere infra | Sin jar ni cabeceras persistentes |
| HTTPS/TLS | Requiere infra | `u_http_url` rechaza `https`; `TLS.md` esqueleto; sin cripto/red nueva |
| SearXNG HTML público | Requiere infra (TLS) | Instancias públicas `https://` → error terminal hoy |
| SearXNG JSON | Requiere infra | Exige instancia con `json` + parser JSON Ring 3 nuevo |
| Páginas modernas (web real) | Requiere infra | HTTPS+JS+CSS+chunked+>64K las excluyen por diseño en v1 |

## 10. Limitaciones técnicas

1. Red: solo `http://`, solo `GET`, `close` siempre, sin
   redirección automática, sin chunked (`-7`), cuerpo `≤64K`
   (`-8`), cabeceras `≤8K/32/512` (`-6`), 1 conexión por
   `trae()`, 4 TCB, stop-and-wait 536/2048, timeouts §4.6.
2. Página: tope 64K, sin streaming a fichero en v1; HTML
   mayor/complejo falla explícito (no parcial silencioso).
3. Gráficos: celda fija 8x16, sin fuentes escalables/negrita
   real (énfasis por color/subrayado), sin imágenes
   decodificadas, ventana `≥320x200`, `≤pantalla-30`,
   `NVENT 16`, título `≤23 B`, IPC `≤64 B`, `texto ≤256`/llamada.
4. Memoria: 1.5 MB heap; diseños con arena fija y offsets,
   sin DOM disperso.
5. Entrada: `TAB/ESC` solo con `EDIT==1`; stop cooperativo
   (no preempciona `recibir≤5 s`); sin portapapeles del sistema.
6. Entorno pruebas: slirp pierde en ráfagas (F7-B.1 §9),
   headless deforma color (t2_gfx), harness deforma
   `() , + " =`; SearXNG público inalcanzable sin TLS.

## 11. Riesgos y dependencias

- R1. Páginas reales >64K o chunked → `-8/-7` sistemáticos.
  Mitigación: subset + instancia/páginas de prueba propias;
  documentar el fallo como límite, no como bug.
- R2. Servidor que no cierra (sin CL) → timeout global.
  Mitigación: deadline + stop cooperativo + estado visible.
- R3. RST con cuerpo completo (`-9` interno → `-5`): el diseño
  futuro debe decidir si cuerpo-completo+RST cuenta como éxito
  con aviso (precedente F7-A R2); en v1 se reporta error.
- R4. Cuarentena de tupla 6 s: navegar rápido al mismo host
  con puerto efímero distinto no choca (salta tuplas), pero
  reintentar la misma URL al instante puede dar `-6`: la UI
  debe espaciar reintentos y explicar el código.
- R5. Fugas: cada `trae()` debe cerrar/abortar en todas las
  ramas (precedente F6-D/F7-B); el navegador multi-fetch es
  el primer cliente intensivo del TCB.
- R6. Seguridad: parser permisivo-con-límites (nunca adivinar
  más allá de topes); `script/style` ignorados; `https:`,
  `javascript:`, `mailto:` no navegables; sin cookies → sin
  sesiones (menor superficie, también menor compatibilidad).
- Dependencias por fase (§12): B no depende de gfx; C depende
  de B + `ug_*`; D depende de C + red viva; E depende de D;
  F depende de instancia propia + (B..E). Sin núcleo/ABI/
  syscalls/compositor/build en ninguna.

## 12. Hoja de ruta de implementación

División orientativa ajustada a dependencias (no obligatoria,
pero recomendada). Cada fase: objetivo testeable, sin núcleo,
reutilizando interfaces, con aceptación propia. No empezar
F7-C-B automáticamente (esta fase cierra aquí).

- **F7-C-B — infra + parser HTML mínimo.** Objetivo: librería
  Ring 3 (`u_html_*` en `ulib`, patrón codec DNS/HTTP) con
  tokenizer + bloques + entidades + resolución URLs + tests
  sin gfx (volcado a texto/fich, como `t_red` 45 vectores).
  Aceptación: vectores unitarios (tags §5, entidades,
  comentarios, mal cerradas, UTF-8, 512 bloques/128 enlaces)
  + páginas del stub.
- **F7-C-C — representación de texto y elementos.** Objetivo:
  layout a filas 8x16 + viewport + scroll en visor mínimo
  (read-only, sin chrome). Aceptación: páginas stub con
  h1-h6/p/listas/enlaces/hr/br render correctos, wrap,
  scroll rueda/teclas/scrollbar, recorte UTF-8-safe.
- **F7-C-D — interfaz gráfica y navegación.** Objetivo:
  ventana `640x460` §7 (URL, atrás/adelante, recargar,
  detener cooperativo, estado, historial 16). Aceptación:
  navegar 3 páginas enlazadas, atrás/adelante/recargar,
  error visible por código, sin fugas TCB (N fetches
  seguidos), `DRAW/ACK` sin cuelgues.
- **F7-C-E — imágenes (placeholder), historial y recursos
  adicionales.** Objetivo: caja `img+alt`, historial
  persistente en fich, fetch secuencial de recursos con la
  misma `trae()` (sin paralelo), política `Location` explícita.
  Aceptación: página con N imgs muestra N placeholders,
  historial sobrevive reinicio, recursos >64K fallan `-8`
  con mensaje.
- **F7-C-F — integración SearXNG.** Objetivo: contra
  instancia propia `http://` LAN (html primero, json cuando
  exista parser): query → lista → navegar. Aceptación: buscar
  y abrir un resultado; documentar 403/HTTPS como errores
  terminales si la instancia no cumple.

Primera fase que permite abrir una página sencilla desde
FacilOS32: **F7-C-C** (tras B). B sola trae y vuelca texto;
la primera página *visible* exige B+C. D la hace navegable.

## 13. Alcance mínimo del primer navegador funcional

Ventana única `~640x460`, `http://` + `GET` + `close`; subset
§5 (sin JS/CSS/forms/cookies); layout §6 (texto, enlaces,
listas, `hr/br`, `img` como placeholder+alt); chrome §7
(dirección, atrás/adelante, recargar, detener cooperativo,
estado/errores, scroll, historial 16 en memoria); página
`≤64K`; secuencial; errores con `rc` de `http.c`; identidad
marina propia. Explícitamente fuera: `https`, JS, CSS,
POST, cookies, imágenes decodificadas, tabs, descargas
binarias, SearXNG público/JSON.

## 14. Pruebas de aceptación propuestas

Unitarias (estilo `t_red`, sin red): tokenizer por etiqueta
§5, entidades (5 nombradas + numéricas + desconocida),
comentarios, `script/style` ignorados, desconocidas,
mal cerradas/cruces, pila 16, UTF-8 válido/inválido,
atributos con/ sin comillas, URLs absolutas/relativas
(`/`, `../`, `?`, `#`), topes (512 bloques/128 enlaces,
URL 512, hnom 64/hval 256) con truncado avisado.
Vivas (stub `10.0.2.2:18080`, sesiones frescas + `dhcp`):
`simple.html` (todos los tags), `malformada.html`,
`entidades.html`, `grande.html` (>64K → `-8`),
`404/500`, `redir301` (muestra `Location`, no sigue),
navegación por 3 enlaces, atrás/adelante/recargar/stop,
historial, scroll hasta fin, `chk` cuerpo como en F7-B.
SearXNG (F): solo contra instancia propia `http://` con
`html+json`; 403/HTTPS documentados como terminales.
Criterio: sin pánicos, sin fugas TCB (N fetches seguidos),
`DRAW/ACK` sin cuelgues, árbol/`main`/stash intactos.
