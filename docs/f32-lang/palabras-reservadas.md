# F32 Lang v0.1 — Palabras reservadas

Toda palabra de esta lista no puede usarse como identificador
(error F32-011). El lexer las reconoce en minúsculas exactas;
`Si`, `SI` = identificador inválido... no: al no ser reservada
exacta, `Si` sería un nombre válido pero confuso; se prohíbe
por estilo: cualquier identificador que iguale una reservada
sin importar mayúsculas = error F32-011 (regla simple y
predecible).

| Palabra | Categoría | Función |
|---|---|---|
| programa | estructura | inicio del programa |
| fin | estructura | final del programa |
| escribir | E/S | mostrar una expresión |
| leer | E/S | leer línea a variable cadena |
| variable | declaración | declarar variable con valor |
| si | condición | abrir condición |
| sinosi | condición | alternativa con condición |
| sino | condición | alternativa final |
| finsi | condición | cerrar condición |
| mientras | bucle | abrir bucle condicional |
| finmientras | bucle | cerrar bucle |
| funcion | función | declarar función |
| finfuncion | función | cerrar función |
| retornar | función | devolver valor y salir |
| y | operador | conjunción lógica |
| o | operador | disyunción lógica |
| no | operador | negación lógica |
| verdadero | literal | booleano cierto |
| falso | literal | booleano falso |

Notas:

- `retornar` existe porque las funciones devuelven valor;
  sin él, caer del final devuelve `0` documentado.
- `y/o/no` como palabras (no símbolos) mantienen todo el
  lenguaje en castellano legible.
- `#` no es palabra: es comentario de línea (ver sintaxis).
- Futuro (NO v0.1, solo reserva de idea): `para`, `romper`,
  `continuar`, `decimal`, `importar`. No usarlos como
  identificadores se recomienda pero no es error en v0.1.
