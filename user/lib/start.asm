[BITS 32]
[SECTION .text]
global _start
extern _main
_start:
    call _main
    mov ebx, eax
    xor eax, eax
    int 0x80
.hang:
    hlt
    jmp .hang
