# ROADMAP-KERNEL-v2.0-ACTUALIZADO (Etapa 3)

## Hecho en Etapa 3
ATA PIO verificado, `blk` (IDENTIFY+rangos+flush), formato v1.2
especificado, `voldisco.py` (mkfs/verifica/lista), fs.c vía `blk`,
`disco32` 38/38, persistencia real entre reinicios, kill -9 coherente,
corruptos rechazados sin tocar disco, ISO etapa3 + imagen de datos.

## Etapa 4 propuesta (gráficos/servicios)
Servidor gráfico con superficies; eventos por IPC; ratón PS/2; segundo
backend VFS persistente sobre esta misma capa `blk`; drenaje explícito;
telemetría `sc_count`.

## Deuda restante
`t2_gfx` 11/12; doble búfer efectivo; MAXPROC=8; sin SMP/COW/swap/POSIX;
heap-k sin carga; sesiones >15 min y hardware real sin probar.
