# F32 Lang v0.2 — Archivos

Archivos como builtins con paréntesis (sin sintaxis nueva).
Rutas absolutas (`/datos.txt`). Modos: `"lectura"`,
`"escritura"` (trunca/crea), `"anadir"`.

| Builtin | Aridad | Devuelve | Notas |
|---|---|---|---|
| `abrir(ruta, modo)` | 2 | descriptor INT | modo raro = 104; no existe = 100 |
| `leer_linea(h)` | 1 | STR sin `\n` | EOF = `""` |
| `escribir_linea(h, texto)` | 2 | INT 0 | escribe texto + `\n` |
| `cerrar(h)` | 1 | INT 0 | doble cierre / malo = 103 |
| `existe(ruta)` | 1 | BOL | nunca falla por ausencia |

Máximo 8 descriptores abiertos (error F32-105).
Tipos malos = error F32-043. Fallo de lectura = 101,
de escritura = 102.

## Ejemplo (eco + añadir)

```text
programa
    variable f = abrir("/datos.txt", "escritura")
    escribir_linea(f, "hola")
    escribir_linea(f, "mundo")
    cerrar(f)
    variable g = abrir("/datos.txt", "lectura")
    escribir leer_linea(g)
    escribir leer_linea(g)
    cerrar(g)
fin
```

Salida:

```text
hola
mundo
```

Los descriptores se cierran al terminar; en `t_f32` cada
prueba usa `/t_f32_datos.txt` en este orden: eco, relectura
(EOF da `""`), añadir `"!"`, existencia.
