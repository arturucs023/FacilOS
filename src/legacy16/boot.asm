; FacilOS - Bootloader
; Carga el kernel (sectores 2..64) a 0x7E00 usando lecturas sector por sector (CHS)
; 100% desde cero, sin Linux ni Windows. BIOS 16-bit.

[BITS 16]
[ORG 0x7C00]

inicio:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [unidad], dl

    mov si, msg_cargando
    call imprimir

    ; v1.2: primero LBA (AH=42, sin geometria); si no hay extensiones, CHS.
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [unidad]
    int 0x13
    jc .usar_chs
    cmp bx, 0xAA55
    jne .usar_chs
    test cx, 1
    jz .usar_chs
    jmp .bucle_lba

.usar_chs:
    ; Cargar sectores LBA 1..63 (63 sectores = 32256 bytes) a 0x7E00
    mov di, 1           ; LBA actual

.bucle:
    cmp di, 64
    je .ok
    mov bp, 3           ; reintentos por sector
.reintentar:
    ; destino ES:BX desde LBA + CHS recalculados (el reset puede alterar registros)
    mov ax, di
    mov bx, 512
    mul bx              ; AX = LBA*512 (DX=0: 63*512<32768)
    add ax, 0x7E00 - 512
    mov bx, ax          ; BX = 0x7E00+(LBA-1)*512
    xor ax, ax
    mov es, ax          ; ES=0
    mov ax, di
    xor dx, dx
    mov cx, 36
    div cx              ; ax = pista, dx = resto
    push ax             ; guarda pista (CH se perderia con mov cx,18)
    mov ax, dx          ; resto
    xor dx, dx
    mov cx, 18
    div cx              ; ax = cabeza, dx = resto_sector
    mov dh, al          ; cabeza
    mov cl, dl
    inc cl              ; sector = resto+1
    pop ax
    mov ch, al          ; pista (restaurada DESPUES de usar CX)
    mov dl, [unidad]
    mov ax, 0x0201      ; AH=02 leer, AL=01 (1 sector)
    int 0x13
    jnc .siguiente
    ; fallo: punto visible + reset + reintentar con todo recalculado
    push ax
    push bx
    mov ah, 0x0E
    mov al, '.'
    int 0x10
    pop bx
    pop ax
    mov ah, 0x00
    mov dl, [unidad]
    int 0x13
    dec bp
    jnz .reintentar
    jmp .error

.siguiente:
    inc di
    jmp .bucle

    ; ---- carga LBA con paquete EDD en 0x0700 ----
.bucle_lba:
    mov di, 1
.bl_bucle:
    cmp di, 64
    je .ok
    mov bp, 3
.bl_re:
    xor ax, ax
    mov ds, ax          ; DS=0 (el BIOS podria alterarlo)
    mov ax, di
    mov bx, 512
    mul bx
    add ax, 0x7E00 - 512
    mov bx, packet + 4
    mov [bx], ax        ; offset destino
    mov bx, packet + 6
    mov word [bx], 0    ; segmento 0
    mov ax, di
    mov bx, packet + 8
    mov [bx], ax        ; LBA baja
    mov bx, packet + 12
    mov word [bx], 0    ; LBA alta
    mov bx, packet + 2
    mov word [bx], 1    ; 1 sector
    mov bx, packet
    mov byte [bx], 0x10
    mov byte [bx+1], 0
    mov dl, [unidad]
    mov si, packet
    mov ah, 0x42
    int 0x13
    jnc .bl_sig
    push ax
    mov ah, 0x0E
    mov al, '.'
    int 0x10
    pop ax
    mov ah, 0x00
    mov dl, [unidad]
    int 0x13
    dec bp
    jnz .bl_re
    jmp .error
.bl_sig:
    inc di
    jmp .bl_bucle

.ok:
    mov si, msg_ok
    call imprimir
    mov dl, [unidad]    ; pasar unidad al kernel en DL
    jmp 0x0000:0x7E00

.error:
    mov si, msg_error
    call imprimir
.halt:
    cli
    hlt
    jmp .halt

imprimir:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
.rep:
    lodsb
    or al, al
    jz .fin
    int 0x10
    jmp .rep
.fin:
    popa
    ret

unidad db 0
msg_cargando db 13, 10, 'FacilOS cargando...', 13, 10, 0
msg_ok db 'Kernel cargado. Iniciando...', 13, 10, 0
msg_error db 13, 10, 'Error de disco. Reinicia.', 13, 10, 0
packet times 16 db 0

times 510-($-$$) db 0
dw 0xAA55
