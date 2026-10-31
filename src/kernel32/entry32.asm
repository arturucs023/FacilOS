[BITS 32]
[SECTION .text.start]
global _kmain_entry
extern _kmain
extern __bss_start
extern __bss_end
_kmain_entry:
    ; v1.9: 64 KB de pila de arranque (antes 4 KB: el nucleo de v1.9
    ; anida mucho mas al cargar procesos y llamar a la shell)
    mov esp, 0xA0000
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    shr ecx, 2
    xor eax, eax
    rep stosd
    call _kmain
    cli
.halt:
    hlt
    jmp .halt
