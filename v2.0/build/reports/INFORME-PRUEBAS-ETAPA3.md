# INFORME-PRUEBAS-ETAPA3 (Fase 13 + 14 + 16)

Evidencias: `build/logs/v30_*.log` + `voldisco` + listados. Todo SUPERADA
salvo lo indicado. Sin PANIC en ninguna sesión.

| Prueba | Veces | Resultado | Evidencia |
|---|---|---|---|
| blk sin disco (RAM sigue) | 2 | SUPERADA | v30_blk1/3/5/6 |
| blk con disco (ATA 20480) | 3 | SUPERADA | v30_blk2/4/7 |
| `disco32` 38/38 en RAM | 1 | SUPERADA | v30_disco1 |
| `disco32` 38/38 en disco + verifica | 2 | SUPERADA | v30_disco2, kill2/3 |
| Persistencia real (2 arranques) | 1 | SUPERADA | v30_per1/2 (bytes exactos) |
| Kill -9 en escritura + reboot | 1 | SUPERADA | v30_kill4/5 (prefijo coherente) |
| Disco corrupto ×2 (rechazo + intacto) | 1 | SUPERADA | v30_cor1/2 + hash |
| Batería 8/8 (con disco) | 2 | SUPERADA | v30_reg1, bat7 |
| mem32/t1_lib/ipc32/sucio/equidad | 1–2 | SUPERADA | v30_reg1/2, wp2 |
| estres_mem/ipc/mix | 1 | SUPERADA | v30_reg2/4, mega |
| plan opt1/opt3, reloj ×2, 2 gfx | 1 | SUPERADA | v30_reg3/4 |
| Dirs+archivos (shell, disco) | 1 | SUPERADA | v30_reg3 (d1/f.txt) |
| `t2_gfx` 11/12 | 1 | FALLIDA | banda1 preexistente |

Veredicto pruebas: el sistema guarda de verdad (reinicio real verificado).
