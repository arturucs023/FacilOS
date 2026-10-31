; FacilOS32 - stubs IDT (32-bit). Ensamblar con -f win32 (COFF).
[SECTION .text]
global _idt_cargar
global _gdt_cargar
global _tss_cargar
global _entrar_usuario
global _salir_usuario
extern _gdt_ptr
extern _kern_esp_save
extern _user_rc
global _isr_comun
global _irq_comun
global _syscall_comun
extern _idt_tabla
extern _excepcion_c
extern _irq_c
extern _syscall_c

_idt_cargar:
    lidt [_idt_ptr]
    ret
_idt_ptr:
    dw 256*8-1
    dd _idt_tabla

_gdt_cargar:
    lgdt [_gdt_ptr]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

_tss_cargar:
    mov ax, 0x28
    ltr ax
    ret

_entrar_usuario:
    push ebp
    mov ebp, esp
    push ebx
    mov eax, [ebp+8]
    mov ebx, [ebp+12]
    mov [_kern_esp_save], esp
    push 0x23
    push ebx
    pushfd
    pop eax
    or eax, 0x200
    push eax
    push 0x1B
    push dword [ebp+8]
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iret

_salir_usuario:
    mov esp, [_kern_esp_save]
    pop ebx
    pop ebp
    mov eax, [_user_rc]
    sti
    ret

; stub excepcion SIN codigo de error CPU: mete 0 ficticio + numero
%macro ISR_SIN 1
global _isr%1
_isr%1:
    push dword 0
    push dword %1
    jmp _isr_comun
%endmacro
; stub excepcion CON codigo de error CPU: solo numero
%macro ISR_CON 1
global _isr%1
_isr%1:
    push dword %1
    jmp _isr_comun
%endmacro
; stub IRQ: solo numero
%macro IRQ_STUB 2
global _irq%1
_irq%1:
    push dword 0
    push dword %2
    jmp _irq_comun
%endmacro

ISR_SIN 0
ISR_SIN 1
ISR_SIN 2
ISR_SIN 3
ISR_SIN 4
ISR_SIN 5
ISR_SIN 6
ISR_SIN 7
ISR_CON 8
ISR_SIN 9
ISR_CON 10
ISR_CON 11
ISR_CON 12
ISR_CON 13
ISR_CON 14
ISR_SIN 15
ISR_SIN 16
ISR_CON 17
ISR_SIN 18
ISR_SIN 19
ISR_SIN 20
ISR_SIN 21
ISR_SIN 22
ISR_SIN 23
ISR_SIN 24
ISR_SIN 25
ISR_SIN 26
ISR_SIN 27
ISR_SIN 28
ISR_SIN 29
ISR_SIN 30
ISR_SIN 31
IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

global _isr128
_isr128:
    push dword 0
    push dword 128
    jmp _syscall_comun

_syscall_comun:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call _syscall_c
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret
global _isr_tabla
_isr_tabla:
    dd _isr0, _isr1, _isr2, _isr3, _isr4, _isr5, _isr6, _isr7
    dd _isr8, _isr9, _isr10, _isr11, _isr12, _isr13, _isr14, _isr15
    dd _isr16, _isr17, _isr18, _isr19, _isr20, _isr21, _isr22, _isr23
    dd _isr24, _isr25, _isr26, _isr27, _isr28, _isr29, _isr30, _isr31
global _irq_tabla
_irq_tabla:
    dd _irq0, _irq1, _irq2, _irq3, _irq4, _irq5, _irq6, _irq7
    dd _irq8, _irq9, _irq10, _irq11, _irq12, _irq13, _irq14, _irq15

_isr_comun:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call _excepcion_c
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret

; Retoma un contexto de anillo 0 guardado por GUARDA_GPR (ver proc.c).
; cdecl: retoma_kernel(k_ebx, k_esi, k_edi, k_ebp, esp_guard). No retorna:
; cambia a la pila guardada y el ret salta al punto de cesion.
global _retoma_kernel
_retoma_kernel:
    mov ebx, [esp+4]
    mov esi, [esp+8]
    mov edi, [esp+12]
    mov ebp, [esp+16]
    mov esp, [esp+20]
    ret

; Salta a anillo 3 con el marco ya montado (ver entra_usuario en proc.c).
; Restaura TODOS los registros de proposito general del proceso (incluidos
; eax/ecx/edx: una preempcion no debe cambiar valores vivos del usuario).
; cdecl: salta_usuario(sp, ebx, esi, edi, ebp, eax, ecx, edx). No retorna.
global _salta_usuario
_salta_usuario:
    mov eax, 0x20
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ebx, [esp+8]
    mov esi, [esp+12]
    mov edi, [esp+16]
    mov ebp, [esp+20]
    mov eax, [esp+24]
    mov ecx, [esp+28]
    mov edx, [esp+32]
    mov esp, [esp+4]
    iret

_irq_comun:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call _irq_c
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret
