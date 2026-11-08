; F7-C: copia la cola alta del catalogo (tras la pila 0x90000).
; Vive en el sector de relleno LBA 224 del area del nucleo
; (cargado en 0x1000:0x1BE00); el arranque la llama con call far
; (call 0x1000:0x1BE00) y vuelve con retf. Autocontenida.
; Lee [CORTE_LBA, fin) por CHS via rebote 0x8000 y lo copia a
; 0x1000000 con DS/ES en irreal (GDT propia). Preserva todo.
[BITS 16]
[org 0xBE00]

CORTE_LBA equ 993
ALTO_BASE equ 0x1000000

copy_alta:
    pushad
    push ds
    push es
    pushfd
    cli
    xor ax, ax
    mov ds, ax
    mov ax, [0x7DF0]        ; pay_secs (sector arranque+496)
    mov cx, [0x7DF2]        ; cat_lba (sector arranque+498)
    add ax, cx              ; ax = fin (ultimo LBA + 1)
    sub ax, CORTE_LBA       ; ax = sectores altos
    jbe .fin                ; sin cola
    mov [cs:alto_n], ax
    mov word [cs:cur_lba], CORTE_LBA
    ; GDT propia de 4GB (limite + base lineal)
    mov word [cs:mi_gdt], 23
    mov dword [cs:mi_gdt + 2], mi_gdt_tab + 0x20000
    lgdt [cs:mi_gdt]
.bucle:
    xor bx, bx               ; segmentos normales para el BIOS
    mov ds, bx
    mov es, bx
    mov ax, [cs:cur_lba]
    mov di, ax
    mov cx, 36
    xor dx, dx
    div cx                   ; ax=cilindro, dx=resto
    push ax
    xchg ax, dx
    xor dx, dx
    mov cx, 18
    div cx                   ; ax=cabezal, dx=sector-0
    mov dh, al
    mov cl, dl
    inc cl
    pop ax
    mov ch, al
    mov dl, [0x7000]         ; unidad de arranque (ds=0)
    mov ax, 0x0201
    mov bx, 0x0800           ; rebote 0x8000 (libre en arranque)
    mov es, bx
    xor bx, bx
    int 0x13
    jnc .leido
    mov dx, 0x3F8            ; '3': error leyendo la cola
    mov al, '3'
    out dx, al
    jmp .fin                 ; sigo sin la cola
.leido:
    mov eax, cr0             ; irreal: DS/ES limite 4GB
    or al, 1
    mov cr0, eax
    mov bx, 0x10
    mov ds, bx
    mov es, bx
    and al, 0xFE
    mov cr0, eax
    mov esi, 0x8000          ; rebote -> alto (32 bits)
    mov edi, [cs:alto_dst]
    mov ecx, 128
    a32 rep movsd
    xor bx, bx               ; DS/ES normales otra vez
    mov ds, bx
    mov es, bx
    mov eax, [cs:alto_dst]
    add eax, 512
    mov [cs:alto_dst], eax
    mov ax, [cs:cur_lba]
    inc ax
    mov [cs:cur_lba], ax
    dec word [cs:alto_n]
    jnz .bucle
.fin:
    xor ax, ax
    mov ds, ax
    mov es, ax
    popfd
    pop es
    pop ds
    popad
    retf

mi_gdt:
    dw 0
    dd 0
mi_gdt_tab:
    dd 0, 0
    dd 0x0000FFFF, 0x00CF9A00
    dd 0x0000FFFF, 0x00CF9300

alto_dst dd ALTO_BASE
alto_n dw 0
cur_lba dw 0
