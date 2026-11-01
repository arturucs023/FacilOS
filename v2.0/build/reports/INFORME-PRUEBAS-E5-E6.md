# INFORME-PRUEBAS-E5-E6 (Fases Q/R/S)

Convención: funcional = píxeles + serie; capturas = complemento.
Objeto `qrun.py`: bug Enter-fantasma en llaves (corregido, ver §7).

## Funcionales E5/E6 (todas SUPERADA salvo nota)
| Prueba | Evidencia |
|---|---|
| Menu abrir/navegar/lanzar (teclado+ratón) | v50m3, v50_exp3, v50_cfg |
| Terminal builtins + ejecutar + rc | v50t5, v50final, v50gb2 |
| Explorador CRUD + visor + CD | exp4/8, mkd/ren/crud, vw0/1 |
| Sysinfo datos vivos | v50sys2 |
| Editor guardar + clic + aviso | v50ed2, per2/3 |
| Cursor/mover/MOU/clic-foco | v50e4a-e, v50_cur0, v50_btn2 |
| Drag/resize/min/max-close/[X] | drag/closed/min/rest/rsz |
| Muerte sucia barriada (gbye) | v50gb2 (rc=0) |
| Persistencia reboot | v50_per2/3 + voldisco |
| Estrés 3 apps + tormenta + ESC | v50_stress (+sucio/mem limpios) |

## Regresión (binario final salvo ISO)
| Prueba | Resultado |
|---|---|
| aislamiento32 8/8, equidad32 3/3 | SUPERADA |
| ipc32 111, t1_lib 258 | SUPERADA |
| sucio32 46/46 (nuevo raton+dir+reloj), mem32 15/15 | SUPERADA |
| estres_ipc 27/27*, estres_mix 8/8, estres_mem 12/12* | SUPERADA* |
| disco32 38/38 (RAM) | SUPERADA |
| t2_gfx 11/12 | FALLIDA conocida (banda1-azul, preexistente) |
| shell/menu/texto, reloj CMOS, disco scratch | SUPERADA |

(*) Flakes preexistentes de `crear` bajo carga (p5-crear-eco, p2-crear):
1/3 y 1/2 fallos aislados; pasan al repetir sin cambios; ruta fork
intacta en E5/E6. NO ocultos: logs v50_rg3/4 y v40_reg4 conservados.

## Incidencias de harness (no del SO)
1. Llaves `{X}` con Enter fantasma → falsos lanzamientos/ecos. CORREGIDO.
2. `sendkey menu/apps` no inyecta IRQ en este QEMU → F1 como tecla menú
   (E0 5B/5D se conserva para HW real). DOCUMENTADO.
3. `mouse_move` manda deltas (siempre + salvo negativos, que sí van):
   posicionado relativo exacto verificado.
4. Escaneos de píxeles: exigir color exacto (gris/verde invisibles a
   escaneo solo-blanco). Metodología.
