# F32 Lang v0.1 — Arquitectura del intérprete

Programa Ring3 `f32` (C freestanding, `ulib/utext`, sin
dependencias). Fases estrictas, no buscar-y-sustituir:

1. **Carga**: `u_fopen` + lectura total (tope 32768 B;
   F32-001/002). Valida UTF-8 (F32-032).
2. **Léxico**: tokens por línea con indentación medida
   (espacios; tab = F32-022), número de línea, literales con
   valor, palabras reservadas, operadores. Topes de línea y
   tokens (F32-003/091).
3. **Parser**: AST en arena `u_malloc` (nodos: programa,
   bloque, decl, asign, escribir, leer, si, mientras, def,
   llamada, retornar, binario, unario, literal, var).
   Valida estructura `programa...fin`, bloques y cierres,
   indentación relativa (F32-012..022, F32-060/061).
4. **Semántica**: pre-recolección de funciones (F32-082),
   tabla de símbolos con tipos (256 globales), comprobación
   de declaraciones/tipos/arity en una pasada (F32-040..055,
   F32-08x). Sin ejecución parcial ante errores semánticos.
5. **Ejecución**: walker con entorno (globales + marco por
   llamada), cuotas (iteraciones, profundidad, expresión).
   `escribir` -> `u_print`; `leer` -> `u_lee_linea`.
6. **Errores**: siempre con archivo+línea cuando se conoce;
   primer error aborta con su código (ver `errores.md`).

Memoria: fuente + tokens + AST + valores acotados por los
topes (< 512K muy por debajo del heap de 1.5 MB; medido con
`u_malloc_info` en FASE 7). Cada `u_malloc` comprobado.
