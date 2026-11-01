# INFORME-APLICACIONES-E5 (fondo común E6)

Estado por app (todas Ring3, UGFX, procesos independientes):

| App | Estado | Evidencia |
|---|---|---|
| terminal | IMPLEMENTADA+PROBADA (builtins, historial, ejecutar+rc+SYNC) | v50t5, v50final, v50gb2 (rc=0) |
| explorador | IMPLEMENTADA+PROBADA (lista FS+CD, CRUD, visor, ejecuta CD) | v50exp3/4/8, mkd/ren/crud, vw0/1 |
| sysinfo | IMPLEMENTADA+PROBADA (datos vivos: mem, procs, INF, reloj, FS) | v50sys2, v50cfg |
| editor | IMPLEMENTADA+PROBADA (abrir/crear, editar, guardar+clic, aviso) | v50ed2, per2/3 (reboot) |
| configura | IMPLEMENTADA+PROBADA (solo lectura real: pantalla, reloj, mem) | v50cfg |
| wdemo | INTACTA (testigo E4, compatible con protocolo extendido) | compilación + menú anterior |
| ugfx32 | IMPLEMENTADA, PROBADA PARCIALMENTE (ejemplo; cobertura vía apps) | compilación ver=3 |
| gbye | IMPLEMENTADA+PROBADA (muerte sucia → barrido, rc=0) | v50gb2 |

## Notas
- Terminal `ejecutar`: F32X reales (FS o CD) con rc real; sin tuberías
  (limitación ABI documentada); la salida del hijo va a consola y se
  repinta con SYNC.
- Explorador CD (tipo 3): solo lectura/ejecución; el resto deshabilitado
  con mensaje.
- Editor: buffer 4 KB; en backend RAM el fichero queda en ≤512 B y se
  informa lo escrito; doble Salir con cambios.
- Configura: sin controles ficticios (todo etiquetado como lectura v1).
