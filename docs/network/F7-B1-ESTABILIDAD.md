# F7-B.1 — Estabilización y regresión (auditoría sin cambios)

Fase breve de estabilización posterior a F7-B. Objetivo: verificar
que HTTP, el nuevo cargador de cola alta y el resto del sistema
funcionan tras la integración. Sin implementar funcionalidad.
Sin modificar núcleo, ABI, syscalls, red, stub, ni build.
Rama `v2.3-red`.

## 1. Estado inicial (verificado 2026-11-08, antes de actuar)

- Rama `v2.3-red`, HEAD `335353d` ("F7-B: implementar cliente
  HTTP/1.0 en Ring 3"), sincronizada con `origin/v2.3-red`
  (`fetch` limpio, sin divergencia).
- Árbol de trabajo limpio (`git status` vacío).
- `main` en `0b0532a` (intacta).
- Stash preexistente intacto (`stash@{0}`: "compositor:
  limpieza al soltar y en ACK tardio (pendiente verificar)").
- Leído: `docs/network/F7-B-HTTP.md` (informe F7-B completo),
  `docs/network/F7-A-AUDITORIA-HTTP.md` (viabilidad),
  `v2.0/src/kernel32/{boot32.asm,cd.c,pag.c,exec.c}`,
  `v2.0/tools/{build32.py,copyalta.asm,http_stub.py}`,
  `docs/network/{TCP,DNS,API-FACILOS32,PRUEBAS,COMANDOS.md}`.

Conclusión ETAPA 0: el estado coincide con lo esperado.
Se procede. No se asume nada no probado directamente.

## 2. Auditoría del nuevo mapeo de memoria (ETAPA 1)

Mecanismo F7-B (revisado línea por línea, sin ejecutar
cambios): la cola del catálogo (LBA >= 1121) no cabe bajo
el hueco VGA (0xA0000 no retiene), así que el arranque la
copia a 0x1000000 con `tools/copyalta.asm` (inyectado en
el LBA 224 del área del núcleo, con `assert kb <= 223*512`
y `assert len <= 512` en `build32.py`); `cd.c` remapea
(`CD_CORTE_LBA` 1121, base 0x1000000, tope 256 KB);
`pag.c` reserva y mapea identidad supervisor
0x1000000-0x1040000.

Verificación punto por punto:

1. Tamaños coherentes: cola real (1336-1121)*512 =
   110080 B < 262144 (reserva y mapeo). Margen ~2.3x.
2. Sin colisiones: heap de usuario 0x300000-0x480000
   (`UBRK_INI`/`UHEAP_FIN` en `exec.c`), núcleo
   0x100000-0x200000, pilas anillo 0 + pool FS
   0x480000-0x640000, heap/dorso/tablas 0x800000-0xD00000,
   pilas de procesos 0xE00000+, dorso grande top-down
   (23 MB útiles según E820; 16 MB cabe con margen).
   El `assert kb <= 223*512` impide que el núcleo invada
   el sector de copyalta.
3. Mapeo con permisos correctos: identidad + PRESENTE +
   ESCRITURA, sin bit USER. Los directorios nuevos lo
   heredan, pero Ring 3 no puede tocarlo.
4. Reserva coherente: `reserva()` marca DUENO_RESERVA y
   `pag_mapa_nuevas()` solo entrega marcos libres, así
   que esos marcos nunca se asignan a procesos ni al
   heap de usuario. La ventana de copia (`pag_ventana`)
   no los toca.
5. Sin dependencias frágiles con el tamaño de QEMU:
   `reserva()` recorta a `PAG_RAM_MAX`; con menos de
   ~16.25 MB de RAM la zona no existiría y el mapeo
   leería vacío (degradación documentada, no cuelgue).
   QEMU usa 32 MB (23 MB útiles).
6. copyalta.asm autocontenida y correcta: no-op si no
   hay cola (`jbe` tras `sub`); CHS 18 spt/2 caras igual
   que `leer`; rebote 0x8000 libre en arranque; GDT
   propia 4 GB con base lineal correcta para CS=0x2000;
   irreal canónico; preserva registros y vuelve con
   `retf`; error de lectura señalizado por serie ('3')
   sin bloquear el arranque.
7. Documentación (`F7-B-HTTP.md` apartados 2,3,11)
   describe el mecanismo con fidelidad.

Nits solo-comentario (no se tocan, sin efecto funcional):
(a) `copyalta.asm:3` dice `call 0x1000:0x1BE00` pero el
call real es `0x2000:0xBE00` (misma dirección lineal
0x2BE00; el `org` y la GDT usan correctamente la forma
0x2000); (b) comentario en `pag.c` ("2-3 MB y 2-4.75 MB")
solapado. Prioridad baja/cosmética; se dejan como están.

Conclusión ETAPA 1: el mapeo es correcto y coherente.
No se encontró ningún defecto. No se requiere ni se
aplica ningún cambio.

## 3. Catálogo: las 8 apps antes rotas (ETAPA 2)

ISO `FacilOS-v2.3-red-f7b.iso` en QEMU i386 sin red
(`qrun.py`, ESC al shell). Las 8 cargan (F32X en el
catálogo), se ejecutan y se cierran sin bloqueos:

| App | Prueba | Resultado |
|---|---|---|
| tutf8 | ejecución completa | 26/0, rc=0 |
| t2_gfx | ejecución completa | 9/12, rc=3 (ver nota) |
| editor16 | abrir `prueba.txt`, ESC, salir | "Cancelado", rc=0 |
| demo16 | menú + `q` | "fin demo16.", rc=0 |
| menu | render + ESC + eco posterior | shell responde, sin cuelgue (ver captura) |
| calculadora | abrir + `q` | rc=0 |
| reloj | abrir + `q` | rc=0 |
| adivina | prompt + `q` | rc=0 |

Nota t2_gfx: los 3 fallos son lecturas de color
(`banda0-negro`, `banda1-azul`, `transparente-fondo`).
Pasan todas las pruebas de lógica, sprites sin
transparencia, blit y animación, con salida limpia
(`t2: 12 pruebas 3 fallos`, rc=3, prompt recuperado).
v2.2 ya documenta estos tests como sensibles al entorno
de display (1 frente a 3 fallos según entorno); en
headless (`-display none`) se reproducen 3. No indica
regresión del cargador: la app carga, corre y cierra
con normalidad.

Captura `menu.ppm` (800x600): menú renderizado íntegro
("FacilOS - menu", 9 entradas, ayuda de teclas), tomada
con la app en ejecución antes de ESC.

Muestra no afectada: `t1_lib` 258/0 y `mem32` 15/0
(ver apartado 7); `terminal`/`dns`/`dhcp`/`http` usan
las mismas syscalls ya probadas (F7-B + apartados 4-5).

## 4. Matriz HTTP vía app (ETAPA 3, stub 10.0.2.2:18080)

Sesiones QEMU frescas (`dhcp` + peticiones `http`,
stub con log por conexión). 19/20 rutas con el
comportamiento especificado:

| Ruta | Resultado |
|---|---|
| /normal | 200, 13 B, chk 1161, rc=0 |
| /conlongitud | 200, 16 B, chk 930, rc=0 |
| /fragmentado | 200, 14 B, chk 1423, rc=0 (1640 ms: reensamblado byte a byte) |
| /por-cierre | 200 sin CL, 15 B, chk 1436, rc=0 |
| /noencontrado | 404, 9 B ("404 nada"), rc=0 |
| /vacio | 200 CL:0, 0 B, rc=0 |
| /vacio204 | 204, 0 B, rc=0 (~5.6 s: extra-check + cierre) |
| /mala-cabecera | rc=4, "malformada" (-6) |
| /cl-mal | rc=4, "malformada" (-6) |
| /lento | rc=4 a ~30 s (EOF sin cabeceras -> -6; acotado, sin cuelgue; el timeout global -5 lo cubre t_red con 5 s) |
| /chunked | rc=4, "no soportada" (-7) |
| /corte | rc=4, "malformada" (-6) |
| /corte-cuerpo | rc=4, "truncado" (-8) |
| /cl-corto | rc=4, "truncado" (-8) |
| /cl-exceso | rc=4, "truncado" (-8) |
| /redir301 | 301 + Location, 5 B, chk 433, rc=0 ("sin redirigir") |
| /redir302 | 302 + Location, 5 B, chk 433, rc=0 |
| /eco/hola | 200 "visto: hola", 11 B, chk 1075, rc=0 |
| /error | 500, 6 B ("falla"), chk 522, rc=0 |
| /ocho-kb | 200, 8192 B, **chk 1044480**, rc=0 (ver nota) |

Nota 8k: en sesión fresca con el camino limpio, el stub
envía los 8192 B deterministas y el invitado los recibe
íntegros (chk idéntico en ambos extremos, rc=0). El
parse incremental multi-llamada queda probado de extremo
a extremo. Ver apartado 9 para el comportamiento bajo
ráfagas sostenidas.

El log del stub confirma cada petición bien formada
(`GET /ruta HTTP/1.0`) y cierre limpio por conexión;
su stderr quedó vacío (sin excepciones).

## 5. Limpieza de TCB (ETAPA 3, segunda parte)

El sistema declara 4 TCB simultáneos. Evidencia de que
cerrar/abortar liberan y las conexiones se reutilizan:

- Sesión con 6 `http` seguidos (normal, vacio,
  eco/hola, noencontrado, conlongitud, redir301):
  los 6 con rc=0 y cuerpo/código correctos. Con solo
  4 TCB, 6 conexiones seguidas correctas exigen
  liberación y reutilización (sin ella, la 5ª fallaría).
- Sesión con 7 seguidas (añade vacio204): igual, 7/7.
- Secuencias con abortos (`-7` chunked, `-6`
  mala-cabecera/cl-mal/corte, `-8` cl-corto/cl-exceso/
  corte-cuerpo, timeout /lento) seguidas de peticiones
  normales correctas: ningún `k=-6` espurio de tabla
  llena, ningún cuelgue ni conexión residual.
- `t_red` vivo usa la misma tabla por proceso para sus
  20 pruebas HTTP; los fallos observados son siempre
  k=-5 con nbody=0 y c=0 (sin SYN del invitado en el
  stub), nunca -6 de tabla llena; y `tcp-escuchar`
  (que también necesita un TCB) pasa en la misma
  sesión (ver apartado 7).
- Conclusión: sin fugas de TCB; cerrar/abortar liberan
  para reutilización inmediata.

## 6. funcion.f32: pendiente, documentado (ETAPA 4)

- Existe: `docs/f32-lang/ejemplos/funcion.f32` (234 B):
  `saludar()` imprime "Hola desde F32 Lang";
  `suma(a,b)` retorna a+b; principal llama, calcula
  `suma(2, 3)` e imprime.
- El intérprete `f32` carga y responde (bare `f32` ->
  `uso ...`, rc=2, sin cuelgue).
- Sin vía válida de ejecución en esta fase:
  (a) la ISO no incluye ningún `.f32` y añadirlo
  alteraría el proceso de construcción;
  (b) la única vía en tiempo de ejecución sería
  `crear_archivo` + 14x `escribir_archivo --agregar`
  (el lexer exige `\n` por línea) + `f32`, pero la
  transcripción exacta es inviable: probado con
  `eco () , + " = abc`, el harness dejó `)=  *` y el
  resto reventado en comandos sueltos (evidencia en
  serie). Caracteres imprescindibles del lenguaje
  (paréntesis, coma, `+`, comillas, `=`) no llegan
  íntegros.
- No se ejecuta un archivo vacío como prueba, no se
  inventa resultado, no se crea inyector. Queda pendiente.

## 7. Regresiones generales (ejecutadas, no reutilizadas)

Sin adaptador (boot sin red):
- KAT: `ip OK`, `icmp OK`, `udp OK`, `tcp OK`.
- `t_red`: 97 pruebas 0 fallos (incluye parser HTTP
  8k chk=1044480 y vía negativa sin adaptador).
- `t1_lib`: 258/0. `tutf8`: 26/0. `mem32`: 15/0.
- `f32` sin args: uso, rc=2 (carga OK).

Con adaptador (user-net 10.0.2.2, stub DNS/TCP/HTTP):
- KAT: `ip OK`, `icmp OK`, `udp OK`, `tcp OK`.
- `t_red`: 188 pruebas, 32 fallos, `retorno: 1`
  (desglose abajo). Sin pánicos ni excepciones.
- DNS vivo: 24/24 contra stub (directo, cache-hit,
  alias-full/cadena/texto, ciclo, 8/9-saltos, nxdomain,
  nodata, servfail, refused, tc->TCP, multi, ttl0,
  ttl0-nocache, ttl-tope, timeout, otro-srv 5354,
  srv-cache, 33-miss, llena, desalojo, conserva) +
  UDP/DNS/ARP/config/ping previos verdes.
- HTTP vivo y suite TCP: ver apartado 9 (entorno).

Desglose de los 32 fallos (todos k=-5, nbody=0, c=0
salvo 8k con n=5504 parcial): 19 pruebas HTTP vivo
(8k + 18 de matriz) y 13 TCP (abrir-eco, dos-abrir,
abrir-fin, rechazada-rapido, negro-rechazado,
aviso-listo, aceptar, abrir-silencio, tupla-abrir,
tupla-gracia, tupla-reuso, abrir-aborto, h-arg).
Verdes en la misma sesión: todo DNS, `http-vivo-timeout`
(-5 esperado con espera 5 s), `tcp-escuchar`,
`tcp-cerrar-escucha`, polls, validaciones de args,
contadores (tx_ok=640 rx_ok=281 tx_err=0 rx_err=5).

## 8. Defectos encontrados (clasificados)

- P0/bloqueante: ninguno.
- P1 (requiere cambio de código): ninguno.
- Observaciones (no son defectos del repo, no se tocan):
  1. El hilo TCP del stub DNS en 5354 no puede bindear
     (lo posee mDNSResponder del anfitrión); su UDP
     funciona y es lo único que usa el test
     (`otro-srv` no necesita TCP: respuesta A simple).
  2. `t2_gfx` 3/12 en color bajo `-display none`
     (display, no cargador: v2.2 ya lo documenta como
     sensible al entorno; carga/ejecuta/cierra bien).
  3. Comentarios desactualizados/solapados
     (`call 0x1000:0x1BE00` en copyalta.asm;
     "2-3 MB y 2-4.75 MB" en pag.c): cosmético.
- Pendiente (no defecto): `funcion.f32` (ver apartado 6).

## 9. Limitaciones del entorno de pruebas

(a) QEMU/slirp pierde paquetes en ráfagas sostenidas
hacia el invitado ("Slirp: Failed to send packet",
rx_err>0) y, tras un stall con datos pendientes, deja
de entregar tráfico nuevo en esa sesión. Reproducido:
el stub envía los 8192 B completos ("8k enviado
completo") y el invitado se queda en n=5504 con
k=-5; a partir de ahí ningún SYN sale (cero llegadas a
ambos stubs) y todo lo posterior falla con k=-5
(incluida suite TCP). Es el mecanismo ya documentado
en F7-B; esta fase lo re-confirma sin cambios.
No se toca red/TCP por ello (restricción expresa);
tampoco se reintentan envíos para enmascararlo.
(b) El harness de teclado deforma `() , + " =`
(evidencia: eco de sondeo), lo que impide transcribir
`.f32` multilineas exactos (ver apartado 6).
(c) Sin display no hay verificación visual salvo
capturas puntuales (menu verificado en imagen).

## 10. Conclusion

Estable con limitaciones conocidas del entorno; no se
requieren correcciones. Sin cambios de código en F7-B.1:
el cargador de dos regiones queda auditado y verificado
en QEMU (44/44 F32X, tutf8 26/0, regresiones v2.2 en
verde); el cliente HTTP responde según especificación
en 19/19 rutas cuando el camino entrega; la suite
DNS/UDP/ARP/config/ping pasa íntegra con red; los
fallos observados son del entorno (slirp/display),
clasificados y reproducibles, sin indicio de defecto
nuevo. No iniciar F7-C.
