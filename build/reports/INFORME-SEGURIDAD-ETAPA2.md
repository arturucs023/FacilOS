# INFORME-SEGURIDAD-ETAPA2 (Fase 12)

## Modelo (sin afirmaciones absolutas)
Ring0/Ring3 con CR3 y pilas propios; U/S por página; ventana solo-
supervisor; CR0.WP=1 (núcleo incluido); validación de punteros/tamaños/
fds/pids en syscalls; IPC sin memoria ajena; recursos acotados con error
controlado; diagnósticos con vector/pid/eip/esp/err/CR2/CR3.

## Evidencia defensiva (solo QEMU local, sin técnicas ofensivas)
- Batería A–D ×7: escritura a núcleo, lectura ajena, div0, UD → solo muere
  el ofensor (rc=-1), resto y núcleo intactos.
- `sucio32` 32/32 ×2: punteros/fds/pids/syscalls inválidas → -1/0/-100
  controlados + núcleo íntegro (doble-free y anomalías a cero).
- Tipos buscados y NO hallados: accesos indebidos, confusión user/kernel,
  desbordamientos con efecto, UAF (fusión nunca sobre el curso), doble-free
  (contador a cero), permisos excesivos, huérfanos (balance exacto).
- WP verificado: batería + sucio verdes con CR0=80010011.

## Límites
Sin SMAP/SMEP en 386 (se compensa validando); framebuffer compartido;
`memcpy` directo núcleo↔usuario solo tras validación (ventana en el resto).
