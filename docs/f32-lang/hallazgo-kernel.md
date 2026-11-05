# F32 Lang v0.1 — Hallazgo en el núcleo (corregido)

Durante FASE 4 el intérprete fallaba con `F32-010` en la
línea 1 de cualquier programa válido. La depuración mostró
`SRC == LINI` (mismo puntero para el fuente y la tabla de
líneas): `u_malloc` devolvía memoria ya usada.

Causa en `src/kernel32/exec.c`: `fds_init()` (llamada en
cada `abrir`/`crear`, syscalls 3/9) hacía
`proc_actual->brk = UBRK_INI`, reiniciando el heap, y además
vaciaba `fds[]`. Consecuencias verificadas en QEMU:

1. `malloc → fopen → malloc` solapa bloques (heap corrupto;
   en el editor hacía desaparecer la escritura tras
   «Guardar como»).
2. Dos ficheros abiertos a la vez compartían `fd`:
   `copiar_archivo` producía ficheros vacíos con
   verificación vacuamente correcta (se demostró con
   `mostrar_contenido` vacío tras copiar "ORIGINAL").

Corrección mínima (2 eliminaciones + comentario): quitar la
llamada a `fds_init()` en `sc_03`/`sc_09`. El proceso ya nace
con `brk` y `fds` inicializados (`proc_crea`; tarea 0 al
arrancar), así que el reinicio era innecesario y contrario al
diseño («heap monótono», `proc.c`). Verificado: el editor
vuelve a escribir tras guardar, la copia conserva contenido,
y toda la regresión sigue verde.
