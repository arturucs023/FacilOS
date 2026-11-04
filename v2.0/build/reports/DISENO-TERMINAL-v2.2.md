# DISENO-TERMINAL-v2.2

## Modelo
App Ring3 (terminal.c) sobre UGFX/utext/ulog. Sin interprete en kernel.
Prompt `FacilOS32> `. Buffer linea 512 B (UTF-8, cursor en bytes con
navegacion por caracter). Historial 20 con borrador preservado.
Scrollback 160x96.

## Sintaxis
`comando [posicionales] [nombre=valor]`; comillas `"..."` agrupan
(incluidos espacios y Unicode); booleanos `si`/`no`. Sin opciones de
una letra, sin sintaxis Linux. Errores: comillas sin cerrar, param sin
nombre, duplicado, desconocido (lista validos), valor no admitido
(lista admitidos), falta/sobra argumento (con Uso+Ejemplo). Sin
globbing ni tuberias (fuera de alcance, documentado).

## Registro central (`CMDS`)
nombre/categoria/breve/detalle/uso/ejemplo/min/max/params (nombre,
tipo, defecto, valores|). Alimenta ayuda, manual, TAB y validacion.
La ejecucion revalida por su cuenta (FS).

## TAB visual
Opt-in servidor `EDIT 1` (TAB+ESC llegan; driver ya emite TAB).
Contextos: comando, opciones (nombre=), valores (de la lista del
registro), rutas (reales del FS, con / para carpetas), comandos para
manual/ayuda. 1 coincidencia: completa; N: selector navegable
(arriba/abajo/ENTER completa sin ejecutar/ESC cierra/TAB cicla).
Dibuja dentro de la ventana (6 filas + scroll, respeta tamano).

## CWD propio
Indice + pila de nombres (max 8); resolvedor con /, ., .., comillas
ya quitadas; no toca el dir global del nucleo. mostrar_ruta desde la
pila. todas las operaciones usan rutas absolutas reconstruidas.

## Comandos (todos con operacion real)
ayuda, manual, crear_carpeta (+crear_padres), crear_archivo,
eliminar_archivo/carpeta (carpeta solo vacia, exige el kernel),
listar_archivos (formato simple/detallado con tamanos reales,
orden nombre/tipo), entrar_carpeta, mostrar_ruta, mostrar_contenido,
copiar_archivo (+sobrescribir/verificar por sumas), mover_archivo
(misma carpeta renombra; otra copia+borra sin sobrescribir),
cambiar_nombre, limpiar_pantalla, informacion_sistema,
mostrar_fecha/hora, teclado, historial, eco, ejecutar (puente F32X),
salir. Sin entrar global, sin red, sin paginador externo (el
scrollback de la ventana actua).

## Registros (ulog)
`[AAAA/MM/DD HH:MM:SS] [NIVEL] [TERMINAL] TER-NNNN: mensaje` (INFO,
AVISO, ERROR, DEPUR, CRITICO). Reloj RTC (ano 2 digitos -> +2000).
Kernel serial intacto (lo usan los tests).
