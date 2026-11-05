# F32 Lang v0.1 — Pruebas (QEMU, `qrun.py`)

Método: programas `.f32` en imagen de disco de prueba
(construidos con `inyecta.py` desde `docs/f32-lang/ejemplos/`
más casos de error) + `ejecutar f32 ...` y `ejecutar ....f32`
en terminal; capturas e inspección de serie/ISO.

Funcionales (todos OK en QEMU):
- `hola`, `variables` (tipos, `+` concatena, suma entera),
  `condicion` (si/sinosi/sino), `bucle` (0..4),
  `funcion` (sin args, con params, retorno, llamada en
  expresión: 120 con factorial de 5).
- `leer` (eco + asignación), recursión con tope (F32-085),
  cuota de bucle 100000 (F32-070), tope de cadena 512
  (F32-051), división por cero (F32-050), multi-`sinosi`,
  `no` con comparación (`no a == b` ≡ `no (a==b)` → 10, 30),
  anidados, `escribir` sin argumento (línea vacía), bucle de
  0 iteraciones, retorno por defecto 0.
- Errores con archivo+línea: F32-001 (inexistente), 002, 012,
  015, 022 (indentación), 033, 034, 040-044, 050-054, 060,
  061, 083, 084, 085, 070. Códigos de salida 0/1/2.
- Modos `-t` (volcado de tokens) y `-p` (validación sin
  ejecutar, «N funciones, programa valido»).

Integración (OK con captura):
- Terminal: `ejecutar /hola.f32` y `ejecutar /variables`
  (sin extensión) → salida en ventana + `rc=0` + «Programa finalizado
  correctamente.»; `ejecutar menu` (F32X) intacto. Captura Ring3
  vía `/f32sal.tmp` (sin pipes en ABI): vale para `ejecutar /x.f32`
  y `ejecutar f32 /x.f32`; errores `F32-0NN` también salen en ventana.
- Editor: auto-indent en `.f32` (+4 tras apertura), dedent de
  cierres al escribirlos, Shift+Tab y Backspace por niveles,
  `.f32` como «Texto» en el explorador, apertura con ruta.

Regresión (misma ISO): t1_lib 258, tutf8 26, mem32 15,
disco32 38, ipc32 111, equidad32 3 (verdes); t2_gfx 12/7
idéntico a la base limpia (preexistente del entorno).

Límites: programas ≤ 32768 B, 4096 líneas, 8192 tokens, 4096
nodos, 256 globales / 64 locales, 64 funciones, 8 params,
llamadas ≤ 64, iteraciones ≤ 100000, cadenas ≤ 512 B.
