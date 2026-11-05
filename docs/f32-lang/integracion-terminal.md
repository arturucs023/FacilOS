# F32 Lang v0.1 — Integración con la terminal

Comando nativo `ejecutar` (existente; se extiende, no se
duplica). Flujo para `.f32`:

1. Recibe nombre o ruta (`pos_arg[0]`).
2. Si no lleva extensión y existe tal cual, se usa; si no,
   se prueba añadiendo `.f32` (solo si resuelve sin
   ambigüedad: existe exactamente uno de los dos).
3. Si termina en `.f32`: convierte a absoluta con
   `a_absoluta()` (respeta el cwd del terminal) y lanza el
   intérprete `f32` con esa ruta (`u_ejecutar`, modal).
4. Si no, flujo actual de programas F32X (intacto).
5. El intérprete imprime resultados y sus errores
   (`Error F32-0NN en archivo linea N: ...`).
6. Al terminar, la terminal muestra `rc=N` (0 = `Programa
   finalizado correctamente.` + mensaje; error = mensaje
   del intérprete + `rc=1`), como con cualquier programa.

Mensajes coherentes con el estilo actual (`TER-00NN` para
fallos del propio comando; `F32-0NN` para el programa).
Ayuda de `ejecutar` actualizada (uso con `.f32` + ejemplo).

Sin cambios de kernel, ABI ni FS. Rutas con espacios,
acentos y multinivel soportadas (parser y FS ya las
manejan).
