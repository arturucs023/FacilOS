# INFORME-VFS-ETAPA3 (Fase 8)

## Integración sin cambios de comportamiento
La tabla `vfs_ops_t` (19 sitios migrados en `exec.c`: cargador + syscalls
3/4/5/6/9/15/16/24) delega en el backend único `ramfs` = `fs.c` intacto.
`t1_lib` 258/0 y ciclo shell sobre disco y RAM lo prueban.

## Montaje
No hay montaje múltiple en esta etapa: el backend montado es fijo
(`vfs_actual`). La selección RAM/disco vive en `fs.c` (`fs_disco_ok`
por validación al arrancar): transparente para VFS y aplicaciones.

## Directorios y rutas
Navegación (`fs_resuelve`) y gestión de directorios, deliberadamente fuera
de la tabla (backend-directo, documentado): un backend persistente futuro
añadirá ops de mkdir al extender la tabla, sin tocar syscalls.

## Pendiente (etapa posterior)
Backends persistentes adicionales, montaje múltiple, `mkdir` en tabla.
