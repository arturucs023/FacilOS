# F32 Lang v0.2 — Estructuras (arreglos, `para`, cadenas, registros)

## 1. Arreglos

Literal: `[1, 2, 3]`, `[]` (vacío). Elementos heterogéneos
permitidos. Acceso `a[i]` (0-based, solo lectura en
expresión). Asignación `a[i] = v` (el arreglo sigue siendo el
mismo objeto). `longitud(a)` = nº de elementos.
Impresión: `[1, 99, 3]`.

```text
programa
    variable a = [1, 2, 3]
    a[1] = 99
    escribir a[1]
    escribir a
    escribir longitud(a)
fin
```

Fuera de rango = error F32-120. `a[i] = v` con `a` no
arreglo = error F32-121. Comparar arreglos con `==`/`!=`
compara contenido (mismo tipo exigido, si no F32-043).

## 2. `para`

```text
para i desde A hasta B [paso S]
    ...
finpara
```

`A`, `B`, `S` enteros. Sin `paso`: +1 si `A <= B`, -1 si no.
La variable se crea para el bucle y sombrea temporalmente
cualquier variable exterior del mismo nombre; al terminar se
restaura. Si el cuerpo la modifica, el siguiente valor es
`modificado + paso`. Iteraciones ≤ 100000 (error F32-070,
como `mientras`). Ejemplo (suma 1+2+3+4):

```text
programa
    variable s = 0
    para i desde 1 hasta 4
        s = s + i
    finpara
    escribir s
fin
```

## 3. Cadenas (0.2)

- `+` concatena STR+STR (tope 512 B, error F32-051).
- Comparación lexicográfica por bytes: `< <= > >=` con
  STR+STR (v0.1 la prohibía con F32-052; en v0.2 ese código
  queda reservado y ya no se emite).
- `s[i]` = carácter `i` como STR de 1 (fuera de rango =
  F32-120). `longitud(s)` = caracteres.
- `buscar(texto, sub)` = índice de la primera ocurrencia
  o -1 (`""` = 0). `subcadena(texto, ini [, n])` = porción;
  fuera de rango = `""`.

```text
programa
    variable s = "ho" + "la"
    escribir s
    escribir s[1]
    escribir buscar("hola mundo", "mundo")
    escribir subcadena("hola", 1, 2)
fin
```

Salida: `hola`, `o`, `5`, `ol`.

## 4. Registros

Literal: `[nombre: "ana", edad: 30]` (claves identificador,
valores cualquier tipo). Acceso `r.nombre` o `r["nombre"]`.
Asignación `r.campo = v` (el registro sigue siendo el mismo
objeto). `longitud(r)` = nº de campos. Impresión:
`[nombre: ana, edad: 30]`.

```text
programa
    variable r = [nombre: "ana", edad: 30]
    escribir r.nombre
    r.edad = 31
    escribir r["edad"]
fin
```

Campo inexistente = error F32-122. Punto sobre no-registro
= error F32-121. `==`/`!=` entre registros compara campos
por nombre y valor.

## 5. `longitud` (resumen)

STR = caracteres, ARR = elementos, REG = campos. Otro tipo
= error F32-043.
