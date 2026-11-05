# F32 Lang v0.1 — Especificación oficial

Lenguaje propio de FacilOS32 v2.2 (el SO no cambia de versión).
Extensión fuente `.f32`. Extensión `.f32x` reservada para futuros
ejecutables compilados (v0.1 solo interpreta; no existe formato
compilado `.f32x` utilizable).

## 1. Programa mínimo

```text
programa

    escribir "Hola, mundo!"

fin
```

Reglas estructurales (error si se incumplen):

1. La primera línea no vacía ni comentario debe ser `programa`
   (indentación 0). La última debe ser `fin` (indentación 0).
2. Fuera de `programa...fin` solo se permiten bloques `funcion`.
3. El cuerpo se indenta con múltiplos de 4 espacios (ver
   `sintaxis.md`). Tabuladores prohibidos.
4. Archivo ≤ 32768 bytes, ≤ 4096 líneas (error F32-002/F32-003).

## 2. Tipos (v0.1)

| Tipo | Literal | Notas |
|---|---|---|
| entero | `19`, `-4`, `0` | 32 bit con signo |
| cadena | `"texto"` | UTF-8, comillas dobles, sin escapes en v0.1 |
| booleano | `verdadero`, `falso` | — |

Sin decimales en v0.1 (estudiado: exactitud y tamaño del
intérprete; `/` es división entera; futuro documentado).
Sin conversiones implícitas (ver `variables-y-tipos.md`).

## 3. Instrucciones (resumen)

- `variable nombre = expresion` (declara; tipo fijado).
- `nombre = expresion` (reasigna; debe existir y mismo tipo).
- `escribir expresion` (una expresión; concatena con `+`).
- `leer nombre` (lee una línea ≤ 255 caracteres como cadena;
  declara la variable si no existe).
- `si/​sinosi/​sino/​finsi`, `mientras/​finmientras`,
  `funcion/​finfuncion` (+ `retornar expresion`).
- Comentario `#` hasta fin de línea. Líneas vacías ignoradas.

## 4. Límites de ejecución (seguridad Ring3)

- Variables globales ≤ 256; locales+parámetros ≤ 64/función;
  funciones ≤ 64; parámetros ≤ 8; cadenas ≤ 512 B.
- Profundidad de expresión ≤ 32, anidado de bloques ≤ 32,
  llamadas ≤ 64, iteraciones por bucle ≤ 100000.
- Superarlos = error con archivo y línea, nunca cuelgue ni
  PANIC. Sin acceso a memoria del kernel (solo syscalls
  de ficheros, consola, memoria y procesos).

## 5. Errores y salida

Formato único (ver `errores.md`):

```text
Error F32-012 en demo.f32 linea 8: falta finsi.
```

Salida 0 = fin correcto (`Programa finalizado correctamente.`
lo imprime la terminal, no el programa). Salida 1 = error del
programa. Salida 2 = mal uso (sin archivo).
