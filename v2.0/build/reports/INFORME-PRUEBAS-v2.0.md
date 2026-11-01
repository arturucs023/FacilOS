# Pruebas v2.0 (etapa 1)

Evidencias: `build/logs/v20_*.log` (serie) + `build/images/v19_*.ppm`.
Estados: SUPERADA / FALLIDA / NO EJECUTADA / BLOQUEADA.

| Prueba | Veces | Resultado | Evidencia |
|---|---|---|---|
| Compilación + ISO v2.0 (25 prog) | 10+ | SUPERADA | build exit 0 |
| Arranque (E820, pag CR0 WP, kheap, ktimer, 4 drv, 25 prog) | 25+ | SUPERADA | serie boot |
| Menú→shell, version 2.0, eco | 15+ | SUPERADA | todas las sesiones |
| Batería aislamiento32 8/8 | 6 | SUPERADA | v20_bat1/4/5/6 + wp1 |
| `mem32` 15/15 | 4 | SUPERADA | v20_mem1/2/3, wp2, bat2 |
| `equidad32` 3/3 | 3 | SUPERADA | v20_equ4, bat2/4 (+1 flake pre-fix) |
| `ipc32` 111/0 | 2 | SUPERADA | v20_ipc2, bat4/5 |
| `sucio32` 32/32 | 2 | SUPERADA | v20_sucio, bat4/5, wp1 |
| `t1_lib` 258/0 | 4 | SUPERADA | v20_mem3, bat2/3, wp2 (+1 flake aislado v20_mem2) |
| planificador32 opt1 (2 a la par) | 1 | SUPERADA | v20_final1 |
| planificador32 opt3 (20/20) | 1 | SUPERADA | v20_final2 |
| `reloj` + captura + relanzar | 3 | SUPERADA | v19_reloj.ppm, final1/2 |
| 2 gráficos simultáneos | 1 | SUPERADA* | v19_2gfx (*con limitación H) |
| `t2_gfx` 11/12 | 2 | FALLIDA | banda1 preexistente (ver Seguridad) |
| FS crear/escribir/leer/borrar | 2 | SUPERADA | v20_vfs1 |
| Balance marcos exacto | 3 | SUPERADA | 5631=5664−33 |
| 50 simultáneos | — | BLOQUEADA | límite MAXPROC=8 por diseño |

Flakes aislados (1 caso cada uno, sin mecanismo reproducible, vigilados):
`t1_lib` en v20_mem2, `equidad32` pre-fix de `salta_usuario` (causa hallada
y corregida: EAX/ECX/EDX sin restaurar). El diagnóstico contuvo ambos (solo
murió el ofensor).
