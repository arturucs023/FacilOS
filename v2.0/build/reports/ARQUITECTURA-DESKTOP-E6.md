# ARQUITECTURA-DESKTOP-E6 (diseño construido)

## Capas (separación kernel Ring0 / usuario Ring3 conservada)
Apps Ring3 → UGFX (lib usuario) → protocolo/IPC → servidor (Ring3) →
WM + compositor + eventos + superficies → framebuffer/driver (Ring0).

## Framebuffer (reutilizado, sin cambios)
VESA 32bpp XRGB 800x600 de bootinfo; primitivas con clip; propiedad por
lease (`texto` toma, resto exige); sin VSync (presentación por demanda).

## Superficies (decisión documentada)
Sin superficies de núcleo: cada cliente dibuja en lease sobre el LFB y el
servidor recompone por frame. Propiedad/aislamiento: paginación de
procesos + leases con timeout; un cliente díscolo solo mancha su frame
(se repinta). Sin punteros de kernel compartidos (solo texto IPC).

## Servidor (`servidor.c`, Ring3)
Ventanas (tabla 16, z, foco, estados), composición por leases con DRAW
secuenciado + ACK, eventos (KEY/MOU/FOCUS/QUIT), cursor 12x12 recortado,
barra de tareas, menú, reloj CMOS (syscall 44), launcher (`u_crear`).

## Syscalls nuevas justificadas (ABI 0-40 intacto)
41 `dir_hijo` (lista FS+CD, con idx), 42 `dir_crear`, 43 `renombrar`,
44 `reloj_cmos`. VFS extendido con `nombre/padre/renombrar` (mismo patrón
delegante). F1/Menu→0x105 en driver (sin ABI nueva: código de tecla).

## Límites v1
Sin maximizar; sin mover por teclado (solo drag); Tab=ciclo foco;
ESC=salir; Tab/ESC no llegan a apps; releases sin eventos; CD 48 entradas.
