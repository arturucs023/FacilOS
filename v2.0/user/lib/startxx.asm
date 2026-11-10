; Arranque C++ F32 (startxx.asm): igual que start.asm pero ejecuta
; los constructores globales antes de _main y los destructores
; registrados despues. No toca el flujo C (start.asm intacto).
[BITS 32]
[SECTION .text]
global _start
extern _main
extern ___f32_cxx_init
extern ___f32_cxx_fini
_start:
    call ___f32_cxx_init
    call _main
    push eax
    call ___f32_cxx_fini
    pop ebx
    xor eax, eax
    int 0x80
.hang:
    hlt
    jmp .hang
