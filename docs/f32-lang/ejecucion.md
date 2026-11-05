# F32 Lang v0.1 — Motor de ejecución (`f32.c`, parte 6)

Walker sobre el AST con entornos encadenados:

- Globales (≤ 256) y marcos de llamada (≤ 64 locales;
  anidamiento ≤ 64, F32-085). Los parámetros se pasan por
  valor. `retornar` fija `EXV`; caer del final devuelve 0
  (documentado en `funciones.md`).
- Valores: entero (aritmética con `unsigned` intermedio para
  un wrap-around definido), cadena (arena, tope 512, F32-051),
  booleano. Sin conversiones implícitas; cada operador
  revalida tipos en ejecución (defensa en profundidad además
  de la semántica).
- `si/sinosi/sino` en orden; `mientras` reevalúa y cuenta
  iteraciones (tope 100000, F32-070); `y/o` con cortocircuito.
- `escribir` formatea (decimal, `verdadero`/`falso`, texto)
  más salto de línea; sin argumento escribe línea vacía.
  `leer` usa `u_lee_linea` (≤ 255 caracteres) y declara la
  variable como cadena si no existe (F32-054 si existía no
  cadena).
- Bloques con ámbito propio que se restaura al salir (igual
  que la semántica): una variable declarada dentro de un `si`
  no sale de él.
- Errores: primer error aborta con `Error F32-0NN en archivo
  linea N: descripción`; salida 0 ok, 1 error de programa,
  2 mal uso/fichero.
