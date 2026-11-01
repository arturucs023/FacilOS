# ROADMAP-KERNEL-v2.0-ACTUALIZADO (Etapa 3 + Etapa 4)

## Hecho en Etapa 3
ATA PIO verificado, `blk` (IDENTIFY+rangos+flush), formato v1.2
especificado, `voldisco.py` (mkfs/verifica/lista), fs.c via `blk`,
`disco32` 38/38, persistencia real entre reinicios, kill-9 coherente,
corruptos rechazados sin tocar disco, ISO etapa3 + imagen de datos.

## Hecho en Etapa 4 (E2-E6, esta rama)
Leases de composicion (cero cambios de kernel al inicio); syscalls
38/39 (raton) + 40 (ipc_hay) + 41-44 (dir_hijo/crear/renombrar/reloj);
raton PS/2 por IOAPIC (cascada i8259 rota en este QEMU); servidor +
WM + escritorio (menu/barra/reloj); UGFX + controles; terminal,
explorador, sysinfo, editor, configura; persistencia re-verificada;
ISO etapa4. Veredicto en INFORME-MAESTRO-ETAPA4.

## Etapa 5 propuesta (servicios/dureza)
Maximizar, mover por teclado accesible, portapapeles, menus contextuales,
doble-clic con politica, segundo backend VFS, drenaje explicito,
telemetria `sc_count`, sesiones largas, hardware real.

## Deuda restante
`t2_gfx` 11/12; doble bufer efectivo; MAXPROC=8; sin SMP/COW/swap/POSIX;
heap-k sin carga; hardware real sin probar; flakes `crear` bajo carga;
F1/Menu dual documentado; CD 48 entradas.
