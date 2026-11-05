# F32 Lang v0.1 — Parser y AST (`f32.c`, partes 2-4)

Descendente recursivo sobre la tabla de tokens agrupada por
línea (`LTINI/LTN`). Sin backtracking: una pasada.

- `p_prog`: exige `programa` primero (F32-016), cuerpo con
  indentación > 0, funciones a nivel 0 o hijas directas de
  `programa` (la sintaxis aprobada las muestra dentro; se
  aceptan ambas posiciones), `fin` final y nada tras él
  (F32-015).
- Bloques (`p_cuerpo`): las líneas con indentación mayor
  pertenecen al bloque; la primera con indentación igual o
  menor lo cierra. Cuerpo vacío = F32-020; cierre con
  indentación distinta = F32-021; `sinosi/sino` fuera de
  lugar = F32-060/061; `fin` prematuro con bloques abiertos
  = F32-012/013/014.
- Expresiones con precedencia (`p_y > p_no > p_cmp > p_sum >
  p_mul > p_un > p_prim`): `no` liga menos que las
  comparaciones (`no a == b` ≡ `no (a == b)`); las
  comparaciones no se encadenan (F32-034); `()` y llamadas
  `nombre(args)` con aridad comprobada en semántica.
- AST: nodos `N_PROG/N_BLOQ/N_VAR/N_ASIG/N_PRINT/N_READ/
  N_IF/N_WHILE/N_DEF/N_CALL/N_RET/N_BIN/N_UN/N_LIT/N_REF`
  con número de línea. Límite 4096 nodos (F32-091).
