# F32 Lang v0.1 — Bucles

```text
programa

    variable contador = 0

    mientras contador < 5

        escribir contador
        contador = contador + 1

    finmientras

fin
```

## Reglas

1. `mientras expresion` exige booleano (error F32-053 si no).
2. La condición se evalúa antes de cada iteración; si es
   falsa, el bucle termina y sigue tras `finmientras`.
3. `finmientras` alineado con su `mientras` (F32-021) y
   obligatorio (F32-013 si falta). Sin `mientras` = F32-060.
4. Anidamiento permitido (tope 32, F32-062).
5. Cuota anti-cuelgue: 100000 iteraciones por bucle; al
   superarla, error F32-070 (nunca bucle infinito silencioso).
   Sin `romper`/`continuar` en v0.1 (futuro documentado).
6. El cuerpo debe estar más indentado que el `mientras`
   (F32-020).
