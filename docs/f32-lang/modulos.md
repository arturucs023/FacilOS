# F32 Lang v0.2 — Módulos

`importar` carga otro `.f32` como módulo. Solo a nivel de
programa (nivel 0): dentro de bloques o funciones es error
F32-016.

## Resolución

`importar a` carga `/a.f32` (relativo al fichero que importa).
`importar a.b` carga `/a/b.f32`. Puntos = subdirectorios.
Sin `..` ni componentes vacíos (error F32-088).
No encontrado = error F32-086.
Dependencia circular (a importa b que importa a) = error
F32-087, detectado en la precarga antes de ejecutar nada.

## Ejecución única

Cada módulo se ejecuta una sola vez aunque se importe varias
veces (o desde varios módulos). Su cuerpo puede declarar
funciones y variables; las variables nacen al ejecutar el
cuerpo del módulo (llamada implícita en el primer `importar`).

## Nombres cualificados

- Llamada: `mod.func(args)`.
- Variable: `mod.var` (lectura y asignación).
- Sin cualificar no se ven: las variables y funciones de un
  módulo no entran en el ámbito del programa.
- Dos módulos pueden repetir el mismo nombre de función; el
  programa también puede definir `doble` y coexistir con
  `m1.doble`: sin cualificar se usa la propia, cualificada la
  del módulo.

## Ejemplo

`/m1.f32`:

```text
programa
funcion doble(n)
    retornar n * 2
finfuncion
    variable base = 100
fin
```

`/uso.f32`:

```text
programa
importar m1
    escribir m1.doble(21)
    escribir m1.base
fin
```

Salida:

```text
42
100
```

## Errores

| Código | Significado |
|---|---|
| F32-086 | módulo no encontrado (también `mod.var` / `mod.f()` con `mod` desconocido) |
| F32-087 | dependencia circular entre módulos |
| F32-088 | ruta de módulo inválida (`..`, vacío, demasiado largo) |
| F32-083 | `mod.nada()` si no existe esa función en el módulo |
| F32-084 | nº de argumentos incorrecto (también en cualificadas) |
