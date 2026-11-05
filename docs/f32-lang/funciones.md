# F32 Lang v0.1 — Funciones

```text
programa

    funcion saludar

        escribir "Hola desde F32 Lang"

    finfuncion

    saludar()

fin
```

Con parámetros y retorno:

```text
    funcion suma(a, b)

        retornar a + b

    finfuncion

    variable r = suma(2, 3)
    escribir r
```

## Reglas

1. Solo a nivel superior (indentación 0), nunca anidadas ni
   dentro de bloques (error F32-080).
2. `funcion nombre` o `funcion nombre(p1, p2, ...)` (≤ 8
   parámetros, nombres únicos; F32-081/F32-082).
3. La llamada es `nombre(...)` como instrucción o dentro de
   expresiones. Llamar a algo inexistente = error F32-083;
   número de argumentos distinto = error F32-084.
4. Parámetros por valor. Tipos comprobados en la llamada
   (desajuste = error F32-043).
5. `retornar expresion` sale con ese valor. Caer del final
   sin `retornar` devuelve `0` (documentado, no ambiguo).
6. Recursión permitida, tope 64 llamadas anidadas
   (error F32-085). Sin cierres ni funciones como valores.
7. Las funciones se recolectan antes de ejecutar el cuerpo
   principal: el orden de declaración no importa.
8. Redefinir una función = error F32-082.
