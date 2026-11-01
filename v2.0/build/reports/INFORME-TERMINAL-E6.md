# INFORME-TERMINAL-E6 (Fase J)

Estado: IMPLEMENTADA + PROBADA (`terminal`, UGFX, 480x320).

## Funciones reales
Ventana propia, scrollback 120x64, entrada con cursor/inserción/borrado/
flechas, historial (10, arriba/abajo), builtins con APIs reales: ayuda,
version (cadena del núcleo), mem (`info_memoria2`), procs (`listar`),
fecha (`reloj` CMOS), ver (lectura real, tope 3000 B), ejecutar (F32X
real por FS o CD, rc real + SYNC), limpiar, salir. Cabecera con `srv=N`
(pid necesario para lanzar apps hermanas a mano).

## Límites honestos
Sin tuberías ni captura de salida (ABI sin pipes): la salida del hijo va
a consola y se repinta al terminar (SYNC). Sin builtins de shell (viven
en Ring0 modo texto). Sin Tab (ciclo foco) ni ESC (salir escritorio).

## Pruebas
- `version`/`mem` con salida visible por filas (`v50t5`, `v50final`).
- `ejecutar hola` (hijo normal, terminal sobrevive) y `ejecutar gbye 2`
  (muerte sucia del hijo, rc=0, sin cuelgue) (`v50gb2`).
- Una sola tecla `q~` sin duplicados ni pérdidas (`v50one`).
- Incidencia de harness: llaves `{X}` metían Enter fantasma (corregido
  en `qrun.py`; ver PRUEBAS).
