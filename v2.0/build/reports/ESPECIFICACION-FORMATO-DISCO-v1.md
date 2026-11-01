# ESPECIFICACION-FORMATO-DISCO-v1 (FACILOS2 / v1.2)

Formato propio de FacilOS32 (Etapa 3 lo documenta; no lo cambia). Todo
little-endian, sectores de 512 B, tipos de tamaño fijo. Referencia:
`src/kernel32/fs.{c,h}`.

## Cabecera (LBA 0, 512 B)

| Offset | Tamaño | Campo |
|---|---|---|
| 0 | 8 | firma `"FACILOS2"` |
| 8 | 2 | checksum_lo (LE16) |
| 10 | 2 | checksum_hi (LE16) |
| 12 | 1 | versión (=2) |
| 13 | 1 | reservado (=0) |
| 14 | 1 | teclado (0 US / 1 ES) |
| 15..512 | — | cero |

`checksum = suma(tabla 2688 B + espejo 64×512 B) & 0xFFFFFFFF`.

## Tabla (LBA 1–6, 64 entradas × 42 B)

| Offset | Tamaño | Campo |
|---|---|---|
| 0 | 1 | tipo (0 libre, 1 directorio, 2 fichero) |
| 1 | 1 | padre (índice) |
| 2 | 32 | nombre (NUL, sin `/`, ≤31 significativos) |
| 34 | 2 | tamaño LE16 (≤65535) |
| 36 | 2 | seg_ini LE16 (0 = heredado: un sector en idx) |
| 38 | 4 | suma LE32 (suma de los `tam` primeros bytes) |

Raíz: idx 0, tipo 1, nombre `/`.

## Datos (desde LBA 7)

- Fichero heredado (`seg_ini`=0, `tam`≤512): sector `7+idx` (espejo RAM,
  write-back en `fs_guardar`).
- Fichero extendido: `tam`>512 exige tramo `[seg_ini, +nsec)` con
  `seg_ini`≥64 (`FS_SEG_MIN`); sector `7+ds` directo en disco
  (write-through) o pool RAM sin disco. `FS_LBA_MAX`=20480 (10 MB).
- Tramos: first-fit en [64, sec_max), sin solapes; suma verificada al montar.

## Montaje
Válido ⟺ firma + versión=2 + checksum + raíz tipo 1 + tramos/sumas OK.
Si no: RAM limpio y el disco NO se toca (sin formateo automático).
Migración v1.1→RAM solo en memoria; persiste al primer `guardar` explícito.
