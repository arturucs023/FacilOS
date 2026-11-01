# INFORME-GESTOR-VENTANAS-E6 (Fase E/O)

Estado: IMPLEMENTADO + PROBADO.

## Tabla (16): dueño, x/y/w/h, z, visible, minimizada, foco, título, ack_pend
- Crear (REG, cascada, clamps 120x60..800x560), cerrar ([X]→QUIT+libera
  async), minimizar ([_]/toggle en barra), restaurar (clic en barra),
  mover (drag en título), redimensionar (drag en esquina 14px, redraw
  con DRAW w h), foco (clic/Tab/FOCUS), Z (trae-frente en clic/REG).
- Sin maximizar (NO IMPLEMENTADO, documentado).

## Ciclo de vida (Fase O)
- BYE → libera; muerte (stat FIN) → libera en cada frame (`barre_muertos`);
  muerte sin BYE (`gbye`) → barrido sin cuelgue.
- Cierre asíncrono (sin `u_esperar`: colgaría con bloqueados); el cliente
  sale al desatascarse. Salida masiva (ESC) igual.
- ACK pendiente: ventana congelada (cromo sigue), se reanuda con ACK
  tardío; cliente BLOQ nunca frena al servidor.

## Pruebas (píxeles)
- Drag +50+30 exacto (`v50drag`: 7928 px en pos nueva, 0 en vieja).
- [X] cierra (`v50closed`: 0 rojo); [_] minimiza + barra restaura
  (`v50min/rest`); resize 480→520 (`v50rsz`); foco por clic (`v50e4e`,
  `v50_btn2`); foco por Tab implícito en sesiones.
- Multi-ventana sin corrupción (`v50cfg`: terminal+configura, foco rojo).
- Barrido `gbye` (`v50gb2`); estrés 3 apps + tormenta + ESC (`v50_stress`).
