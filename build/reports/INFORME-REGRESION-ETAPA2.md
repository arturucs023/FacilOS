# INFORME-REGRESION-ETAPA2 (Fases 14 y 16)

## Calidad de las pruebas (Fase 14, verificado)
- Comportamiento real (contenido/rc/estados, no solo fin).
- Sin aleatoriedad (secuencias fijas; lo concurrente se acota por protocolo).
- Sin esperas ciegas salvo drenajes de fusión diferida (500 ms en G/H,
  justificados por diseño + éxito 7/7) y drenajes verificados por condición
  (estres_*, reintentan hasta 1 s y fallan si no cuadra).
- Timeout/equivalentes: fases acotadas (sprint 100, lluvias ≤60, esperas
  sobre hijos que terminan por construcción); el deadlock de test P4 se
  detectó y corrigió (cede + drenaje).
- Error esperado (-1/-2/-100/rc) vs inesperado: asserts separados.
- Umbrales sin tocar: G sigue exigiendo 7, equidad ratio 1,5, F marcos ±2.

## Regresión mínima exigida (esta etapa, binario final salvo nota)
- Batería 8/8: SUPERADA ×7. mem32 15/15, equidad32 3/3, ipc32 111/0,
  sucio32 32/32, t1_lib 258/0, secuenciales 20/20, balance exacto,
  shell/hola/version, reloj (+reloj 2×), gráficos (2 gfx en v1.9; reloj en
  v2.0), arranque completo: SUPERADA.
- estres_mem 12/12, estres_ipc 27/27, estres_mix 8/8: SUPERADA.
- ABI 0–33 congelado (comparado caso por caso en refactor a tabla) y
  34–37 según contrato (sucio32 + ipc32): SUPERADA.
- `t2_gfx` 11/12: sin cambios (limitación documentada).
