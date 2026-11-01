# ARQUITECTURA-ALMACENAMIENTO-ETAPA3

## Capas (propias de FacilOS32, sin POSIX)

```
App Ring3 --syscalls 3/4/5/6/9/15/16/24--> VFS (vfs.c: tabla de ops)
    --> backend ramfs --> fs.c (FACILOS2/v1.2, espejo+tramos+pool)
    --> sec_lee/sec_pon --> blk (IDENTIFY+rangos+flush) --> ata PIO
    --> IDE primario-maestro (QEMU -drive if=ide)
Shell (Ring0) --> fs.c directo (incluidos directorios)
```

## Contratos
- `blk`: init/detecta+capacidad, leer/escribir con rangos, flush; 0/-1.
- `fs`: montar valida (firma+versión+checksum+tramos+sumas) o RAM limpio
  sin tocar disco; guardar = espejo+tabla+cabecera+flush, -1 ante fallo.
- Orden seguro: datos directos (write-through) → espejo → tabla →
  cabecera → flush. Ventana de corte documentada (sin journal por diseño).
- VFS: backend único montado (`vfs_actual`); directorios vía fs.c directo.
- Sin disco: pool RAM 256 KB, misma semántica, sin persistencia.

## Límites
Imagen ≤20480 sectores (10 MB); 64 entradas; nombres ≤31; fichero ≤65535;
tramos first-fit desde ds 64; LBA28. Sin montaje múltiple ni red.
