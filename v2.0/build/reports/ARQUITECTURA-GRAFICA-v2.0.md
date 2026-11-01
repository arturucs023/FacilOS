# ARQUITECTURA-GRAFICA-v2.0 (Etapa 4, Fase 1: diseño)

## Decisión central (justificada)
Composición por **leases de propiedad** con CERO cambios de kernel:
el servidor retoma la propiedad cada frame (`texto`, idempotente si ya es
dueño); cada cliente dibuja su contenido solo al recibir `DRAW` (toma la
propiedad con su primer `texto`, dibuja, cede). Alternativa descartada por
ahora: pin de propiedad + present de buffer de usuario (2 syscalls nuevas);
solo si los tests demuestran insuficiencia (fallback documentado).
Coherente con el modelo actual (menú XOR shell; ahora escritorio XOR gfx
legado). Sin syscalls nuevas en E2/E3 (reservadas 38/39 para ratón en E4).

## Capas y responsabilidades
- Driver pantalla (Ring0, existe): LFB XRGB 32bpp + primitivas con clip.
- Servidor (Ring3, nuevo `servidor`): dueñez LFB, tabla ventanas
  {id,dueño,x,y,w,h,z,visible,minimizada,foco}, orden Z, sucios, chrome
  (título/bordes), cursor, hit-test, foco, leases DRAW, rutas de eventos,
  launcher (crear/esperar).
- Clientes (Ring3): backbuffer NO (v1: dibujo directo en lease); registran
  (id,w,h), dibujan contenido al `DRAW x y`, reciben `KEY/MOUSE/FOCUS/QUIT`.
- Superficies = memoria de usuario del cliente (aislamiento por paginación;
  sin superficies de núcleo). Sin punteros de kernel compartidos.
- Eventos por buzones IPC 34/35 (16×64 B suficientes; sin ampliar).

## Protocolo (mensajes ≤64 B, texto)
C→S: `REG w h`, `BYE`, `ACK`. S→C: `DRAW x y`, `FOCUS 0/1`,
`KEY <code>`, `MOU <x> <y> <bot>`, `QUIT`.
Frame: servidor dibuja fondo+chrome (dueño), concede DRAW de abajo arriba,
redibuja cursor encima. Redibujado bajo demanda (sucio/input), sin vsync
(documentado). Daño acotado al frame (un cliente díscolo se corrige solo).

## Ventanas/ratón/lib/escritorio
Estados: visible/oculta/minimizada/cerrada; muerte del dueño libera todo;
dimensiones validadas (1..800×600). Ratón PS/2 (E4): driver `ps2.c`,
IRQ12, paquetes 3 B, syscall 38 bloqueante + 39 no bloqueante (únicas
syscalls nuevas justificadas). Lib usuario `ugfx` (E5): registro, clip,
texto, controles (botón/etiqueta). Escritorio (E5): fondo+barra+menú+reloj
dibujados por el servidor; launcher con crear/esperar.

## Límites aceptados v1
Sin doble búfer real (leases sin vsync); clip por convención en cliente;
teclado sin releases; sin transparencias/animaciones; gfx legado solo
fuera del escritorio; `t2_gfx` 11/12 intacto (preexistente).
