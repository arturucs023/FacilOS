# Seguridad y aislamiento v2.0

## Modelo (verificado por ejecución, no absoluto)
- Ring3 con CR3 y pilas propios; núcleo/ventana/LFB solo-supervisor;
  U/S por página; CR0.WP=1 (núcleo tampoco escribe páginas RO).
- Validación central de punteros/tamaños/fds/pids en syscalls.
- IPC sin memoria ajena (copias núcleo↔usuario validadas).
- Recursos acotados (buzones 16×64, heap 1,5 MB, pilas, MAXPROC) con error
  controlado (-1/-2/-100…-105), nunca pánico por agotamiento.
- Procesos defectuosos mueren solos con diagnóstico (vector, pid, eip, esp,
  err, CR2 en #PF, CR3, estado).

## Evidencia
- Batería A–D (escritura a núcleo, memoria ajena, div0, UD): solo muere el
  hijo, sistema intacto (5 sesiones).
- `sucio32` 32/32: 30 formas de llamar mal → error controlado + núcleo
  íntegro (doble-free y anomalías a cero).
- Cero PANIC en ~40 sesiones QEMU de la etapa.

## Límites conocidos
- Sin SMAP/SMEP en 386 (se compensa validando antes de tocar usuario).
- Framebuffer compartido entre gráficos (interferencia visual posible).
- `t2_gfx` 11/12 por política de propiedad (preexistente, sin riesgo).
