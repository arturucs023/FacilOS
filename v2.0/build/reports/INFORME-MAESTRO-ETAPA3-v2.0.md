# INFORME-MAESTRO-ETAPA3-v2.0

## Resumen ejecutivo
Almacenamiento persistente propio verificado: el sistema guarda de verdad
(archivo idéntico entre dos arranques reales con el mismo disco). Sin
reescrituras: driver ATA existente + capa `blk` + formato v1.2 existente +
VFS intacto. 0 P0/P1 abiertos. Regresión total verde.

## Arquitectura examinada
App → syscalls ficheros (ABI intacto) → VFS → ramfs/fs.c → blk →
ATA PIO → IDE QEMU. Shell directa a fs.c (documentado).

## Subsistemas auditados
Bloques, formato, metadatos, asignación, directorios, VFS, syscalls (sin
cambios), consistencia, seguridad Ring3, herramientas, persistencia.

## Pruebas realizadas (exacto)
blk ±disco; `disco32` 38/38 ×2 backends; persistencia 2 arranques;
kill -9 + reboot; corruptos ×2; batería 8/8; mem/t1/ipc/sucio/equidad/
estres ×3; plan opt1/opt3; reloj ×2; 2 gfx; dirs; balance exacto.

## Defectos confirmados
Ninguno en código heredado de almacenamiento. Uno de desarrollo (tope
catálogo 24 por apps nuevas — etapa 2, ya corregido a 32). Tests: 2 fallos
de protocolo corregidos en los propios tests.

## Correcciones / regresiones / limitaciones / riesgos / no ejecutado
Correcciones: `blk`, flush, catálogo 32. Regresión: toda SUPERADA salvo
`t2_gfx` 11/12 (preexistente). Limitaciones: sin journal (checksum+
verificador), pool RAM sin persistencia, 10 MB/64 entradas/64 KB por
diseño, sesiones >15 min y hardware real no probados.
Riesgos: caché QEMU ante corte (documentado), desgaste sin wear-leveling
(fuera de alcance QEMU). No ejecutado: AHCI, segundo backend, ratón.

## Evidencias
`build/logs/v30_*.log`, `voldisco verifica/lista`, ISO
`FacilOS-v2.0-etapa2.iso` + `build/images/FacilOS-v2.0-etapa3-datos.img`,
commits en `v2.0-etapa3`.

## Veredicto final
**ETAPA 3 COMPLETADA.**
