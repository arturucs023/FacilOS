# F32 Lang v0.1 — Informe final A–O (FacilOS32 v2.2, rama `v2.2-f32lang`)

Base: `v2.2-editor@29eb475`. Sin tocar `main`. Sin red. Sin v2.3. F32 versiona aparte (v0.1).

## A. Objetivo
Intérprete Ring3 de lenguaje propio en castellano (F32 Lang v0.1), ejecutable desde terminal (`ejecutar *.f32`) e indentación automática en editor. `.f32x` solo reservado.

## B. Requisitos respetados
- Seguir en v2.2; F32 Lang versiona aparte (v0.1).
- No red, no compilador nativo, no v2.3.
- Respetar especificación aprobada (`docs/f32-lang/` FASE 2); cambios de sintaxis justificados: `no` liga menos que comparaciones, `escribir` sin arg = línea vacía, CRLF tolerado.
- Probar en QEMU todo lo declarado. Git en rama propia, sin `main`, sin force push.
- Documentar solo lo implementado.

## C. Decisiones de lenguaje
Tipos v0.1: entero (32 bit, wrap definido vía `unsigned` intermedio), cadena (tope 512), booleano. Sin decimales (futura coma fija). Sin conversiones implícitas; condiciones booleanas estrictas. Funciones nivel superior `funcion nombre(a,b)`, llamada `nombre(args)`, `retornar`; sin retorno explícito devuelve `0`; recursión tope 64. `no a == b` ≡ `no (a == b)`.

## D. Arquitectura
Monolito Ring3 `v2.0/user/apps/f32.c` (~17.5K plano): lexer por líneas, parser descendente, AST (4096 nodos), semántica y walker. `tools/build32.py`: entrada `("f32", ["f32.c"])`. Límites: fuente 32K, 4096 líneas, 8192 tokens, 256 globales / 64 locales, 64 funciones, 8 params, 64 llamadas, 100000 iters, cadenas 512B. Salidas: 0 ok, 1 error programa, 2 mal uso. Formato error: `Error F32-0NN en archivo linea N: descripcion`.

## E. Lexer (ver `lexer.md`)
Por líneas (CRLF tolerado: `\r` excluido). Tokens `T_RES` (18), `T_ID`, `T_ENT` (desborde F32-031), `T_CAD` (UTF-8 validado), `T_OP`. Ignora blancos y `#`. Cada token conserva línea.

## F. Parser y AST (ver `parser.md`)
`p_prog` exige `programa` primero, `fin` final. Bloques por indentación (>0, múltiplos 4; tab=F32-022). Precedencia `p_y > p_no > p_cmp > p_sum > p_mul > p_un > p_prim`. Comparaciones no encadenables (F32-034).

## G. Ejecución (ver `ejecucion.md`)
Walker con entornos encadenados, ámbito de bloque restaurado. `y/o` cortocircuito. `mientras` con cuota. `escribir` + salto; `leer` (≤255, declara cadena). Primer error aborta.

## H. Memoria (ver `memoria.md`)
Sin estáticos grandes. Heap `u_malloc` comprobado: fuente 32K + índices + tokens 128K + símbolos + marcos 164K + arena AST 384K ≈ 420K típico, peor ≈840K < 1.5M. Regla Ring3: abrir antes de reservar (ver I).

## I. Núcleo — hallazgo y fix (ver `hallazgo-kernel.md`)
Causa de `F32-010` en línea 1 válida y de escritura perdida en editor: `fds_init()` en `sc_03`/`sc_09` (`v2.0/src/kernel32/exec.c`) reseteaba `brk=0x300000` e invalidaba `malloc` previos (`SRC==LINI` observado) y vaciaba `fds[]` (copia con multi-apertura salía vacía, verificado con `ORIGINAL` → `mostrar_contenido` vacío). Fix mínimo: eliminar ambas llamadas; `proc_crea` ya inicializa `brk`/`fds`. Verificado: editor reescribe tras guardar, copia conserva contenido, regresión verde.

## J. Integración terminal (ver `integracion-terminal.md`, `pruebas-f32.md`)
Rama `.f32` en `op_ejecutar` (`v2.0/user/apps/terminal.c`): abs vía `a_absoluta`, prueba `+.f32` sin extensión, `rc=N` y «Programa finalizado correctamente.» si rc 0. F32X intacto (`ejecutar menu`). Ayuda actualizada.

## K. Integración editor (ver `integracion-editor.md`)
Solo indentación v0.1, sin coloreado: auto-indent +4 tras apertura, dedent autoritario al completar cierre, Backspace y Shift+Tab (`0xE121`, espejo `0xE120`/`0xE220`; defines `TEC_*_TAB*` en `teclado.h`/`ulib.h`) por niveles. `.f32` como «Texto» en explorador.

## L. Pruebas funcionales QEMU (serie + capturas `v2.0/build/images/f32_*`, `ed_*`, `term_*`)
Verificación final 05/11/2026 sobre `FacilOS-v2.2-f32.iso` (copia de terminal, 1517568 B):
- `ejecutar /hola.f32` → `Hola desde F32 Lang`; `ejecutar /variables` (sin ext) → `Artur, 19, 25, Suma:, 44`; `ejecutar /funcion.f32` → `5` y factorial 120 (serie: `Hola, mundo! / Bienvenido a FacilOS32 / Artur / 19 / 25 / Suma: / 44 / Hola desde F32 Lang / 5`).
- Bucle 0-4, `condicion`, `leer` con eco, `e17` (multi-`sinosi`, `mientras` 0 iters, `<=/==`, `==` cadenas, `no falso`), `e18` (retorno 0 + `escribir` vacía).
- Errores con archivo+línea y rc=1 (re-verificado hoy): `Error F32-042 en /e03.f32 linea 3: variable inexistente`; `Error F32-050 en /e16.f32 linea 3: division o resto por cero`. Batería e01–e26: F32-001/002/012/013/015/020/022/033/034/040-044/050-054/060/061/083/084/085/070; `F32-080` duplicada y `F32-081` aridad incluidos. Modos `-t`/`-p` OK.

## M. Integración y regresión
Terminal: `rc=0` + mensaje final en captura; `ejecutar menu` intacto. Editor: indent + dedent + Shift+Tab + Backspace + `.f32` Texto (capturas). Regresión misma ISO: t1_lib 258, tutf8 26, mem32 15, disco32 38, ipc32 111, equidad32 3 verdes; t2_gfx 12/7 idéntico en base limpia `14a3b74` (preexistente entorno, no regresión).

## N. Límites / no hecho / futuro
Sin coloreado, sin depurador, sin `.f32x` (reservado), sin red, sin decimales. Topes en §D. CRLF solo tolerado, no generado.

## O. Entrega
Rama `v2.2-f32lang` (remoto `a2f50d9`, local cierra FASE 3-4 + fix + docs). Archivos: `user/apps/f32.c`, `terminal.c`, `editor.c`, `explorador.c`, `kernel32/exec.c`, `teclado.c/.h`, `lib/ulib.h`, `docs/f32-lang/` (12 spec + `lexer/parser/ejecucion/memoria/hallazgo-kernel/pruebas-f32` + 5 ejemplos), ISO `v2.0/build/iso/FacilOS-v2.2-f32.iso` (validada: arranca y ejecuta `.f32`). Históricas intactas.
