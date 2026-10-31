# v2.0 Etapa 1 — ENTREGA 7: VFS y almacenamiento

Estado: COMPLETADA (cubre Fase 11).

## 1. Problema y causa

Las syscalls de ficheros llamaban directamente a `fs_*` (RAM): un futuro
almacenamiento persistente exigiría reescribir syscalls y aplicaciones
(Fase 11). Faltaba una frontera de abstracción.

## 2. Solución (diseño D8)

- `vfs.h/vfs.c` (nuevos): tabla `vfs_ops_t` (hijo/tipo/tam/crear/
  leer_parcial/escribir_parcial/truncar/borrar/sincroniza) + backend
  `vfs_ramfs` que delega en `fs.c` SIN cambiarlo + `vfs_actual` montado.
- `exec.c`: 19 sitios migrados a `vfs_actual->` (cargador F32X y syscalls
  3/4/5/6/9/15/16/24). `fs_resuelve` queda directo (navegación de
  directorios, pendiente para backend persistente con mkdir).
- La shell sigue usando `fs_*` (cliente interno del backend RAM, incluidos
  directorios): documentado, sin cambio de comportamiento.

## 3. Pruebas (ejecución)

- `t1_lib` 258/0 (`v20_vfs1.log`): ficheros multisector, parciales, seek,
  truncar, borrar vía VFS.
- Ciclo shell crear/escribir/leer/borrar: SUPERADA (`VFS-OK` íntegro).
- Batería 8/8 (`v20_bat6.log`). Sin PANIC ni FUGA.
