# F32 Lang v0.1 — Sintaxis

## 1. Líneas y bloques

- Una instrucción por línea. Sin `;` ni continuaciones.
- Indentación: múltiplos de 4 espacios. Tabuladores = error
  léxico (el editor inserta espacios y nunca tabs).
- Tras `programa`, `si`, `sinosi`, `sino`, `mientras`,
  `funcion`, la línea siguiente no vacía debe estar más
  indentada (error F32-020 si no).
- `finsi`, `finmientras`, `finfuncion`, `fin`, `sinosi`,
  `sino` deben alinearse con su apertura (misma indentación;
  error F32-021 si no).
- `fin` siempre en columna 0. `programa` siempre en columna 0.

## 2. Léxico

- Identificadores: `[A-Za-z_][A-Za-z0-9_]*`, ASCII en v0.1
  (los literales de cadena sí admiten UTF-8 completo).
- Enteros: `[0-9]+` (el `-` unario es operador, no parte del
  literal). Rango 32 bit; fuera de rango = error F32-031.
- Cadenas: `"` ... `"` en una sola línea, sin escapes en v0.1
  (una `"` interior = error F32-030). UTF-8 válido exigido.
- Comentarios: `#` fuera de cadena ignora el resto de la
  línea. Una línea solo con comentario/blancos no cuenta para
  estructura ni indentación.
- Operadores y puntuación: `+ - * / % = == != < <= > >= ( ) ,`.
  Palabras operador: `y o no` (reservadas, no identificadores).

## 3. Expresiones (precedencia, de menor a mayor)

1. `o`
2. `y`
3. `no` (unario)
4. Comparaciones `== != < <= > >=` (no encadenables: `a < b < c`
   = error F32-034)
5. Aditivas `+ -` (`+` también concatena cadenas)
6. Multiplicativas `* / %`
7. Unario `-`, llamadas `nombre(...)`, literales, variables,
   paréntesis

Asociatividad izquierda; `* / %` y `+ -` habitual; `no` se
aplica al operando siguiente.

## 4. Ejemplo indentado

```text
programa

    variable edad = 19

    si edad >= 18

        escribir "Mayor de edad"

    sinosi edad >= 16

        escribir "Edad intermedia"

    sino

        escribir "Menor de edad"

    finsi

fin
```
