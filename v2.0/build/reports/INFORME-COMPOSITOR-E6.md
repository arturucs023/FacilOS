# INFORME-COMPOSITOR-E6 (Fase E/P)

Estado: IMPLEMENTADO + PROBADO.

## Algoritmo (opaco, sin efectos)
Snapshot por z → fondo → por ventana (cromo + DRAW secuenciado con ACK)
→ barra → menú → cursor encima. Sin VSync: presentación bajo demanda
(eventos + cambio de minuto para el reloj). Sin transparencias.

## Leases y orden (herencia E2-E4, endurecido E6)
DRAW secuenciado abajo→arriba con espera acotada (muerte/BLOQ/2 s);
recién llegados (REG en espera) entran en pasada extra; SYNC fuerza
reframe; INF se atiende también en espera (deadlock detectado con
sysinfo y corregido).
Un cliente lento solo congela su ventana; uno muerto se barre; el daño
queda acotado al frame (repaint total corrige).

## Rendimiento medido (Fase P, sin FPS inventados)
- Frame vacío (fondo+barra+cursor): ms (imperceptible en capturas).
- Frame con 2 demos E4: ~1-2 s (redibujado completo 800x600 por software).
- Reloj: reframe 1/min (sin bucles activos; idle `dormir(20)`).
- Sin copias: dibujo directo en lease; mensajes ≤64 B; sin asignaciones
  por frame en el servidor (tablas estáticas).
- Límite conocido: QEMU `mouse_move` manda deltas grandes (hasta 127 por
  paquete); el cursor salta, no hay suavizado (QA, no defecto).

## Pruebas
Solape/ocultación/movimiento/destrucción en todas las sesiones E5/E6
(v50t3, v50cfg, v50drag, v50_stress): sin restos ni corrupción.
