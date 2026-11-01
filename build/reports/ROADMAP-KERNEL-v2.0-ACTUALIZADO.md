# ROADMAP-KERNEL-v2.0-ACTUALIZADO (Etapa 2)

## Hecho en Etapa 2 (auditoría extrema)
NUEVO→LISTO atómico, despertares sin pérdida teórica, chequeo FUGA en cada
destrucción, VMA con unión, sbrk anotado, 7 GPR en entrada a usuario, tabla
IPC cableada, CD_CAT_MAX 32, 3 apps de estrés + 1 mixta, ISO etapa2 propia.

## Etapa 3 propuesta (gráficos/servicios, fuera de esta etapa)
Servidor gráfico con superficies por proceso; eventos por IPC; ratón PS/2;
backends VFS persistentes; drenaje explícito o reclamación en espera;
telemetría `sc_count`; carga real del heap-k si se usa.

## Deuda restante (igual que etapa 1 + lo de arriba)
`t2_gfx` 11/12; doble búfer efectivo; MAXPROC=8; sin SMP/COW/swap/POSIX.
