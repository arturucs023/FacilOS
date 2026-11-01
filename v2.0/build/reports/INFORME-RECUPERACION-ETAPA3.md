# INFORME-RECUPERACION-ETAPA3 (Fase 10)

## Orden seguro de escritura
Sectores de datos directos (write-through en la operación) → espejo →
tabla → cabecera → FLUSH. Cada paso con error definido (-1); sin journal
por diseño justificado (monousuario, write-through, checksum al montar).

## Corte brusco real (kill -9 durante `g-huge`, `v30_kill4/5`)
- Host tras el corte: `verifica` VÁLIDO (transacción no vaciada perdida,
  estado prefijo coherente: 30 g-ficheros + persist.txt, sin `d_huge`).
- Reboot misma imagen: montaje aceptado, listado coherente, tramos
  contiguos sin solapes. Sin referencias imposibles. Sin reparación pedida
  (nada que reparar: coherente por construcción).
- No se promete recuperación automática más allá de esto.

## Discos corruptos (montaje rechazado, sin formateo automático)
- Firma rota: arranque a RAM limpio, trabajo en RAM, imagen byte-idéntica
  después (hash). `v30_cor1`.
- Checksum roto: arranque a RAM limpio, imagen byte-idéntica. `v30_cor2`.
- Verificador anfitrión rechaza ambos (`sin firma`, `checksum no cuadra`).
