# INFORME-ESTRES-MEMORIA-ETAPA2 (Fases 1–4)

## Alcance y resultado
`estres_mem` 12/12 + `mem32` 15/15 + batería F (40 ciclos) + balance exacto
repetido. Sin FUGA, sin doble-free (contador a cero), sin PANIC.

## Fases cubiertas
- F1 física: 20 crecer+salir con drenaje verificado cada 5; 10 muertes por
  #PF tras crecer (recuperación exacta); 5 concurrentes con presión visible;
  sprint al tope de heap (limpio) + sistema vivo; balance final exacto.
  Agotamiento global de marcos: NO alcanzable desde usuario por diseño
  (8×608 < libres; verificado por inspección + tope limpio en sprint).
  Doble-free forzado: no invocable desde usuario; contador a cero en toda
  la etapa (sin falsos positivos) + revisión de todas las rutas devuelve.
- F2 virtual: VMA contra mapeos en cada destrucción (~400 sin desajustes);
  supervisor inaccesible (A), WP activo (CR0=80010011 en serie), TLB por
  página y por CR3, sin aliasing usuario/núcleo.
- F3 heap: autoprueba en arranque + canarios; sin consumidores reales en el
  núcleo (límite documentado: carga real pendiente si el heap se usa).
- F4 sbrk: crecimiento, tope 1,5 MB limpio, muerte inmediata tras crecer
  (P2), VMA con unión por solape, valores/límites según contrato.

## Defectos hallados (ver REGISTRO)
- Condición invertida de anotado en sbrk (P1, corregida).
- VMA merge solo-adyacente (falso FUGA; corregido a unión).
