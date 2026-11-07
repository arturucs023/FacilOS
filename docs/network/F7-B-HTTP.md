# F7-B — Cliente HTTP/1.0 en Ring 3

Rama `v2.3-red`. Sin cambios de ABI ni syscalls nuevas; Ring3 para
logica HTTP; nucleo de red intacto (se reutiliza tal cual).
Unico cambio de nucleo/arranque: copia alta del catalogo (ver apartado 10),
exigida porque el catalogo supero el hueco VGA (ver apartado 11).

## 1. Estado inicial

- `v2.3-red` @ `9368132` (F6-E documental), sincronizada con
  `origin/v2.3-red`, `main` en `0b0532a`, stash del compositor
  intacto (`stash@{0}`: "compositor: limpieza al soltar y en ACK
  tardio (pendiente verificar)").
- F7-A auditoria aprobada (`docs/network/F7-A-AUDITORIA-HTTP.md`).
- No existia `v2.0/user/apps/http.c` ni `u_http_*` ni stub HTTP.

## 2. Arquitectura

```
URL -> u_http_url -> (IPv4 literal? directo : u_dns_resuelve3)
    -> u_tcp_abrir -> u_http_pide -> u_tcp_enviar (<=536/lamada)
    -> bucle u_tcp_recibir (<=2048/llamada) + u_http_res_datos
    -> EOF (-12) + u_http_res_fin -> cerrar (exito) / abortar (error)
```

- `v2.0/user/lib/ulib.{h,c}`: parser HTTP/1.0 incremental y
  reutilizable (`u_http_res_{inicia,datos,fin}`), resolutor de URL
  (`u_http_url`), constructor de peticion (`u_http_pide`),
  buscador de cabeceras (`u_http_cabecera`) y traedor completo
  (`u_http_trae`). Precedente: codec DNS F6-D.
- `v2.0/user/apps/http.c`: app fina (CLI + presentacion +
  `/httpsal.txt`). Sigue el patron de `dns.c` + `/dnssal.txt`.
- `v2.0/user/apps/terminal.c`: comando `http` (`op_http`,
  CMDS + dispatch), analogo a `consultar_dns`.
- `v2.0/tools/http_stub.py`: stub determinista en anfitrion
  (ver apartado 7). Sin cambios en `qrun.py`: el invitado inicia la
  conexion contra `10.0.2.2:18080` (NAT user-mode, sin hostfwd).
- `v2.0/tools/build32.py`: programa `http` al catalogo; salida
  `FacilOS-v2.3-red-f7b.iso` (ver apartado 13).

## 3. Archivos nuevos y modificados

Nuevos (en el commit F7-B):
- `v2.0/user/apps/http.c` (app CLI).
- `v2.0/tools/http_stub.py` (stub, 20 rutas).
- `v2.0/tools/copyalta.asm` (copia alta del catalogo; ver apartado 10).
- `docs/network/F7-A-AUDITORIA-HTTP.md` (auditoria previa).
- `docs/network/F7-B-HTTP.md` (este informe).
- `v2.0/build/iso/FacilOS-v2.3-red-f7b.iso` (entrega).

Modificados:
- `v2.0/user/lib/ulib.{h,c}` (+75 / +~730): API y parser HTTP.
- `v2.0/user/apps/t_red.c` (+~470): 46 vectores unitarios,
  matriz viva (20 pruebas) y vector 8k determinista.
- `v2.0/user/apps/terminal.c` (+41): comando `http`.
- `v2.0/tools/build32.py` (entrada `http`, salida f7b, aserto
  de cola, inyeccion de `copyalta.bin` en LBA 224).
- `v2.0/src/kernel32/{boot32.asm,cd.c,pag.c}`: solo copia alta
  del catalogo (ver apartado 10). Ni TCP, ni ABI, ni syscalls.

## 4. API HTTP (`u_http_*`, solo cabeceras/GET/close)

- `u_http_url(url, &u)`: 0 ok; -1 args, -2 esquema (nunca
  `https`), -3 host, -4 puerto, -5 largo/forma. Minusculas,
  IPv4 estricta (sin ceros a la izquierda), puerto 1..65535
  (80 por defecto), ruta con `?` (sin `#`), tope 512.
- `u_http_pide(u, extra, sal, smax)`: `GET ruta HTTP/1.0`
  + `Host` (+`:puerto` si != 80) + `Connection: close` +
  `User-Agent: FacilOS32-HTTP/0.1` + `Accept: */*`. `extra`
  validado (sin Host/Connection/CL/TE, sin LF sueltos).
- `u_http_res_inicia/datos/fin`: incremental. `*nbody` =
  total acumulado (sincronizado en cada llamada). CL:0
  completa de inmediato. Excedente (incl. tras completar, en
  la misma llamada o en otra) = error -8. TE (cualquiera,
  incl. chunked) = -7. Malformada/CL no numerico/duplicado
  incoherente = -6. Truncada/sobre-tope = -8.
- `u_http_trae(url, r, cuerpo, cap, espera_ms)`: 0 ok
  (cualquier codigo 100..599 con respuesta valida), -1 args,
  -2 URL, -3 NXDOMAIN, -4 DNS, -5 timeout/red, -6 malformada,
  -7 TE, -8 truncada/excedente/tope. DNS con
  `u_dns_resuelve3` (0 = configurado), TCP con `u_tcp_*` tal
  cual (envio <=536, recepcion <=2048, 0 = timeout,
  -12 = EOF), cerrar-ordenado-en-exito y abortar-en-error
  (sin TCB huerfanos).

## 5. Parser

Estados LINEA/CAB/CUERPO/OK/ERR. Valida version (solo
1.0/1.1), codigo 100..599, linea <=512, cabecera
`token: valor` (nombre letras/digitos/`-`, valor 32..126,
sin continuaciones), tope 8 KiB de seccion y 32 cabeceras,
CL decimal estricto (duplicado igual ok, distinto mal),
cualquier `Transfer-Encoding` rechazado, 1xx/204/304 y
CL:0 sin cuerpo, cierre-delimitado sin CL, EOF valido solo
con cuerpo completo o sin CL. Todo acotado, sin malloc,
sin recursion; el cuerpo vive en buffer del llamador
(tope 65536).

## 6. Limites y timeouts

URL 512; cabeceras 8192/32/linea 512; cuerpo 65536;
global 60 s por defecto (param, tope 120 s); recv por
llamada <= 5 s; 1 conexion por `trae()` (secuencial).

## 7. Servidor de pruebas (`tools/http_stub.py`)

`python tools/http_stub.py [puerto]` (defecto 18080).
20 rutas: `/normal`, `/conlongitud`, `/ocho-kb` (8192 B
`((i*131+7)&0xFF)`, chk=1044480), `/fragmentado`
(byte a byte), `/por-cierre` (sin CL), `/chunked`,
`/mala-cabecera`, `/cl-mal`, `/cl-corto` (CL:100+10B),
`/cl-exceso` (CL:10+20B), `/vacio` (CL:0), `/vacio204`,
`/noencontrado` (404), `/error` (500), `/redir301/302`
(+Location, sin seguir), `/lento` (30 s), `/corte`,
`/corte-cuerpo`, `/eco/<resto>`. Log por conexion
(peticion, ruta, fin).

## 8. Pruebas: unitarias (sin red)

`t_red` sin adaptador: **97 pruebas 0 fallos** (F1+F2+F6-B
intactas + 45 vectores HTTP + 8k-parser):
`http: 8k chk=1044480 n=8192` (parser byte-exacto del
cuerpo de 8192 sobre trozos 1+1500+...; registro en serie).

## 9. Pruebas: matriz viva (stub) y app

`t_red` vivo contra stub (`10.0.2.2:18080`) y app `http`
contra stub (sesiones frescas; `dhcp` previo):
normal/conlongitud/fragmentado/por-cierre/vacio/vacio204/
noencontrado(404+`404 nada`, 9 B)/error(500)/redir301/302
(+Location sin seguir)/eco(`visto: hola`) OK; chunked -7;
mala-cabecera/cl-mal/corte -6; cl-corto/cl-exceso/
corte-cuerpo -8; lento -5 (timeout).
App: mismos resultados + `cuerpo: N bytes en M ms chk=...`
y `/httpsal.txt` verificado byte a byte por serie
(`leer_archivo /httpsal.txt` tras `http .../normal`).

## 10. Prueba critica: cuerpo de 8 KiB

- Stub envia exactamente 8192 B deterministas
  (`8k enviado completo` en su log).
- Invitado (`http http://10.0.2.2:18080/ocho-kb`, sesion
  fresca): `cuerpo: 8192 bytes en 3640 ms chk 1044480`,
  `rc=0`. `trae` solo devuelve 0 con `cuerpo_n == CL`
  exacta (sin perdidas/duplicados/excedente) y el `chk`
  coincide con el esperado (misma formula en stub,
  app y test).
- Parser: `http-8k-parser` OK en QEMU (chk 1044480).
- Recepcion en multiples `u_tcp_recibir` (8192 > 2048 por
  llamada; el stub trocea a 128 B): el parser acumula sin
  confundir fragmentacion TCP con limites HTTP.
- Cierre ordenado en exito (`cerrar`), aborto solo en
  error; sin TCB huerfanos.

## 11. Incidencias del entorno (no son regresiones)

(a) Hueco VGA: la copia del catalogo a 0x30000 pierde lo
que cae en 0xA0000+ (la RAM no retiene). Con 44 programas
(555 KB) la cola (LBA >= 1121) quedaba ilegible:
`tutf8`/`t2_gfx`/editor16/demo16/menu/calculadora/reloj/
adivina no cargaban (-101). Arreglo minimo (sin tocar
red/ABI/syscalls): `boot32.asm` llama a `copyalta.asm`
(LBA 224) que copia la cola a 0x1000000 en irreal;
`cd.c` remapea (`CD_CORTE_LBA` 1121); `pag.c` reserva y
mapea 0x1000000-0x104000 (fuera del heap Ring3, que
empieza en 0x300000); `build32.py` lo inyecta y lo
acota (cola <= 240 KB). Verificacion: los 44 programas
anuncian `F32X` al arrancar y `tutf8` vuelve a 26/0.

(b) QEMU/slirp del anfitrion: tras rafagas TCP sostenidas
el camino invitado<-slirp se interrumpe (paquetes que no
llegan: `Slirp: Failed to send packet`, `rx_err` en el
invitado) y las conexiones nuevas dejan de establecerse
en esa sesion. Reproducido en 5 sesiones (el 8k se queda
en 4480..6440 B de 8192 y el resto de la sesion falla).
El stub demuestra envio completo y el invitado conserva
el prefijo exacto; el parser unitario + app en sesion
fresca demuestran la integridad byte a byte (chk). No se
toca TCP/red por ello (restriccion expresa).

(c) ISP del anfitrion intercepta `example.com` con una
pagina sin cabeceras HTTP: el cliente la rechaza con -6
(comportamiento correcto ante respuesta malformada).
Smoke real con `google.com`: DNS real (216.58.205.46),
`HTTP/1.0 301`, 219 B, chk 17369 identico en ambos
lados, `rc=0` (no vinculante: un solo servidor).

## 12. KAT y regresiones

- KAT al arrancar (con y sin adaptador): `ip OK`,
  `icmp OK`, `udp OK`, `tcp OK` (nucleo intacto).
- `t_red` sin red: 97/0. `t_red` vivo: DNS-stub 24/24 + cache,
  TCP F5 intacto en verde en sesion limpia (C8); la cola
  HTTP/TCP queda supeditada al entorno ((ver apartado 11b).
- v2.2: `t1_lib` 258/0, `tutf8` 26/0, `mem32` 15/0.
- `funcion.f32`: existe como ejemplo
  (`docs/f32-lang/ejemplos/funcion.f32`) pero no hay via
  valida para ejecutarlo en QEMU (sin `.f32` en la ISO ni
  escritor no interactivo); se documenta sin simular
  ejecucion.

## 13. ISO de entrega

`v2.0/build/iso/FacilOS-v2.3-red-f7b.iso` (copia del build
verificado; `f5.iso` restaurada intacta desde git).

## 14. Cierre del commit

- Mensaje: F7-B: implementar cliente HTTP/1.0 en Ring 3.
- Hash: el del commit F7-B en la punta de v2.3-red (verificar con: git log -1 --format="%H %s" v2.3-red).
- git status: limpio tras el commit (salvo el stash preexistente).
- origin/v2.3-red: sincronizado con push sin force (verificado).
- main: en 0b0532a (verificada pre/post, intacta).
- Stash del compositor intacto (verificado pre/post).
- Al cerrar: DETENERSE (no F7-C, ni HTTPS, ni FacilBrowser).
