# INFORME-PERSISTENCIA-ETAPA3 (Fase 13F / Fase 16.11)

## Prueba fundamental (reinicio real, mismo disco)
- Run 1 (`v30_per1.log`): `crear_archivo` + `escribir_archivo` (25 B) +
  lectura correcta en vivo; quit limpio de QEMU.
- Host: `verifica` válido + `lista` muestra `persist.txt tam=25 seg=64`.
- Run 2 (`v30_per2.log`, QEMU nuevo, misma imagen): `blk: disco ATA`,
  "Disco de datos listo. Archivos recuperados", `leer_archivo` devuelve
  EXACTO `CONTENIDO-PERSISTENTE-123`, listado correcto.
- Veredicto: PERSISTENCIA VALIDADA (bytes idénticos entre reinicios).

## Corte brusco (kill -9, Fase 10/13H)
(Ver INFORME-RECUPERACION-ETAPA3: resultado del corte durante `g-huge`.)
