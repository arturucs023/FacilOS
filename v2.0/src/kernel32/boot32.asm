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
    xor bp, bp
    mov [0x7001], bp
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
    ; El BIOS entrega DL = unidad de CD (no-emulacion El Torito).
    ; El nucleo (LBA logicos 1..224) y el catalogo van empaquetados
    ; en sectores de 2048 B del CD desde iso_base (sector 2).
    ; Se leen por EDD (INT 13h AH=42h) aqui, en MODO REAL: desde
    ; 32 bits el int 0x13 del BIOS peta (#UD), y el nucleo no lo
    ; necesita porque se lo copiamos ya en RAM.
    ;   0x30000 = catalogo + programas, copia baja [cat, 993)
    ;   0x1000000 = cola alta [993, fin) (carga directa, sin copyalta)
    ;   0x10000 = nucleo (se copiara a 0x100000)
    ; Sin interrupciones: los IRQ del BIOS pisan la copia (probado).
    cli
    call edd_ok
    cmp ax, 0
    jne sin_edd
    mov ax, 0x1000
    mov [p_seg], ax
    xor ax, ax
    mov [p_off], ax
    inc ax
    mov [p_lba], ax
    mov ax, 240                     ; nucleo hasta 240 sectores logicos
                                        ; (F8: debe coincidir con KERN_SECS de
                                        ; build32.py; el assert kb<=240*512 lo
                                        ; garantiza; si crece, subir ambos)
    mov [p_cnt], ax
    call leer
    mov ax, 0x3000
    mov [p_seg], ax
    mov ax, [cat_lba]
    mov [p_lba], ax
    mov ax, [pay_secs]
    ; F7-C: la copia baja se topa en el LBA 993 (=0x90000 lineal):
    ; la pila de arranque (64 KB en [0x90000,0xA0000)) y la EBDA/hueco
    ; VGA no retienen. La cola [993, fin) la pone copyalta en alto
    ; (0x1000000); aqui solo [cat, 993).
    mov bx, 993
    sub bx, [cat_lba]
    cmp ax, bx
    jbe .cnt_ok
    mov ax, bx
.cnt_ok:
    ; F8: cinturon y tirantes: aunque cat_lba fuese <225, la baja no
    ; pasa de 768 sectores (0x30000->0x90000, pila/EBDA/VGA).
    cmp ax, 768
    jbe .cnt2_ok
    mov ax, 768
.cnt2_ok:
    mov [p_cnt], ax
    call leer
    ; F8: la cola [993, fin) va directa a 0x1000000 por EDD (la vieja
    ; copyalta por CHS ya no existe: la ISO no es disquete emulado).
    call carga_alta
.sin_cola:
.ok:
    mov ax, [cat_lba]
    mov [0x7010], ax
    mov ax, [dat_lba]
    mov [0x7018], ax
    mov dl, [unidad]
    mov [0x7000], dl        ; unidad de arranque
    cli
    lgdt [cs:gdt_ptr]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x08:pm_entry


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
; F7-C: cola alta del catalogo (tras la pila de arranque 0x90000).
; CORTE_LBA = primer LBA copiado alto (993 = 225 + 768;
; 768 = (0x90000 - 0x30000) / 512).
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
; ---- sector 2 (F8: arranque EDD, sin limite de 1,44 MB) ----
; Bloque de datos PRIMERO (offsets fijos): build32 localiza 'BFSX'.
iso_magic dd 0x58534642    ; 'BFSX' + iso_base justo detras
iso_base dd 22             ; LBA CD (2048 B) del logico 0 (parcheado)
alta_lin dd 0x1000000
cach_lo dd 0xFFFFFFFF
cach_hi dd 0xFFFFFFFF
edd_pkt db 16, 0
edd_cnt dw 0
edd_off dw 0
edd_seg dw 0
edd_lba dd 0, 0, 0, 0
; GDT propia para el irreal de leer_alta (dato 32 bit 4 GB).
gdt_u_ptr:
    dw gdt_u_fin - gdt_u - 1
    dd gdt_u
gdt_u:
    dd 0, 0
    dd 0x0000FFFF, 0x00CF9A00   ; codigo 32 bit 4 GB (igual que copyalta)
    dd 0x0000FFFF, 0x00CF9300   ; dato 32 bit 4 GB (igual que copyalta)
gdt_u_fin:


; edd_ok: F8: el chequeo AH=41h se cuelga con DL=0xE0 en este
; SeaBIOS (probado); se asume EDD (la lectura AH=42h lo demostrara
; o fallara con '4' por serie). Devuelve 0.
edd_ok:
    xor ax, ax
    ret

; carga_alta: si fin>993, copia [993, fin) a 0x1000000 por EDD.
carga_alta:
    mov ax, [cs:cat_lba]
    add ax, [cs:pay_secs]          ; ax = fin (ultimo LBA + 1)
    cmp ax, 993
    jbe .no
    sub ax, 993
    mov [cs:p_cnt], ax
    mov word [cs:p_lba], 993
    mov dword [cs:alta_lin], 0x1000000
    call leer_alta
.no:
    ret

; leer: copia p_cnt sectores logicos (512 B) desde p_lba a p_seg:p_off
; (<1 MB) por EDD con rebote 0x8000 y cache del ultimo sector CD.
; edd_trae: DI=lba logico -> SI=origen en rebote (0x8000+((DI&3)*512)).
; Preserva DI/BX/ES; deja DS=0.
leer:
    xor ax, ax
    mov ds, ax
    mov ax, [p_seg]
    mov es, ax
    mov bx, [p_off]
    mov di, [p_lba]
leer_bucle:
    call edd_trae
    push di
    mov ax, bx              ; AX = destino viejo (para el acarreo 64K)
    mov di, bx              ; DI = destino
    mov cx, 256
    rep movsw               ; DS:SI -> ES:DI (DS=0, SI=rebote)
    mov bx, di
    cmp di, ax
    jae leer_sin
    mov ax, es
    add ax, 0x1000
    mov es, ax
leer_sin:
    pop di
    inc di
    dec word [p_cnt]
    jnz leer_bucle
    ret

; leer_alta: igual pero a [alta_lin] lineal (>=0x1000000) en irreal.
; F8: DI es los 16 bits bajos de EDI: NO se puede usar DI como
; contador y EDI como destino a la vez (el inc di corrompia ambos).
; El LBA vive en [p_lba] y se recarga cada vuelta.
leer_alta:
    xor ax, ax
    mov ds, ax
    lgdt [cs:gdt_u_ptr]         ; GDT 4 GB propia (probada en copyalta)
alta_bucle:
    mov di, [cs:p_lba]
    call edd_trae            ; SI = 0x8000+off, DS=0
    mov eax, cr0
    or al, 1
    mov cr0, eax
    mov bx, 0x10
    mov ds, bx
    mov es, bx
    and al, 0xFE
    mov cr0, eax
    movzx esi, si
    mov edi, [cs:alta_lin]
    mov ecx, 128
    a32 rep movsd
    xor bx, bx
    mov ds, bx
    mov es, bx
    mov eax, [cs:alta_lin]
    add eax, 512
    mov [cs:alta_lin], eax
    inc word [cs:p_lba]
    dec word [p_cnt]
    jnz alta_bucle
    ret

; edd_trae: trae el sector CD que contiene el logico DI.
; Entrada DI=lba (se preserva). Salida SI=0x8000+((DI&3)*512), DS=0.
; Error EDD: '4' por serie y halt.
edd_trae:
    push ax
    push dx
    push bx
    mov ax, di
    shr ax, 2                ; AX = DI>>2 (relativo, 0..16383)
    xor dx, dx
    add ax, [cs:iso_base]
    adc dx, [cs:iso_base+2]      ; DX:AX = sector CD absoluto
    cmp ax, [cs:cach_lo]
    jne .nuevo
    cmp dx, [cs:cach_hi]
    je .hit
.nuevo:
    mov [cs:cach_lo], ax
    mov [cs:cach_hi], dx
    mov word [cs:edd_cnt], 1
    mov word [cs:edd_off], 0x8000
    mov word [cs:edd_seg], 0
    mov [cs:edd_lba], ax
    mov [cs:edd_lba+2], dx
    mov word [cs:edd_lba+4], 0
    mov word [cs:edd_lba+6], 0
    mov dl, [cs:unidad]
    xor ax, ax
    mov ds, ax
    mov si, edd_pkt
    sti                        ; F8: SeaBIOS/ATAPI necesita IRQ (ver log
    mov ah, 0x42               ; EDD); sin esto el AH=42h no vuelve.
    int 0x13
    cli
    jnc .hit
    mov dx, 0x3F8            ; error EDD: '4' (la ISO nueva exige EDD)
    mov al, '4'
    out dx, al
    jmp inicio_error

.hit:
    xor ax, ax
    mov ds, ax
    mov ax, di
    and ax, 3
    shl ax, 9                ; AX = (DI&3)*512
    mov si, 0x8000
    add si, ax
    pop bx
    pop dx
    pop ax
    ret

sin_edd:
    mov dx, 0x3F8            ; sin extensiones EDD: '4' y halt
    mov al, '4'
    out dx, al
    jmp inicio_error
times 1024-($-$$) db 0
