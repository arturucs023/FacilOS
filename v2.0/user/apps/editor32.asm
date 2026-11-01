; editor32: editor de texto minimo (anillo 3, int 0x80).
; Uso: editor32 fichero | Escribe, Enter, Retroceso, ESC g=guardar, ESC s=salir.
; Cabe en 512: syscalls directas, registros fijos (el nucleo preserva
; todo menos eax), args ya sin espacios (la shell los recorta).

[SECTION .text]
global _start

_start:
    mov esi, 0x2F0000         ; ESI=nom (fijo todo el programa)
.e2:
    mov al, [esi]
    cmp al, 0
    je .fin_nom
    cmp al, 0x20
    je .fin_nom
    inc esi
    jmp .e2
.fin_nom:
    mov byte [esi], 0
    mov ebx, 0x2F0000
    cmp ebx, esi
    jne .e4
    mov ebx, m_uso
    mov ecx, 10
    call imp
    mov ebx, 1
    jmp salir
.e4:
    xor ebp, ebp              ; EBP=tam
    mov eax, 3                ; abrir
    mov ebx, 0x2F0000
    int 0x80
    test eax, eax
    js .nuevo
    mov edi, eax              ; EDI=fd
    mov ebx, eax
    mov ecx, buf
    mov edx, 512
    mov eax, 4                ; leer
    int 0x80
    test eax, eax
    js .cerr1
    mov ebp, eax
    mov ebx, buf
    mov ecx, eax
    call imp
.cerr1:
    mov ebx, edi
    mov eax, 6                ; cerrar
    int 0x80
.nuevo:
    mov ebx, m_ban
    mov ecx, 9
    call imp
.bucle:
    mov eax, 8                ; leer_tecla
    int 0x80
    cmp eax, 27               ; ESC
    je .esc
    cmp eax, 8
    je .borra
    cmp eax, 13
    je .enter
    cmp eax, 32
    jb .bucle
    cmp eax, 127
    jae .bucle
    cmp ebp, 511
    jae .bucle
    mov [cb], al
    mov ebx, buf
    add ebx, ebp
    mov [ebx], al
    inc ebp
    mov ebx, cb
    mov ecx, 1
    call imp
    jmp .bucle
.borra:
    test ebp, ebp
    jz .bucle
    dec ebp
    mov ebx, m_bs
    mov ecx, 3
    call imp
    jmp .bucle
.enter:
    cmp ebp, 511
    jae .bucle
    mov ebx, buf
    add ebx, ebp
    mov byte [ebx], 10
    inc ebp
    mov ebx, m_ok
    add ebx, 2                ; "\n" dentro de "ok\n"
    mov ecx, 1
    call imp
    jmp .bucle
.esc:
    mov eax, 8
    int 0x80
    cmp eax, 'g'
    je .guarda
    cmp eax, 's'
    je .fin0
    jmp .bucle
.guarda:
    mov ebx, 0x2F0000
    mov eax, 3                ; abrir
    int 0x80
    test eax, eax
    jns .gab
    mov ebx, 0x2F0000
    mov eax, 9                ; crear
    int 0x80
    test eax, eax
    js .gerr
.gab:
    mov edi, eax
    mov ebx, eax
    mov ecx, buf
    mov edx, ebp
    mov eax, 5                ; escribir
    int 0x80
    cmp eax, ebp
    jne .gerr2
    mov ebx, edi
    mov eax, 6
    int 0x80
    mov ebx, m_ok
    mov ecx, 3
    call imp
.fin0:
    xor ebx, ebx
    jmp salir
.gerr2:
    mov ebx, edi
    mov eax, 6
    int 0x80
.gerr:
    mov ebx, m_err
    mov ecx, 4
    call imp
    mov ebx, 2
    jmp salir

; imp: ebx=ptr ecx=len -> syscall 1 (corta en NUL)
imp:
    mov eax, 1
    int 0x80
    ret

salir:
    mov eax, 0
    int 0x80

m_uso: db "uso: ed f", 10
m_ban: db "ESC g s", 10, 10
m_ok:  db "ok", 10
m_err: db "err", 10
m_bs:  db 8, 32, 8

[SECTION .bss]
buf:   resb 512
cb:    resb 4
