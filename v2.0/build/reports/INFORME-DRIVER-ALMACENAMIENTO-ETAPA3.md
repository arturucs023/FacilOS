# INFORME-DRIVER-ALMACENAMIENTO-ETAPA3 (Fase 2)

## Dispositivo elegido: ATA PIO existente
Primario-maestro (0x1F0–0x1F7), LBA28, `rep insw/outsw`, 0x20/0x30, con
timeouts. Motivos: compatible QEMU (`-drive if=ide`), simple, probado en
v1.8, sin firmware tras arranque, ampliable. AHCI descartado (complejidad
sin necesidad: sin DMA/NCQ/SMP). Sin segundo controlador en esta etapa.

## Capa `blk` (nueva, `blk.{c,h}`)
IDENTIFY (0xEC) con presencia por DRDY (0xFF/0x00 = ausente), capacidad de
words 60–61 (rechaza 0/>2^28), validación de rangos (sin overflow),
FLUSH 0xE7, PIO intacto en `ata.c`. Evidencia: `blk: disco ATA 20480
sectores` con disco; `blk: sin disco en primario-maestro` sin disco (RAM
sigue bien).

## Contratos y errores
init/leer/escribir/flush → 0/-1; rangos/ausente/buf-nulo → -1 sin tocar
hardware; timeouts 100000 iteraciones. Sin operaciones parciales
silenciosas (fallo = -1 al llamante).
