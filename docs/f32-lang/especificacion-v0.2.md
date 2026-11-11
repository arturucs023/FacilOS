# F32 Lang v0.2 — Especificación oficial

Evolución de v0.1 dentro de FacilOS v2.4.3 (el SO no cambia de
versión por el lenguaje). Todo lo válido en v0.1 sigue válido
en v0.2: `programa ... fin`, cuerpo indentado con múltiplos de
4 espacios, `variable x = valor` con inicializador obligatorio,
`escribir`, `leer`, `si`/`sinosi`/`sino`/`finsi`,
`mientras`/`finmientras`, `funcion`/`finfuncion`,
`retornar`, `y`/`o`/`no`, `verdadero`/`falso`.
Formato de error y códigos de salida sin cambios
(ver `errores.md` y `errores-v0.2.md`).

Novedades v0.2 (detalle en su página):

| Tema | Palabras / sintaxis | Doc |
|---|---|---|
| Módulos | `importar a.b`, `mod.func()`, `mod.var` | `modulos.md` |
| Archivos | `abrir`, `leer_linea`, `escribir_linea`, `cerrar`, `existe` | `archivos.md` |
| Arreglos | `[1, 2]`, `a[i]`, `a[i] = v` | `estructuras.md` |
| Bucle `para` | `para i desde A hasta B [paso S]` ... `finpara` | `estructuras.md` |
| Cadenas | `+`, `< <= > >=`, `a[i]`, `longitud`, `buscar`, `subcadena` | `estructuras.md` |
| Registros | `[nombre: "ana", edad: 30]`, `r.campo` | `estructuras.md` |
| Miscelánea | `longitud` | `estructuras.md` |

Límites nuevos: arreglo ≤ 256 elementos, registro ≤ 32
campos, módulos ≤ 16 por programa (pila de carga ≤ 8),
8 descriptores de archivo, iteraciones por bucle ≤ 100000
(compartido con `mientras`). Sin libc/host: solo API nativa
de FacilOS. ABI y mapa de memoria sin cambios.

## Programa mínimo (igual que v0.1)

```text
programa
    escribir "Hola, mundo!"
fin
```

El editor crea este esqueleto solo con `programa` y `fin` al
abrir un `.f32` nuevo o vacío.

## Ejemplo 0.2 (todo junto)

```text
programa
importar m1
    variable notas = [7, 9, 5]
    notas[1] = 10
    variable total = 0
    para i desde 0 hasta 2
        total = total + notas[i]
    finpara
    escribir m1.doble(total)
    variable r = [nombre: "ana", total: total]
    escribir r.nombre
    escribir r.total
fin
```

Batería: `t_f32` (59 pruebas, 0 fallos en QEMU) + FULL
(670 pruebas, 0 fallos). Ver `v2.0/docs/f32.txt`.
