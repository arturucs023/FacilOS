# F32 Lang v0.1 — Variables y tipos

## 1. Declaración y asignación

```text
variable nombre = "Artur"
variable edad = 19
variable activo = verdadero
nombre = "Otro"
```

- `variable` declara y fija el tipo con el valor inicial
  (obligatorio: `variable x` sin `=` = error F32-040).
- Redeclarar = error F32-041. Asignar sin declarar =
  error F32-042 (salvo `leer`, que declara cadenas).
- Reasignar con otro tipo = error F32-043. No hay
  conversiones implícitas: `edad + "x"` es error, no `"19x"`.

## 2. Reglas por tipo

- Entero: aritmética `+ - * / %` (división entera truncada
  hacia cero; `/ %` con divisor 0 = error F32-050).
- Cadena: solo `+` (concatenación, tope 512 B, error
  F32-051 si lo supera) y `== !=`. Comparar cadenas con
  `< >` = error F32-052 (estricto y predecible).
- Booleano: solo `y o no` y `== !=`. `si` y `mientras`
  exigen booleano (un entero NO vale: error F32-053).
- `escribir` acepta cualquier tipo (entero en decimal,
  cadena tal cual, booleano como `verdadero`/`falso`).

## 3. Ámbito

- Globales visibles en todo el programa (incluido dentro de
  funciones, solo lectura recomendada; la escritura a una
  global desde función exige que exista: se reasigna la
  global, no se crea local —documentado, sin `global`).
- Parámetros y `variable` dentro de función = locales
  (sombra a globales del mismo nombre).
- `leer nombre`: si no existe, la crea global/local como
  cadena; si existe y no es cadena = error F32-054.
- Totales: ≤ 256 globales, ≤ 64 locales+parámetros por
  función (error F32-055).

## 4. Decimales (estudiado, fuera de v0.1)

Se excluyen: la libc freestanding carece de soporte flotante
garantizado y el coste (exactitud, tamaño del binario,
formato de salida) no compensa en v0.1. División entera
documentada + error en divisor cero. Futura propuesta:
tipo `decimal` de coma fija.
