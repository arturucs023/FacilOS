# INFORME-MAESTRO-ETAPA4-v2.0 (E2-E6)

## Veredicto: ETAPA 4 FUNCIONAL CON LIMITACIONES

El primer entorno grafico propio de FacilOS32 arranca, compone, responde
a teclado y raton, ejecuta 5 aplicaciones independientes con ventanas,
persiste archivos en disco y conserva shell, regresion y ABI.

## Por entregas (commits en `v2.0-etapa4-desktop`)
- E1 (Fase 0/1): auditoria + diseno por leases, cero syscalls nuevas.
- E2: servidor minimo + demo + syscall 40 (captura verificada).
- E3: foco/mover/cerrar/KEY + DRAW secuenciado (pixel verificado).
- E4 (`d8cfab8`): raton PS/2 via IOAPIC + syscalls 38/39 (hallazgo
  cascada documentado en INFORME-RATON-ETAPA4).
- E5a (`b8d0d7c`): syscalls 41-44 + VFS + ulib + sucio 46/46.
- E5b (`0a12689`): UGFX + escritorio + 5 apps + F1 + cdcat48 + fix qrun.
- E6 (`8ed6e3a` + este): robustez (no-bloqueo/DRAW optimista/cierre
  async) + gbye + CRUD + persistencia reboot + regresion + ISO etapa4.

## Mapa de documentos
UGFX/API/CONTROLES/EVENTOS/APLICACIONES (E5), DESKTOP/WM/COMPOSITOR/
TERMINAL/EXPLORADOR/PERSISTENCIA/PRUEBAS (E6), RATON (E4), ROADMAP,
este MAESTRO. Framebuffer y superficies: secciones en DESKTOP y
COMPOSITOR (no hay superficies de nucleo por diseno).

## No funciona / no probado honesto
Sin maximizar; sin mover por teclado; sin doble-clic; sin portapapeles;
sin menús contextuales; Tab/ESC no llegan a apps; editor 4 KB (512 B en
RAM); visor 2 KB; E0-Menu sin probar en QEMU (F1 sí); sin HW físico;
flakes `crear` bajo carga (2 vistos, pasan al repetir); `t2_gfx` 11/12
(preexistente, intacto).

## Artefactos
`build/iso/FacilOS-v2.0-etapa4.iso` (38 programas; etapa1-3 intactas).
Evidencia local: `build/logs/v50_*.log`, `build/images/v50*.ppm`
(ignorados por git, como en etapas anteriores). Disco scratch fuera del
repo; disco de datos personal intacto.
