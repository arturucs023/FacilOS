# INFORME-PERSISTENCIA-E6 (Fase T)

Estado: IMPLEMENTADA + PROBADA (disco scratch, 2 arranques, bytes exactos).

## Dispositivo
`scratch/disco-e5.img` (10 MB, v1.2, `voldisco mkfs`), FUERA del repo
(temporal del entorno). Disco de datos personal e ISOs intactos.
QEMU `-drive format=raw,if=ide`. Núcleo: `blk: disco ATA 20480`,
`Disco de datos listo`.

## Prueba (Evidencia: `v50_per2`, `v50_per3`)
1. Arranque 1 + disco: editor crea `doc.txt`, escribe `Linea persistente
   123`, clic en [Guardar]; shell `leer_archivo` → idéntico en sesión.
2. Reinicio (mismo disco, QEMU nuevo): `Disco de datos listo`;
   `leer_archivo doc.txt` → `Linea persistente 123` (idéntico).
3. `voldisco verifica` → formato válido; `ver_archivos` lista `doc.txt`.
4. Visor del explorador muestra el contenido desde disco (529 px).

## Límites
RAM backend ≤512 B por fichero (el scratch usa disco: sin tope práctico
en la prueba). Sin journaling (herencia Etapa 3). `guardar` de shell
(Etapa 3) intacto.
