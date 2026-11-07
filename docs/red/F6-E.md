# F6-E — Consolidación documental de la red (Etapa B, solo D1)

## 1. Estado inicial y final

Inicial: `v2.3-red` @ `9ab7bbe`, sincronizada con
`origin/v2.3-red`, árbol limpio salvo `docs/red/`
(informe F6-E-AUDITORIA.md, sin commitear), `main` en
`0b0532a`, stash del compositor intacto.
Final: mismo HEAD + commit F6-E con solo documentación
y un comentario en `ulib.h`; árbol limpio; `main` y
stash intactos; ISOs sin tocar.

## 2. Archivos documentales modificados

- `docs/network/API-FACILOS32.md` (56: tipo 1 → -2;
  63: solo lectura op 0, sin `trazar_ruta`; ABI vigente
  en F6-D).
- `docs/network/COMANDOS.md` (`mostrar_dns` con caché
  F6-D; `consultar_dns` con origen/transporte/cadena).
- `docs/network/ICMP.md` (`trazar_ruta` marcado como
  reservado, no de F6).
- `docs/network/DNS.md` (tope TTL como 86400 segundos).
- `docs/red/F6-E.md` (este informe; nuevo).
- `v2.0/user/lib/ulib.h` (solo comentario: -8 ventana,
  -6 gracia; autorizado en C).

Sin cambios en `TCP.md` (ya reflejaba errores,
bloqueo, `wait_ms=0`, envío/recepción, cuarentena como
no-`TIME_WAIT` y límites; verificado, no requería
edición).

## 3. Correcciones realizadas

D1-A: syscall 56 documenta ARP (t0) + sonda (t2) y
tipo 1 → `-2 no implementado`; 63 documenta solo
op 0 y elimina `trazar_ruta` como existente.
D1-B: `mostrar_dns` describe caché F6-D; `consultar_dns`
describe salida real (origen/transporte/cadena) y
`/dnssal.txt`; reservados siguen marcados como tales.
D1-C: comentario TCP en `ulib.h` con acepciones F6-C,
verificadas contra `red_tcp.h:37-42` y `red_tcp.c`.
D1-D: sin cambios (verificado completo).
D1-E: `86400 segundos` explícito.
Validación: sin referencias obsoletas (`trazar_ruta`
solo como reservado/futuro, sin `definir rutas`,
sin `ARP/DNS`, sin `F5` vigente); sin comandos
inexistentes presentados como implementados; contratos
contra firmas reales coincidentes; errores TCP según
`red_tcp.h/c`; DNS según `dns.c`/`ulib.c`; sin
contradicciones `docs/red/` vs `docs/network/`
(la auditoría describía el estado previo; la propuesta
aplicada lo resuelve).

## 4. Referencias obsoletas eliminadas

- "ARP/DNS: nombre<->IP" (56).
- "leer/definir tabla de rutas" + "`trazar_ruta`" (63).
- "aviso de ausencia de cache en F4" (`mostrar_dns`).
- "`trazar_ruta`" como trabajo de F6 (ICMP.md).
- "intactas en F5" (vigencia ABI actualizada a F6-D).

## 5. Confirmación: ningún código ejecutable modificado

Diff exclusivo de `docs/**` + comentario en
`ulib.h` (sin efecto en binario: verificado por
inspección del diff; sin cambios en `red_*`, `dhcp.c`,
`dns.c`, `exec.c`, `proc.c`, `sys.h`, drivers, tablas,
límites, colas, KAT, asserts de `t_red`, syscalls, ABI,
comandos ni `rc`). D2 (sc_57 parcial) rechazado según
auditoría. Sin HTTP/HTTPS/TLS/IPv6/FacilBrowser.

## 6. Resultados de las comprobaciones

- Arranque ISO `FacilOS-v2.3-red-f6d.iso` (binario F6-D
  intacto, sin reconstruir): OK, RTL8139 detectada.
- KAT: `red: KAT ip/icmp/udp/tcp OK` con y sin adaptador.
- `t_red` sin red: 52/0 (incluye vectores F6-D).
- Humo con red: DHCP obtener (concesion 86400),
  renovar (re-oferta), liberar ("configuracion
  liberada"); `consultar_dns example.com` resuelve
  (origen red, transporte UDP); `mostrar_interfaz`,
  `mostrar_rutas`, `comprovar_conexion 10.0.2.2`,
  `listar_archivos`, `crear_archivo`, `ejecutar`
  completan la sesión sin cuelgues (salida `pon()`
  del terminal: solo VGA, no serie; preexistente).
- v2.2: `t1_lib` 258/0, `tutf8` 26/0, `mem32` 15/0.

## 7. Resultado real de funcion.f32

NO ejecutado su contenido, con causa documentada (no
se inventa resultado):
- Ausencia probada: el catálogo de la ISO lista 43
  programas compilados, ninguno `.f32`; `build32.py`
  solo empaqueta binarios (`objcopy`), sin ejemplos.
- Sin vía no interactiva para crearlo: `crear_archivo`
  crea vacíos; `eco` no redirige; el `editor` es GUI
  (Ctrl+G etc.); reconstruir la ISO para empaquetarlo
  modificaría ISOs y queda fuera de F6-E.
- Ejecución real del intérprete: `crear_archivo
  prueba.f32` + `ejecutar prueba.f32` → `Error F32-016
  en /prueba.f32: codigo fuera de programa` (diagnóstico
  genuino del intérprete sobre fichero vacío).
- Pendiente: dotarlo de contenido sin GUI o empaquetar
  ejemplos (requiere decisión: modifica ISO).

## 8. Limitaciones pendientes

Las de la auditoría F6-E-A §8 (doc desactualizado ya
corregido salvo lo indicado): convenio `1`-vs-`0` de
espera agotada (compatible, no tocar), `sc_57` aborta
la serie en vez de parcial (no tocar), sin `desligar`
UDP (requiere syscall nueva: no autorizado), caché DNS
por proceso (fría para `dns` monodisparo; persistencia
fuera de alcance), `funcion.f32` sin contenido
ejecutable en invitado, `rx-arp-puerta` flaky
ambiental, `funcion.f32`/f32-lang fuera de F6-E.

## 9. Ruta de la ISO

`v2.0/build/iso/FacilOS-v2.3-red-f6d.iso` (F6-D,
verificada de nuevo en F6-E sin modificar; ISOs
anteriores intactas).

## 10. Commit generado

`F6-E: consolidar documentacion de red` (solo docs +
comentario `ulib.h`).

## 11. Sincronización con origin

Push `v2.3-red` sin force; `origin/v2.3-red` igual que
local; árbol limpio tras commit.

## 12. Estado final del árbol

Limpio. `main` en `0b0532a`. Stash del compositor
intacto (sin aplicar/modificar/eliminar).
