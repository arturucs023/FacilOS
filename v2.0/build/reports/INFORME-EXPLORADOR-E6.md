# INFORME-EXPLORADOR-E6 (Fase K)

Estado: IMPLEMENTADO + PROBADO (`explorador`, UGFX, 500x340).

## Funciones reales (syscalls 41/42/16/43 + ficheros)
Lista FS+CD con tipo/tamaño (`dir_hijo`, CD tipo 3), selección
(teclado+ratón), entrar/atrasar (pila de 8, ruta visible), visor de texto
(2048 B, scroll), nuevo archivo (`fopen w`), nueva carpeta (`dir_crear`),
renombrar (`renombrar`, -2 si choca), borrar (`u_remove`, solo vacías),
ejecutar F32X del FS o del CD (`u_ejecutar` + SYNC). CD: lectura/
ejecución; el resto deshabilitado con mensaje. Errores a la vista
("no se abre/crea/borra", "ya existe", "CD: solo lectura").

## Pruebas (cruzadas con shell, bytes exactos)
- Lista con selección + visor de `doc.txt` del disco (529 px de texto).
- Crear `nota1` → shell `ver_archivos` lo lista.
- `mkdir cardir` → shell lo lista; rename a `car2` → shell confirma;
  `x` → desaparece (3 arranques independientes).
- Abrir CD ejecuta (vía mismo camino que terminal).

## Límites
Sin mover entre carpetas (sin syscall; `fs_mover` existe en núcleo para
futuro `mv`). Sin copiar (shell sí la tiene). Tope de vista 2048 B.
Nombres ≤31 sin espacios (regla del FS, validada en núcleo).
