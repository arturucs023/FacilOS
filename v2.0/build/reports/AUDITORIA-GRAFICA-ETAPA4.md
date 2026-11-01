# AUDITORIA-GRAFICA-ETAPA4 (Fase 0, sin código)

Rama `v2.0-etapa4` (desde etapa3). `main`/v1.9/ISOs/informes intactos.

## 1. Estado Git
Limpio; referencia etapa3 intacta; artefactos nuevos se llamarán Etapa 4.

## 2. Arquitectura gráfica existente
Todo el dibujo vive en Ring0 (`gfx.c`): LFB 32bpp XRGB + dorso global en
0xA00000 + `gdest` global. Apps Ring3 solo vía syscalls 10–14/18–23. Sin
superficies por proceso, sin compositor, sin ventanas, sin eventos.

## 3. Framebuffer (verificado por inspección + capturas)
Base/tamaño/pitch de bootinfo (VESA 32bpp; capturas 800×600); mapeo
supervisor hasta 4 MB (Ring3 directo imposible: #PF); primitivas con
clipping; `gdest` global (no por proceso); consola siempre a LFB.

## 4. Aplicaciones gráficas
`menu` (dibuja sin búfer: destino denegado pre-propiedad; barra de título
ausente en primer frame), `reloj` (verificado por píxeles), `calculadora`,
`paint32`, `demo16`, `editor16`+`visor32` (texto). Todas toman propiedad vía
`texto` y dibujan directo a LFB (presentar es no-op sin dorso).

## 5. t2_gfx 11/12 (causa confirmada, preexistente)
Bandas/líneas/limpiar se deniegan ANTES del primer `texto` (sin propiedad);
`texto` toma propiedad (+limpia dorso inexistente=no-op); `pixel` y
`presentar` ya con propiedad pero sobre LFB (gdest=0) y dorso vacío:
banda0 negra ✓, banda1 negra ✗ (esperaba azul), esquina blanca ✓.
No es corrupción: es la política documentada. No se toca en auditoría.

## 6. Contratos IPC disponibles
Buzones 16×64 B + emisor, send no bloqueante, recv bloqueante, limpieza al
morir (`ipc32` 111/0, `estres_ipc` 27/27). Suficientes para eventos sin
ampliar (a justificar si hiciera falta).

## 7. Teclado
Scancodes → ASCII/extendidas (flechas 0x101+), releases ignorados (menos
shift/bloqmay), cola 64 con módulo, bloqueo con despertar dedicado.
Compatible con shell (no tocar sin necesidad). Sin eventos de release.

## 8. Ratón en QEMU (posibilidades)
QEMU pc emula i8042 con auxiliar PS/2; el kernel lo deja enmascarado
(esclavo=0xFF) y sin handler IRQ12. Protocolo estándar aplicable
(reset, defaults, enable reporting, paquetes de 3 B). A VERIFICAR POR
EJECUCIÓN en Fase 10 (no se afirma antes).

## 9. Diseño del servidor gráfico (propuesta a justificar en Fase 1)
Servidor Ring3 con LFB en exclusiva; clientes con backbuffer en SU memoria
(sbrk, lib de usuario); composición por el servidor con leases de
propiedad por IPC (cero cambios de kernel primero); eventos por buzones.
Solo si se demuestra insuficiente: syscall de pin de propiedad y/o de
presentación de buffer de usuario.

## 10. Diseño del compositor (propuesta)
Lógico en el servidor (z-order, visibles, rectángulos sucios) + ejecución
vía leases (cada cliente presenta sus rectángulos sucios con syscalls
existentes). Sin efectos; sin vsync (documentar presentación).

## 11. Diseño del gestor de ventanas (propuesta)
Tabla en el servidor (id, dueño, x/y/w/h, z, visible, minimizada);
crear/cerrar/mover/redimensionar/foco; muerte del dueño libera todo;
dimensiones validadas. Sin animaciones ni transparencias.

## 12. Plan por entregas
E1 (esta): auditoría. E2: leases+servidor mínimo+1 app demo. E3: WM
(crear/cerrar/mover/foco) + eventos teclado. E4: ratón PS/2 + cursor.
E5: lib usuario + controles + escritorio (fondo/barra/menú/reloj).
E6: apps (terminal, explorador sobre disco, sysinfo) + regresión + ISO.
