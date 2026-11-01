# INFORME-SISTEMA-ARCHIVOS-ETAPA3 (E5: app + baterías)

## `disco32`: 38/38 en RAM y en disco
C (crear 3000 B + patrón exacto + parciales), M (reescritura + truncados
arriba/abajo con ceros), E (errores controlados), D (borrar+ausencia),
G (30 ficheros + 60 KB en trozos + relectura + limpieza).
Verifica contenido byte a byte, no solo retornos.

## Baterías con disco
- `v30_disco2.log`: 38/38 sobre ATA real + `verifica` posterior válido.
- `t1_lib` con disco: pendiente en regresión final (Fase 14).
- Directorios (Fase 7/13D): vía shell (`crear_carpeta`, `entrar_carpeta`,
  fichero dentro, `ver_archivos`) en la sesión de persistencia.

## Persistencia entre reinicios reales (Fase 13F/16.11)
- Run 1 (`v30_per1`): crear+escribir+leer `persist.txt` (25 B), quit.
- Host: `verifica` válido + `lista` muestra `persist.txt tam=25 seg=64`.
- Run 2, mismo disco (`v30_per2`): "Disco de datos listo", `leer_archivo`
  devuelve EXACTO `CONTENIDO-PERSISTENTE-123`, listado correcto.
