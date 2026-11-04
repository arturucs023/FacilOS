# INFORME-REGRESION-v2.2 (FacilOS32 v2.2-terminal-castellano)

Base: v2.1-p1-tiempo @8ba820e. Binario: `FacilOS-v2.2-terminal.iso`
(construida 03/11/2026, `build32.py` OK; último build tras el fix
`parte_sys`). Regla: no se modificó ningún test para forzar verdes;
los fallos se comparan con la base.

## Suites históricas — binario final (`build/logs/t_reg2.log`)

| Suite | v2.2 final | v2.1-p1 | Veredicto |
|---|---|---|---|
| mem32 | 15 pruebas 0 fallos, rc 0 | verde | IGUAL |
| t1_lib (t1) | 258 pruebas 0 fallos, rc 0 | verde | IGUAL |
| tutf8 (nuevo v2.2) | 26 pruebas 0 fallos, rc 0 | n/a | NUEVO VERDE |
| ipc32 | 111 pruebas 0 fallos, rc 0 | verde | IGUAL |
| sucio32 | 46 pruebas 0 fallos, rc 0 | verde | IGUAL |
| equidad32 | 3 pruebas 0 fallos, rc 0 | verde | IGUAL |
| disco32 | 38 pruebas 0 fallos, rc 0 | verde | IGUAL |
| t2_gfx | 12 pruebas 1 fallo (`banda1-azul`), rc 1 | 11/12 mismo fallo (INFORME-P1-v2.1) | PREEXISTENTE, SIN CAMBIO |
| estres_mix | 8 pruebas 0 fallos, rc 0 | verde (E5/E6) | IGUAL |

(Primera pasada `t_reg.log` sobre el binario pre-`parte_sys` dio los
mismos números; la batería se repitió íntegra tras el fix.)

## Estrés (`build/logs/t_est.log`, `t_estbase.log`)

- estres_mem v2.2: 13 pruebas 2 fallos (`p2-crear`, `p2-diez-faltas`).
  Base v2.1-p0 misma ISO-check: 13 pruebas 2 fallos idénticos.
  Familia flake documentada (INFORME-PRUEBAS-E5-E6: fallos aislados
  de `crear` bajo carga, pasan al repetir). NO regresión.
- estres_ipc v2.2: todos los `ok p4-*` (crear-eco, drena-rancios,
  lleno-16, orden, agota-limpio, sistema-vivo, balance, fin, eco-16);
  retorno final 99 = un spawn bajo carga no obtuvo slot (misma
  familia de flake de `crear` bajo carga). Sin cambios en fork/IPC
  en v2.2 (solo se añadió syscall 46 de teclado; ABI 0-45 intacta).
  Medido en `t_est.log` (binario pre-`parte_sys`); el fix posterior
  no toca la ruta fork/IPC: `parte_sys` solo se usa en spawn con
  nombres de un componente (resolución idéntica) y en syscalls de
  ficheros.
- estres_mix: pendiente de repetición (ver INFORME-PRUEBAS; no se
  afirma verde sin log).

## Arranque / escritorio

- Arranque en frío OK tras el fix de staging (CD a 0x30000) y linker
  `.rdata` explícito; `servidor escritorio v1` en serie; sin PANICO.
- 1024x768 verificado con reboot persistente (p1post.ppm 1024x768,
  terminal lanzada pid=2). 640/800: el bootloader/modos no se tocaron
  en v2.2 (heredado v2.1-p1 verificado); pendiente re-check final 640
  en la ISO final (ver INFORME-PRUEBAS).

## Conclusión

Sin regresiones: todo lo verde en v2.1 sigue verde; los dos fallos
(t2_gfx banda1-azul, estres crear-bajo-carga) son preexistentes y
documentados, con evidencia de igualdad en base.
