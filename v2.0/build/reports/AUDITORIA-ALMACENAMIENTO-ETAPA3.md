# ETAPA 3 — FASE 0: auditoría inicial de almacenamiento

Rama `v2.0-etapa3` (desde etapa2). Sin cambios de código en esta fase.

## 1. Estado Git
Limpio. Referencia etapa2 intacta en rama `v2.0-etapa2`; `main`/v1.9/tag
`v1.9`/release intactos. ISO anterior: `v2.0/build/iso/FacilOS-v2.0-etapa2.iso`.

## 2. Arquitectura de almacenamiento actual (confirmada por inspección)
- `src/kernel32/ata.c`: ATA PIO LBA28, maestro primario (0x1F0–0x1F7),
  `rep insw/outsw`, lectura 0x20 / escritura 0x30, timeouts con -1.
  Sin IDENTIFY, sin consulta de capacidad, sin FLUSH, sin validación de rango.
- `src/kernel32/fs.c`: formato propio v1.2 (FACILOS2): cabecera LBA0
  (firma, checksum32, versión=2), tabla LBA1–6 (64×42 B), espejo LBA7–70 +
  tramos extendidos (seg+suma por fichero), pool RAM 512 sectores si no hay
  disco. `fs_disco_ok`: 0 RAM / 1 disco / 2 migrar-v1.1.
- Detección: si cabecera/checksum/tramos no cuadran → RAM limpio y el disco
  NO se toca (`fs_guardar` es no-op sin disco válido: sin formateo automático).
- VFS (`vfs.c`): tabla fina sobre fs.c; shell usa fs.c directo (documentado).
- Syscalls de ficheros 3/4/5/6/9/15/16/24 (+`guardar` de shell): todas con
  `fs_guardar()` en rutas mutantes (punto de sincronización único).

## 3. FS RAM: funciona; contratos fs.c intactos y connusados.
fds por proceso, offsets propios, lecturas por tramos de 512, escrituras con
extensión (no truncan), truncar, borrar con -2 si ocupado. Límites: 64
entradas, nombres ≤31 sin `/`, fichero ≤65535 (FS_MAX_TAM16), pool 256 KB.

## 4. Contratos VFS actuales
`vfs_ops_t` (hijo/tipo/tam/crear/leer/escribir/truncar/borrar/sincroniza),
backend único ramfs. La shell es cliente directo del backend (directorios).

## 5. Configuración QEMU real
`tools/qrun.py`: `-drive file=HDD,format=raw,if=ide,index=0,media=disk`
(disco primario maestro = objetivo de ata.c) + `-boot order=d`. Evidencia
previa con disco: `build/evidence/evidencias-v1.8/serie-con-disco.log`.
Imágenes existentes `build/images/*datos*` (origen v1.5–v1.7); herramientas
`mkdatos.py`/`inyecta.py` con rutas/nombres obsoletos.

## 6. Opciones de bloques (Fase 2)
Elegido: **ATA PIO existente** (compatible QEMU, simple, probado en v1.8,
sin firmware tras arranque). AHCI: complejidad sin necesidad (sin DMA/NCQ
requeridos, sin SMP). Sin segundo controlador en esta etapa.

## 7. Comparación de formatos (Fase 4)
Elegido: **formato propio v1.2 existente** (ya integrado, pequeño,
checksums, 64 entradas bastan para shell+apps). FAT12/16: más código
(BPB/FAT/cadenas), sin beneficio para el caso de uso; se descarta con esta
justificación. Sin bump de versión (compatibilidad total).

## 8. Propuesta de arquitectura (por entregas)
E1 (esta): informe. E2: `blk` (IDENTIFY+capacidad+rangos+flush) sobre ata.c
sin cambiar su PIO. E3: doc de formato + `tools/voldisco.py` (mkfs,
verifica, lista). E4: montaje/integración (guardas mínimas; fs.c intacto
salvo defecto probado). E5: app `disco32` + baterías A–H. E6: interrupción
(kill -9) + montaje/verificador. E7: regresión + ISO etapa3 + 10 documentos.

## 9. Riesgos
- Sin FLUSH: ventana de caché QEMU (mitiga E2).
- Escritura parcial sin journal (diseño: checksum al montar + verificador;
  documentado, no prometido auto-reparo).
- Pool RAM y espejo: doble escritura en `fs_guardar` (rendimiento, no
  corrección).
- `inyecta.py`/`mkdatos.py` obsoletos (no tocar; herramienta nueva aparte).

## 10. Plan de implementación
Orden E1→E7 con prueba por entrega; ABI/syscalls sin cambios previstos;
VFS sin cambios previstos; fs.c solo ante defecto probado.
