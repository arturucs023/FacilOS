# Arquitectura del kernel v2.0

```
BIOS → boot32 (El Torito) → entry32 → kmain
  mm(kheap) → pag(marcos+dueños, PD/PT, VMA, ventana, WP) → proc(tabla, RR)
  → idt(PIC+PIT+puertas) → seg(GDT/TSS) → sti → ktimer_test
  → drv(pantalla, teclado, pit, ramfs) → cons → cdcat → menu(proc) → shell
INT 0x80 → tabla sc_[] → handlers (ramfs vía VFS, gfx con dueño, IPC, mem)
IRQ PIT → ticks, kt_tick, fusiona diferida, preempción Ring3
```

## Módulos y contratos

| Módulo | Ficheros | Interfaz pública | Invariantes |
|---|---|---|---|
| arranque | boot32.asm, entry32.asm, link32.ld | núcleo en 0x100000, BSS a cero | — |
| cpu/seg | cpu.h, seg.c, idt32.asm | GDT/IDT/TSS, esp0 por proceso, IF-anidable | esp0 == cima del proceso en curso |
| irq | irq.c, idt.c | PIT 100 Hz, teclado, excepciones con diagnóstico | handler sin bloqueo ni alloc |
| mem física | pag.c (marcos) | toma/devuelve, dueños, picos, verifica | bit==1 ⟺ dueño!=LIBRE |
| mem virtual | pag.c (PD/PT) | dir por proceso, ventana, cuenta_rango | núcleo supervisor; sin aliasing usuario |
| kheap | mm.c | kmalloc/kfree/info/autoprueba | canarios intactos; usados==0 tras test |
| proc/planif | proc.c | RR, GUARDA/retoma/salta, VMA, fusión, stats | contexto GPR+EBP completo |
| syscall | exec.c (tabla) | ABI 0–37, puerta central, O0 | ABI 0–33 congelado |
| ipc | ipc.c | buzones 16×64, send no bloquea, recv sí | sin memoria ajena mapeada |
| ktimer | ktimer.c | una/cada/cancela/tick | callbacks triviales en IRQ |
| drivers | drv.c | registro {nombre,init}, init ordenado | inits idempotentes |
| vfs | vfs.c | ops + backend ramfs (=fs.c) | mismo comportamiento que v1.9 |
| fs/shell | fs.c, shell.c, cdcat.c | sin cambios de comportamiento | fs_init idempotente |

Dependencias: syscall→proc/pag/vfs/ipc/gfx/fs; irq→proc/ktimer; drv→gfx/teclado/idt/fs;
nadie depende de la shell (tarea 0).
