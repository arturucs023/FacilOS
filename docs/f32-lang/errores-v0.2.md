# F32 Lang v0.2 — Errores nuevos

Formato y códigos de salida sin cambios (ver `errores.md`).
F32-052 (`orden < > con cadenas`) queda reservado: v0.1 lo
emitía, v0.2 permite la comparación lexicográfica y ya no lo
emite.

| Código | Significado |
|---|---|
| F32-086 | módulo no encontrado (`importar` o prefijo `mod.` desconocido) |
| F32-087 | dependencia circular entre módulos |
| F32-088 | ruta de módulo inválida (`..`, componente vacío, demasiado largo) |
| F32-100 | no se pudo abrir el archivo |
| F32-101 | fallo al leer el archivo |
| F32-102 | fallo al escribir el archivo |
| F32-103 | descriptor de archivo inválido |
| F32-104 | modo de apertura inválido (solo `lectura`, `escritura`, `anadir`) |
| F32-105 | demasiados archivos abiertos (máx. 8) |
| F32-120 | índice fuera de rango (arreglo o cadena) |
| F32-121 | no se puede asignar ahí (destino no indexable / punto sobre no-registro) |
| F32-122 | campo inexistente en registro |
