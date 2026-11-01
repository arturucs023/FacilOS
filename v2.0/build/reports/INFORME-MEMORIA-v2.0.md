# Memoria v2.0 (física, virtual, heap)

## Física (bitmap + dueños, D1)
16K marcos; clases LIBRE/RESERVA/USUARIO (16 KB); pico de usuario;
contador de doble-free (no corrompe); `pag_verifica` (coherencia);
E820 con alternativa documentada. Límites: heap usuario 1,5 MB,
pila 128 KB + guardia, MARCOS_MAX 768 por proceso (muy por encima del uso).

## Virtual (D2: sin COW/demanda/swap)
PD(12)/PT(96) estáticos; núcleo supervisor por identidad; usuario
0x200000–0x47FFFF + pila con guardia; ventana 0x40000000 cerrada por
iteración; CR0.WP=1 (D9). VMA por proceso (CODIGO/PILA/HEAP, unión por
solape) + chequeo en destrucción (presentes vs anotados → `FUGA`).
TLB invalidada por página y por CR3.

## Heap del núcleo (first-fit + canarios, D3)
Magia cabeza/pie, verificación por recorrido, stats (usados/pico/dobles/
corruptos), `mm_autoprueba` en arranque (`kheap: autoprueba=0`). Sin
buddy/slab (innecesarios). Heap de usuario separado (sbrk + ulib).

## Pruebas
- `mem32` 15/15: coherencia, crecimiento, pico, reuso sin crecer, sbrk
  enorme rechazado, anomalías a cero.
- Baterías: balance exacto 5631=5664−33; cero FUGA en ~400 destrucciones.
- sbrk sin anotar (condición invertida heredada): detectado por el chequeo
  nuevo y corregido; verificado por ejecución.
