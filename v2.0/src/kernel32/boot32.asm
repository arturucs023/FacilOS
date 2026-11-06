; FacilOS32 - Bootloader modo protegido (v1.5)
; Carga el nucleo (64 sectores desde LBA1) a 0x10000, lo copia a 0x100000,
; entra en Protected Mode 32-bit y salta. Solo BIOS real, sin SO.
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
    ; A20: primero el metodo del BIOS (AX=2401), si no el puerto 0x92.
    ; Sin A20 no se puede leer en 0x10000 ni escribir en el LFB.
    mov ax, 0x2401
    int 0x15
    jnc .a20ok
    in al, 0x92
    test al, 2
    jnz .a20ok
    or al, 2
    out 0x92, al
.a20ok:
    ; ---- E820 (v1.9) -------------------------------------------------
    ; Mapa de memoria del BIOS: hasta 32 entradas de 24 B en 0x5000 (libre:
    ; el BIOS ya no usa esa zona cuando estamos aqui). El numero de
    ; entradas validas queda en 0x7001 (bootinfo.e820n); si el BIOS no
    ; responde o no valida, queda 0 y el nucleo usa su alternativa segura
    ; (ver pag.c). ES=0 y DI avanza: el BIOS escribe cada entrada a ES:EDI.
    ;   El BIOS escribe cada entrada en ES:EDI y devuelve en EBX el
    ;   desplazamiento de la siguiente (0 al principio).
    mov byte [0x7020], 0xAA             ; marca de que se llego aqui
    mov word [0x7001], 0
    xor bp, bp
    xor ebx, ebx
    mov di, 0x5000
.e820_bucle:
    mov eax, 0xE820
    mov ecx, 24
    mov edx, 0x534D4150
    int 0x15
    jc .e820_fin
    cmp eax, 0x534D4150
    jne .e820_fin
    add di, 24
    inc bp
    cmp bp, 32
    jb .e820_bucle
.e820_fin:
    mov [0x7001], bp
    ; VESA con modo persistente (v2.1-p1): CMOS 0x40 guarda 0=800x600
    ; (defecto), 1=640x480, 2=1024x768 (lo escribe configura). Sin NVRAM
    ; no hay donde guardar la preferencia (ISO de solo lectura, en frio
    ; aun no hay disco). Si el modo pedido falla, se reintenta 0x143 y
    ; si no, texto 80x25.
    mov byte [0x7002], 0
    mov al, 0x40
    out 0x70, al
    in al, 0x71
    and al, 3                ; 0xFF virgen -> 3 -> defecto (ver tabla)
.modo_ok:
    mov bx, modo_tab
    xlat
    mov cl, al
    mov ch, 1
    push cx
    mov ax, 0x7800
    mov es, ax
    xor di, di
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .cargar
    mov ax, [es:0]
    test ax, 0x0080
    jz .cargar
    ; v1.9: la resolucion la valida gfx_init al arrancar (>=640x400);
    ; aqui solo hace falta LFB y 32 bpp.
    cmp byte [es:0x19], 32
    jne .cargar
.modo_save:
    ; guardar campos (ES=0x7800 valido; DS=0)
    xor ax, ax
    mov ds, ax
    mov ax, 0x7800
    mov es, ax
    mov ax, [es:0x12]
    mov [0x7004], ax
    mov ax, [es:0x14]
    mov [0x7006], ax
    mov ax, [es:0x10]
    mov [0x7008], ax
    mov al, [es:0x19]
    mov [0x7003], al
    mov ax, [es:0x28]
    mov [0x700A], ax
    mov ax, [es:0x2A]
    mov [0x700C], ax
    ; v1.9: la fuente 8x16 no se copia aqui (no cabia el sector): se
    ; guarda su direccion fisica en 0x701A/0x701C y la copia el nucleo.
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10
    mov [0x701A], es
    mov [0x701C], bp
    pop bx
    or bh, 0x40              ; LFB (3 B; equivale a or bx,0x4000)
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .cargar
    mov byte [0x7002], 1
    xor ax, ax
    mov ds, ax
    jmp .cargar
.cargar:
    ; El BIOS entrega DL = la disquete virtual de El Torito, que ES la
    ; imagen que metimos en la ISO. Ahi esta el nucleo (LBA 1..128) y,
    ; detras, el catalogo de programas. Se lee por CHS (AH=02) aqui, en
    ; MODO REAL: desde 32 bits el int 0x13 del BIOS peta (#UD), y el
    ; nucleo no lo necesita porque se lo copiamos ya en RAM.
    ;   0x30000 = catalogo + programas (copia de la imagen)
    ;   0x10000 = nucleo (se copiara a 0x100000)
    ; v2.2: el catalogo va a 0x30000 (el nucleo crecio y su carga de
    ; 160 sectores llega a 0x23FFF: en 0x20000 lo pisaba).
    ; F4: hasta 192 sectores (0x28000): sigue sin pisar 0x30000.
    ; Sin interrupciones: los IRQ del BIOS pisan la copia (probado).
    cli
    mov word [p_seg], 0x1000
    mov word [p_off], 0
    mov word [p_lba], 1
    mov word [p_cnt], 192             ; F4: nucleo hasta 192 sectores
    call leer
    mov word [p_seg], 0x3000
    mov word [p_off], 0
    mov ax, [cat_lba]
    mov [p_lba], ax
    mov ax, [pay_secs]
    mov [p_cnt], ax
    call leer
.ok:
    mov ax, [cat_lba]
    mov [0x7010], ax
    mov ax, [dat_lba]
    mov [0x7018], ax
    mov [0x7000], dl        ; unidad de arranque
    cli
    lgdt [cs:gdt_ptr]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x08:pm_entry

; leer: copia p_cnt sectores (CHS) desde p_lba a p_seg:p_off.
leer:
    mov ax, [p_seg]
    mov es, ax
    mov bx, [p_off]
    mov di, [p_lba]
leer_bucle:
    mov ax, di
    xor dx, dx
    mov cx, 36
    div cx
    push ax
    xchg ax, dx                  ; ax = resto
    xor dx, dx
    mov cx, 18
    div cx
    mov dh, al
    mov cl, dl
    inc cl
    pop ax
    mov ch, al
    mov dl, [unidad]
    mov ax, 0x0201
    int 0x13
    jnc leer_ok
    mov dx, 0x3F8                 ; error: '2' por serie (pantalla ya en
    mov al, '2'                   ; grafico o el kernel ni arranco)
    out dx, al
    jmp inicio_error
leer_ok:
    add bx, 512
    jnc leer_sin
    mov ax, es
    add ax, 0x1000
    mov es, ax
leer_sin:
    inc di
    dec word [p_cnt]
    jnz leer_bucle
    ret

inicio_error:
    cli
.halt:
    hlt
    jmp .halt
[BITS 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, 32768
    rep movsd
    jmp 0x08:0x100000
[BITS 16]
unidad db 0
gdt_ptr:
    dw gdt_fin - gdt - 1
    dd gdt
gdt:
    dd 0, 0
    dd 0x0000FFFF, 0x00CF9A00
    dd 0x0000FFFF, 0x00CF9300
gdt_fin:
; v2.1-p1: modos VESA Bochs (byte bajo; alto siempre 0x01).
; Indice CMOS 0x40: 0=800x600 (defecto y valor si CMOS virgen 0xFF),
; 1=640x480, 2=1024x768. Verificados con LFB+32bpp en QEMU.
modo_tab db 0x43, 0x42, 0x44, 0x43
; Parametros de "leer" y LBAs parcheados por el constructor (bytes
; 496-501 del sector, justo antes de la firma 0x55AA).
times 496-($-$$) db 0
pay_secs dw 0
cat_lba dw 0
dat_lba dw 0
p_seg dw 0
p_off dw 0
p_lba dw 0
p_cnt dw 0
dw 0xAA55
