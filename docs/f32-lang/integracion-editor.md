# F32 Lang v0.1 — Integración con el editor

Estado v0.1: implementado y probado (ver `pruebas-f32.md`):
auto-indent +4 tras apertura, dedent de cierres al
completarlos, Backspace y May+Tab (0xE121) por niveles,
`.f32` como «Texto» en el explorador. Sin coloreado (fase
futura, por estabilidad).

Sin reescribir el editor (v2.2-editor `29eb475` intacto en
funcionalidad). Cambios mínimos aplicados:

Sin reescribir el editor (v2.2-editor `29eb475` intacto en
funcionalidad). Solo indentación + reconocimiento mínimo.

## 1. Indentación automática (4 espacios)

En `tecla_doc`, tras Enter: copiar la indentación de la
línea anterior y +4 si termina (tras recortar) en
`programa/si/sinosi/sino/mientras/funcion`.
Si la línea anterior es un cierre, no se añade nivel.
Al escribir `finsi/finmientras/finfuncion/fin` como única
palabra de la línea, realinearla al nivel de su apertura
(búsqueda hacia atrás del abridor no cerrado).
Backspace al inicio de la zona de indentación: borrar hasta
el múltiplo de 4 anterior.
Shift+Tab: reducir un nivel (requiere código de driver
`0xE121` May+Tab, 3 líneas espejo de `0xE120`; cambio mínimo
ya aceptado en esta línea de trabajo).

## 2. Reconocimiento de extension

`explorador.c`: añadir `"f32"` a la lista de texto de
`icono_de`/`texto_tipo` (hoja + "Texto"). El editor ya abre
cualquier tipo 2 por argumento/ruta.

## 3. Decisión estudiada: sin coloreado en v0.1

El editor no tiene framework de resaltado léxico (su color
es formato de usuario por byte). Añadir lexer+repintado
arriesga la estabilidad por un beneficio cosmético: se
pospone (futuro: colorear palabras de `palabras-reservadas.md`
con el sistema de estilos existente + autocompletado +
marcado de la línea de error F32). La indentación sí entra
en v0.1 porque usa el pipeline de edición ya probado.
