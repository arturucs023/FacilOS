# v2.0 Etapa 1 — ENTREGA 1: auditoría, arquitectura objetivo y plan

Estado: COMPLETADA. Base: v1.9 estable (copia en `C:\Users\artur\Documents\sistema`,
intacta; trabajo en `sistema-v2.0`, git `e28077a` + esta entrega).
ISO v2.0 base: `build/iso/FacilOS-v2.0-32.iso` (1 517 568 B), arranque + shell
verificados en QEMU (`build/logs/v20_boot1.log`).

## 1. Arquitectura real v1.9 (confirmada por inspección + ejecución)

```
BIOS → boot32.asm (El Torito, CHS) → entry32 → kmain
  mm → pag(E820, marcos bitmap, PD/PT, ventana 0x40000000, CR3) → proc tabla
  → idt(PIC+PIT 100Hz, INT 0x80) → seg(GDT/TSS) → cdcat → fs(RAM) → menu(proc)
  → shell(tarea 0, Ring0) ↔ procs usuario (Ring3, dir. propia, pila k propia)
Syscalls 0–33 (INT 0x80, O0) · IRQ: PIT keyboard · Gfx: LFB+dorso+dueño
```

Memoria física: bitmap 16K marcos, reservas fijas, E820. Virtual: PD(12)/PT(96)
estáticos, identidad de núcleo supervisor, usuario 0x200000–0x47FFFF + pila con
guardia. Contexto: GUARDA (ESP+EBX+ESI+EDI+EBP) / retoma_kernel (NASM) /
entra_usuario (iret). Planificador: RR sobre PIT + cesión voluntaria, estados
5+reciclable, fusión diferida en IRQ. FS RAM propio + F32C de solo lectura.

## 2. Arquitectura objetivo v2.0 (módulos, sin capas artificiales)

| Módulo | Ficheros | Responsabilidad / interfaz pública |
|---|---|---|
| arranque | boot32.asm, entry32.asm, link32.ld | Cargar núcleo a 0x100000, BSS, pila |
| cpu/seg | cpu.h, seg.c, idt32.asm | GDT/IDT/TSS, esp0, secciones IF-anidables |
| irq | irq.c, idt.c | PIT/teclado, excepciones con diagnóstico |
| mem física | pag.c (marcos) | bitmap + clase de dueño + stats (nuevo) |
| mem virtual | pag.c (PD/PT) | dirs por proceso + tabla VMA por proceso (nuevo) |
| kheap | mm.c | first-fit + canarios + stats (nuevo) |
| proc/planif | proc.c | RR + stats, ciclo de vida, fusión diferida |
| syscall | exec.c | tabla central + validador (nuevo), ABI 0–33 intacto |
| ipc | ipc.c (nuevo) | buzones acotados, send/recv (34/35) |
| ktimer | ktimer.c (nuevo) | temporizadores de núcleo (lista en PIT) |
| drivers | drv.c (nuevo) | registro {nombre,init} consola/teclado/pit/pantalla/ram |
| vfs | vfs.c (nuevo) | tabla de ops, backend ramfs = fs.c actual |
| fs | fs.c/cdcat | sin cambios de comportamiento |
| shell/apps | shell.c, user/ | ABI compatible; apps nuevas: ipc_hijo, ipc32, sucio32 |

## 3. Decisiones (con justificación)

- D1 Bitmap de marcos: SE MANTIENE (probado, O(16K) despreciable). Se añade
  clase de dueño/byte para detectar doble-free y fugas. No hay lista libre
  (coste sin beneficio a esta escala).
- D2 Sin COW/demanda/swap: la arquitectura no está preparada; solo regiones
  VMA de seguimiento (código/pila/heap/args) + verificación al destruir.
- D3 Heap: first-fit + canarios + stats. No buddy/slab (innecesarios: pocas
  docenas de bloques, tamaños pequeños).
- D4 Planificador: SE MANTIENE RR (equitativo, probado). Estudiadas
  prioridades: rechazadas para etapa 1 (riesgo de inanición/complejidad sin
  necesidad demostrada). Se añaden stats (ticks/cedes) + test de equidad.
  MAXPROC=8 se mantiene (PD 12, PT 96, pilas 32 y marcos dan margen, pero sin
  necesidad no se toca).
- D5 IPC: buzones acotados por proceso (16×64 B) + bloqueo con despertar.
  Rechazados pipes/señales/shm en etapa 1 (superficie mayor, sin caso de uso).
- D6 Syscalls: tabla central SÍ (ampliación segura), ABI 0–33 CONGELADO,
  nuevas 34/35 (ipc) + 36 (mem_info2). O0 intocable.
- D7 fusión diferida en IRQ: SE MANTIENE (segura tras IF-anidable); sin cola
  DPC (prematura).
- D8 VFS: capa fina de ops con backend ramfs; fs.c intacto; se migran solo
  las operaciones de fd de exec.c. Sin driver ATA (no imprescindible).
- D9 CR0.WP=1: se prueba; si la batería completa sigue verde, se queda
  (endurece escrituras del núcleo a páginas RO).
- D10 Gráficos: sin cambios de comportamiento; solo documento de propiedad
  del framebuffer + diseño de eventos (etapa posterior).

## 4. Plan de migración y compatibilidad

- ABI usuario 0–33 congelado (`user/lib/sys.h`): solo ADICIONES (34/35/36).
- F32X/F32C, layout de memoria de usuario y semántica FS: sin cambios.
- Regresión v1.9 obligatoria tras cada entrega (batería 8/8, t1_lib 258/0,
  planificador, reloj, FS). Criterio de avance: 0 PANIC + regresión verde.
- Riesgos conocidos: ventanas de direccionamiento en asm (mitigado con O0 +
  pruebas), bitmaps compartidos IRQ (mitigado con IF-anidable).

## 5. Pruebas de esta entrega

- Compilación v2.0: SUPERADA. ISO con nombre propio: SUPERADA.
- Arranque + `version`: SUPERADA (`v20_boot1.log`).
- Árbol v1.9 intacto (sin modificar): verificado por inspección.
