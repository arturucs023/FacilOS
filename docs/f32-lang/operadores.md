# F32 Lang v0.1 — Operadores

De menor a mayor precedencia: `o`, `y`, `no`, comparaciones,
`+ -`, `* / %`, unario `-`, llamada/paréntesis.

## 1. Aritméticos (enteros)

| Op | Ejemplo | Resultado |
|---|---|---|
| `+` | `2 + 3` | `5` |
| `-` | `7 - 10` | `-3` |
| `*` | `4 * 5` | `20` |
| `/` | `7 / 2` | `3` (entera) |
| `%` | `7 % 2` | `1` |

`/ %` con divisor cero = error F32-050. Desbordamiento 32 bit
(envuelve en complemento a dos, como el C del sistema) =
comportamiento definido y documentado, no error.

## 2. Cadenas

- `+` concatena (`"a" + "b"` = `"ab"`, tope 512 B).
- Cualquier otro aritmético con cadena = error F32-043.

## 3. Comparaciones

- `== !=` válidos para los tres tipos (mismo tipo exigido).
- `< <= > >=` solo enteros. Todo lo demás = error F32-043.
- No encadenables (`a < b < c` = error F32-034).

## 4. Lógicos (booleanos estrictos)

- `y`: cierto si ambos ciertos. `o`: cierto si alguno.
  `no x`: invierte. Operandos no booleanos = error F32-053.
- Evaluación con cortocircuito (el lado derecho puede no
  evaluarse): documentado porque afecta a llamadas con
  efectos (`escribir`) dentro de condiciones.
