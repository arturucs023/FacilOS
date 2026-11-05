# F32 Lang v0.1 — Errores

Formato único (el intérprete sale con código 1):

```text
Error F32-012 en demo.f32 linea 8: falta finsi.
```

Sin archivo/línea conocida se omite ese tramo (p. ej.
`Error F32-002: programa demasiado grande.`).

| Código | Significado |
|---|---|
| F32-001 | no se pudo abrir/leer el archivo |
| F32-002 | programa > 32768 bytes |
| F32-003 | más de 4096 líneas |
| F32-010 | palabra reservada desconocida / token inválido |
| F32-011 | identificador usa palabra reservada |
| F32-012 | falta `finsi` |
| F32-013 | falta `finmientras` |
| F32-014 | falta `finfuncion` |
| F32-015 | falta `fin` / código tras `fin` |
| F32-016 | `programa` no es la primera instrucción |
| F32-020 | el bloque no aumenta indentación |
| F32-021 | el cierre no alinea con su apertura |
| F32-022 | tabulación o indentación no múltiplo de 4 |
| F32-030 | cadena sin cerrar / multilínea |
| F32-031 | entero fuera de 32 bit |
| F32-032 | UTF-8 inválido en el fuente |
| F32-033 | paréntesis/coma mal formados en llamada |
| F32-034 | comparación encadenada |
| F32-040 | `variable` sin `=` inicial |
| F32-041 | variable ya declarada |
| F32-042 | variable inexistente |
| F32-043 | tipos incompatibles |
| F32-050 | división o resto por cero |
| F32-051 | cadena supera 512 B |
| F32-052 | orden `< >` con cadenas |
| F32-053 | se exigía booleano |
| F32-054 | `leer` sobre variable no cadena |
| F32-055 | demasiadas variables |
| F32-060 | cierre sin apertura (`sinosi/sino/finsi/...`) |
| F32-061 | `sino` no es el último / duplicado |
| F32-062 | anidamiento > 32 niveles |
| F32-070 | bucle supera 100000 iteraciones |
| F32-080 | función anidada o fuera de nivel superior |
| F32-081 | parámetros duplicados o > 8 |
| F32-082 | redefinición de función |
| F32-083 | llamada a función inexistente |
| F32-084 | nº de argumentos incorrecto |
| F32-085 | llamadas anidadas > 64 |
| F32-090 | sin memoria |
| F32-091 | límite interno del intérprete |

Códigos de salida: `0` fin correcto, `1` error del programa
(mensaje ya mostrado), `2` mal uso (sin archivo o ilegible).
