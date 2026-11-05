# F32 Lang v0.1 — Condiciones

```text
si edad >= 18

    escribir "Mayor de edad"

sinosi edad >= 16

    escribir "Edad intermedia"

sino

    escribir "Menor de edad"

finsi
```

## Reglas

1. Todo bloque empieza con `si expresion` (expresión
   booleana estricta; si no, error F32-053).
2. Cero o más `sinosi expresion` (cada uno con condición).
3. Como máximo un `sino` (sin condición), y siempre el
   último alternativo (otro orden = error F32-061).
4. Cierra `finsi` alineado con su `si` (error F32-021 si no,
   F32-012 si falta al llegar a `fin`/EOF).
5. Anidamiento permitido (tope 32 niveles, error F32-062).
6. Se evalúa en orden; solo entra en el primer bloque cuya
   condición sea cierta (o `sino`).
7. `sinosi`/`sino`/`finsi` sin `si` abierto = error F32-060.
8. El editor auto-indenta tras `si/sinosi/sino` y alinea los
   cierres (ver `integracion-editor.md`).
