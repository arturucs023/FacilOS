# REFERENCIA-COMANDOS-v2.2 (generada del registro CMDS de terminal.c)

Valores si/no en castellano. Rutas con espacios entre comillas.
Errores comunes: TER-0010 falta argumento, TER-0040 comando
desconocido (+sugerencias), TER-0041 comillas, TER-0043/0044/0046/0047
parametros, TER-0015 ya existe, TER-0020 no existe, TER-0023 carpeta
no vacia.

Varios comandos en una linea con `|` (secuencial, sin tuberia de
datos: cada uno escribe a consola en orden; respeta comillas).
Ej: `eco uno | eco dos`. Un `salir` en la cadena es el ultimo
efecto. Sin anidar `repetir` ni `salir` dentro de `repetir`.

## ayuda [comando] (documentacion)
Muestra el catalogo por categorias; con nombre da su ayuda breve.
Ej: `ayuda`, `ayuda manual`.

## manual <comando> (documentacion)
Ficha completa: categoria, descripcion, sintaxis, opciones (tipo,
valores, defecto), ejemplo. Ej: `manual crear_carpeta`. Tiene manual
propio. Inexistente: mensaje + sugerencias.

## crear_carpeta "nombre" [crear_padres=si|no] (carpetas)
Crea carpeta; crear_padres=si (defecto) crea intermedias.
Ej: `crear_carpeta "Documentos"`.

## crear_archivo "nombre" (archivos)
Crea archivo vacio. Ej: `crear_archivo "notas.txt"`.

## eliminar_archivo "nombre" (archivos)
Elimina un archivo (nombre exacto, sin papelera).
Ej: `eliminar_archivo "viejo.txt"`.

## eliminar_carpeta "nombre" (carpetas)
Solo vacia (el nucleo la protege). Ej: `eliminar_carpeta "Vacia"`.

## listar_archivos [ruta] [formato=simple|detallado] [orden=nombre|tipo]
Lista carpeta (actual por defecto). Detallado: D/F/C + tamano real.
Ej: `listar_archivos formato=detallado`.

## entrar_carpeta "ruta" (carpetas)
Cambia la carpeta actual (/, ., ..). Ej: `entrar_carpeta "Documentos"`.

## mostrar_ruta (carpetas)
Escribe la carpeta actual. Ej: `mostrar_ruta` -> `/Documentos`.

## mostrar_contenido "nombre" (archivos)
Muestra texto (hasta 6000 B). Ej: `mostrar_contenido "notas.txt"`.

## copiar_archivo "origen" "destino" [sobrescribir=si|no] [verificar=si|no]
Copia bytes (destino carpeta conserva nombre). Defectos: no, si.
Ej: `copiar_archivo "a.txt" "copia.txt"`.

## mover_archivo "origen" "destino" (archivos)
Misma carpeta: renombra; otra: copia+borra sin sobrescribir.
Ej: `mover_archivo "a.txt" "Documentos/b.txt"`.

## cambiar_nombre "actual" "nuevo" (archivos)
Renombra en su carpeta (el nuevo sin ruta). Ej: `cambiar_nombre
"a.txt" "b.txt"`.

## limpiar_pantalla (sistema)
Borra el texto visible. Sin opciones.

## informacion_sistema (sistema)
Memoria libre, procesos vivos, tamano de ventana. Sin opciones.

## mostrar_fecha / mostrar_hora (sistema)
Fecha DD/MM/AAAA y hora HH:MM:SS del CMOS. Sin opciones.

## teclado [es|ca|us] (sistema)
Muestra o cambia la distribucion (persiste). Ej: `teclado es`.

## historial (sistema)
Comandos de la sesion numerados. Sin opciones.

## eco [texto...] (sistema)
Repite los argumentos. Ej: `eco hola mundo`.

## ejecutar <programa> [args] (sistema)
Lanza F32X (puente modal, rc real). Ej: `ejecutar menu`.

## procesos (sistema)
Lista los procesos actuales: pid, estado, rc, marcos y nombre.
Ej: `procesos`.

## memoria (sistema)
Marcos totales, libres y usados; RAM util y entradas E820.
Ej: `memoria`.

## repetir <n> <comando> (sistema)
Ejecuta el comando N veces seguidas (de 1 a 100, sin anidar).
Ej: `repetir 3 eco hola`.

## esperar <ms> (sistema)
Pausa de 0 a 10000 ms. Util con `|` para demoras.
Ej: `esperar 500`.

## salir (sistema)
Cierra la terminal. Sin opciones.
