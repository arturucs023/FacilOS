# INFORME-ESTRES-MULTITAREA-ETAPA2 (Fase 5 + planificador)

## Alcance y resultado
Batería 8/8 ×7, `equidad32` 3/3 ×3, planificador32 opt1/opt3,
`estres_mix` 8/8, ráfagas G (7/7), MAXPROC sostenido. Sin inanición
observada; equidad medida 34/34/34 y 67/67/68 (ratio < 1,5).

## Casos verificados
1/2/N procesos; cargas CPU puras (`quema32`); cesión voluntaria (syscall 32
en P4-IPC y `u_ceder`); bloqueos tecla/ticks/espera/IPC; terminación normal
y por excepción; creación/destrucción continuada (40+20+ráfagas); estados
transitorios (NUEVO no seleccionable desde este etapa); contadores de ticks
coherentes con carga (Fase 5.6 verificado por `equidad32`).
Terminado nunca reejecutado (elige solo LISTO); sin bloqueos indefinidos
(ventanas de despertar cerradas: preset-BLOQ + no-pisar-LISTO + recheck).
MAXPROC=8 documentado (7 hijos + padre; -104 controlado).

## Defectos hallados (ver REGISTRO)
- EAX/ECX/EDX de usuario no restaurados en `entra_usuario` (P0, corregido
  vía `salta_usuario`; 1 caso `pid=32` lo evidenció).
- Publicación a medio inicializar (riesgo C → eliminado con NUEVO→LISTO).
- Despertares con ventana teórica (mitigados; residual de ~ns documentado).
