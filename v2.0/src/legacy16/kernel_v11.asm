; FacilOS v1.0 - Kernel + Shell en espanol facil
; 100% desde cero, modo real 16-bit, solo BIOS. Sin Linux ni Windows.
; ORG 0x7E00 (cargado por boot.asm)

[BITS 16]
[ORG 0x7E00]

jmp principal

; ================= DATOS GLOBALES =================
unidad_boot db 0
dir_actual dw 0

buffer_entrada times 128 db 0
buffer_comando times 32 db 0
buffer_args times 96 db 0
buffer_nombre times 32 db 0
buffer_nombre2 times 32 db 0
buffer_texto times 512 db 0
buffer_ruta times 128 db 0
pila_ruta times 64 dw 0

msg_banner db 13,10
    db '========================================',13,10
    db '  FacilOS v1.1 - Sistema facil en espanol',13,10
    db '  Hecho desde cero (sin Linux ni Windows)',13,10
    db '========================================',13,10
    db 'Escribe ayuda para ver los comandos.',13,10
    db 13,10,0

msg_prompt_ini db 'facil:',0
msg_prompt_fin db '> ',0
msg_barra db '/',0
msg_crlf db 13,10,0
msg_espacio db ' ',0

; ---- textos de comandos ----
msg_ayuda_corta db 'Comandos faciles (usa _ ):',13,10
    db '  ayuda [comando|--todo] ........ muestra ayuda',13,10
    db '  ver_archivos [--todo|--carpetas|--archivos] . lista',13,10
    db '  entrar_carpeta <nombre|..|/> .. entra o sale',13,10
    db '  crear_carpeta <nombre> ........ crea carpeta',13,10
    db '  crear_archivo <nombre> ........ crea archivo vacio',13,10
    db '  escribir_archivo <nombre> <texto> [--agregar]',13,10
    db '  leer_archivo <nombre> ......... lee archivo',13,10
    db '  copiar_archivo <origen> <destino>',13,10
    db '  mover_archivo <origen> <destino>',13,10
    db '  renombrar_archivo <antiguo> <nuevo>',13,10
    db '  buscar_texto <archivo> <texto>',13,10
    db '  editar_archivo <nombre> ....... edita texto',13,10
    db '  borrar_archivo <nombre> ....... borra archivo',13,10
    db '  borrar_carpeta <nombre> ....... borra carpeta',13,10
    db '  donde_estoy ................... muestra carpeta actual',13,10
    db '  limpiar ....................... limpia pantalla',13,10
    db '  eco <texto> ................... repite texto',13,10
    db '  fecha [--hora|--fecha] ........ muestra fecha/hora',13,10
    db '  quien_soy ..................... muestra usuario',13,10
    db '  info_sistema [--todo] ......... datos del sistema',13,10
    db '  espacio_disco ................. espacio libre',13,10
    db '  guardar ....................... guarda en disco',13,10
    db '  historial / limpiar_historial . comandos anteriores',13,10
    db '  version ....................... version',13,10
    db '  reiniciar ..................... reinicia',13,10
    db '  apagar ........................ apaga (halt)',13,10
    db 13,10,0

msg_ayuda_todo db '--- Ayuda detallada ---',13,10
    db 'ayuda [comando] : explica un comando.',13,10
    db '  Ej: ayuda crear_carpeta',13,10
    db 'ver_archivos : lista lo que hay aqui.',13,10
    db '  --todo muestra tipos y tamanos.',13,10
    db '  --carpetas solo carpetas, --archivos solo archivos.',13,10
    db 'entrar_carpeta nombre : entra. .. sale, / va a raiz.',13,10
    db 'crear_carpeta nombre : crea carpeta (max 31 letras).',13,10
    db 'crear_archivo nombre : crea archivo vacio.',13,10
    db 'escribir_archivo nombre texto : escribe (borra antes).',13,10
    db '  --agregar agrega al final. Ej: escribir_archivo nota hola',13,10
    db 'leer_archivo nombre : muestra contenido.',13,10
    db 'copiar_archivo origen destino : copia archivo.',13,10
    db 'mover_archivo origen destino : mueve archivo.',13,10
    db 'renombrar_archivo antiguo nuevo : renombra.',13,10
    db 'buscar_texto archivo texto : busca texto.',13,10
    db 'editar_archivo nombre : editor (:guardar/:salir).',13,10
    db 'borrar_archivo nombre : borra archivo.',13,10
    db 'borrar_carpeta nombre : borra carpeta vacia.',13,10
    db '  --forzar borra con todo lo de dentro.',13,10
    db 'donde_estoy : muestra ruta actual.',13,10
    db 'limpiar : limpia la pantalla.',13,10
    db 'eco texto : repite. Ej: eco hola mundo',13,10
    db 'fecha : muestra fecha y hora. --hora solo hora.',13,10
    db 'quien_soy : muestra usuario.',13,10
    db 'info_sistema : datos. --todo mas datos.',13,10
    db 'espacio_disco : ocupados, libres y bytes.',13,10
    db 'guardar : guarda en disco. Historial con flechas.',13,10
    db 'historial / limpiar_historial : ver/borrar historial.',13,10
    db 'version : version de FacilOS.',13,10
    db 'reiniciar : reinicia el equipo.',13,10
    db 'apagar : detiene (luego cierra VirtualBox).',13,10
    db 13,10,0

msg_desc_ayuda db 'ayuda [comando|--todo] : muestra ayuda. Ej: ayuda ver_archivos',13,10,0
msg_desc_ver db 'ver_archivos [--todo|--carpetas|--archivos] : lista carpeta actual.',13,10,0
msg_desc_entrar db 'entrar_carpeta <nombre|..|/> : cambia de carpeta.',13,10,0
msg_desc_crearc db 'crear_carpeta <nombre> : crea carpeta. Ej: crear_carpeta fotos',13,10,0
msg_desc_creara db 'crear_archivo <nombre> : crea archivo vacio. Ej: crear_archivo nota',13,10,0
msg_desc_escribir db 'escribir_archivo <nombre> <texto> [--agregar] : escribe texto.',13,10
    db '  Ej: escribir_archivo nota hola mundo',13,10
    db '  Ej: escribir_archivo nota --agregar mas texto',13,10,0
msg_desc_leer db 'leer_archivo <nombre> : muestra contenido del archivo.',13,10,0
msg_desc_borrara db 'borrar_archivo <nombre> : borra un archivo.',13,10,0
msg_desc_borrarc db 'borrar_carpeta <nombre> [--forzar] : borra carpeta.',13,10,0
msg_desc_donde db 'donde_estoy : muestra en que carpeta estas.',13,10,0
msg_desc_limpiar db 'limpiar : limpia toda la pantalla. (limpiar_pantalla igual)',13,10,0
msg_desc_copiar db 'copiar_archivo <origen> <destino> : copia. Si destino es carpeta, copia dentro.',13,10,0
msg_desc_mover db 'mover_archivo <origen> <destino> : mueve. Si destino es carpeta, mueve dentro.',13,10,0
msg_desc_renombrar db 'renombrar_archivo <antiguo> <nuevo> : cambia el nombre.',13,10,0
msg_desc_buscar db 'buscar_texto <archivo> <texto> : busca texto dentro del archivo.',13,10,0
msg_desc_editar db 'editar_archivo <nombre> : edita. :guardar guarda, :salir cancela.',13,10,0
msg_desc_espacio db 'espacio_disco : muestra ocupados, libres y bytes.',13,10,0
msg_desc_guardar db 'guardar : guarda todo en el disco de datos.',13,10,0
msg_desc_historial db 'historial : muestra comandos anteriores (flecha arriba/abajo).',13,10,0
msg_desc_limphist db 'limpiar_historial : borra el historial.',13,10,0
msg_desc_eco db 'eco <texto> : repite el texto. Ej: eco hola',13,10,0
msg_desc_fecha db 'fecha [--hora|--fecha] : muestra fecha y hora.',13,10,0
msg_desc_quien db 'quien_soy : dice quien eres (invitado).',13,10,0
msg_desc_info db 'info_sistema [--todo] : muestra datos del sistema.',13,10,0
msg_desc_version db 'version : muestra la version.',13,10,0
msg_desc_reiniciar db 'reiniciar : reinicia el equipo.',13,10,0
msg_desc_apagar db 'apagar : detiene el sistema.',13,10,0

msg_no_entendido db 'No entiendo ese comando. Escribe ayuda.',13,10,0
msg_falta_nombre db 'Falta el nombre. Ej: crear_carpeta fotos',13,10,0
msg_falta_texto db 'Falta el texto. Ej: escribir_archivo nota hola',13,10,0
msg_ya_existe db 'Ya existe con ese nombre.',13,10,0
msg_no_existe db 'No existe con ese nombre.',13,10,0
msg_no_es_carpeta db 'No es una carpeta.',13,10,0
msg_no_es_archivo db 'No es un archivo.',13,10,0
msg_sin_espacio db 'No hay espacio (max 64 cosas).',13,10,0
msg_nombre_largo db 'Nombre muy largo (max 31 letras).',13,10,0
msg_nombre_malo db 'Nombre no valido. Usa letras, numeros, _ - .',13,10,0
msg_texto_largo db 'Texto muy largo (max 512 letras).',13,10,0
msg_carpeta_no_vacia db 'Carpeta no vacia. Usa --forzar para borrar todo.',13,10,0
msg_creado_carpeta db 'Carpeta creada.',13,10,0
msg_creado_archivo db 'Archivo creado.',13,10,0
msg_escrito db 'Guardado.',13,10,0
msg_borrado db 'Borrado.',13,10,0
msg_vacia db '(vacio)',13,10,0
msg_et_carpeta db '  <carpeta> ',0
msg_et_archivo db '  <archivo> ',0
msg_quien db 'Eres invitado, usuario de FacilOS.',13,10,0
msg_version db 'FacilOS version 1.1 - Shell facil en espanol.',13,10,0
msg_info db 'FacilOS v1.1 - Hecho desde cero en ensamblador.',13,10,0
msg_info_todo db 'FacilOS v1.1',13,10
    db 'Modo: 16-bit real (BIOS)',13,10
    db 'Shell: facil, comandos con _',13,10
    db 'Archivos: 64 cosas, 512 letras por archivo, disco persistente',13,10,0
msg_copiado db 'Archivo copiado.',13,10,0
msg_movido db 'Archivo movido.',13,10,0
msg_renombrado db 'Archivo renombrado.',13,10,0
msg_encontrado db 'Texto encontrado en la posicion ',0
msg_no_encontrado db 'Texto no encontrado.',13,10,0
msg_edit_ini db 'Editando (escribe :guardar para guardar, :salir sin guardar).',13,10,0
msg_edit_prompt db '> ',0
msg_edit_guardado db 'Archivo guardado.',13,10,0
msg_edit_salir db 'Edicion cancelada.',13,10,0
msg_edit_vacio db '(archivo vacio)',13,10,0
cmd_guardar_dos db ':guardar',0
cmd_salir_dos db ':salir',0
msg_mem db 'Memoria base (KB): ',0
msg_apagar db 13,10,'Sistema detenido. Ya puedes cerrar VirtualBox.',13,10,0
msg_reiniciar db 13,10,'Reiniciando...',13,10,0
msg_uso_ayuda db 'Uso: ayuda [comando|--todo]',13,10,0

; nombres de comandos para comparar
cmd_ayuda db 'ayuda',0
cmd_ver db 'ver_archivos',0
cmd_entrar db 'entrar_carpeta',0
cmd_crearc db 'crear_carpeta',0
cmd_creara db 'crear_archivo',0
cmd_escribir db 'escribir_archivo',0
cmd_leer db 'leer_archivo',0
cmd_borrara db 'borrar_archivo',0
cmd_borrarc db 'borrar_carpeta',0
cmd_donde db 'donde_estoy',0
cmd_limpiar db 'limpiar_pantalla',0
cmd_limpiar2 db 'limpiar',0
cmd_eco db 'eco',0
cmd_fecha db 'fecha',0
cmd_quien db 'quien_soy',0
cmd_info db 'info_sistema',0
cmd_version db 'version',0
cmd_reiniciar db 'reiniciar',0
cmd_apagar db 'apagar',0
cmd_historial db 'historial',0
cmd_limphist db 'limpiar_historial',0
cmd_copiar db 'copiar_archivo',0
cmd_mover db 'mover_archivo',0
cmd_renombrar db 'renombrar_archivo',0
cmd_buscar db 'buscar_texto',0
cmd_editar db 'editar_archivo',0
cmd_espacio db 'espacio_disco',0
cmd_guardar db 'guardar',0
msg_sin_hist db '(sin historial)',13,10,0
msg_hist_ok db 'Historial borrado.',13,10,0
msg_punto db '. ',0

opt_todo db '--todo',0
opt_carpetas db '--carpetas',0
opt_archivos db '--archivos',0
opt_ayuda db '--ayuda',0
opt_agregar db '--agregar',0
opt_forzar db '--forzar',0
opt_hora db '--hora',0
opt_fecha db '--fecha',0
opt_raiz db '/',0
opt_padre db '..',0

; ================= CODIGO =================
principal:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [unidad_boot], dl

    mov ax, 0x0003
    int 0x10

    call init_serial
    call fs_inicializar

    mov si, msg_banner
    call imprimir
    call cargar_disco

bucle_shell:
    call mostrar_prompt
    call leer_linea
    call procesar_linea
    jmp bucle_shell

; ---------- serial COM1 para depurar (QEMU -serial) ----------
init_serial:
    pusha
    push dx
    mov dx, 0x3F9
    mov al, 0x00
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9
    mov al, 0x00
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    pop dx
    popa
    ret

serial_putc:
    pusha
    push dx
    push cx
    push ax
    mov ah, al
    mov cx, 0
.ser_espera:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jnz .ser_listo
    inc cx
    cmp cx, 5000
    jb .ser_espera
    jmp .ser_fin       ; sin puerto serie, salir sin bloquear
.ser_listo:
    mov dx, 0x3F8
    mov al, ah
    out dx, al
.ser_fin:
    pop ax
    pop cx
    pop dx
    popa
    ret

; ---------- imprimir cadena SI ----------
imprimir:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
.imp_rep:
    lodsb
    or al, al
    jz .imp_fin
    int 0x10
    call serial_putc
    jmp .imp_rep
.imp_fin:
    popa
    ret

imprimir_char:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    call serial_putc
    popa
    ret

; ---------- colores v1.1 (INT 10h, modo texto 03) ----------
; Colores: 0x0A verde ok, 0x0C rojo error, 0x0E amarillo aviso,
;          0x09 azul dirs, 0x0F blanco archivos, 0x07 gris normal
color_actual db 0x07
; imprimir cadena SI con color en BL (preserva BL? no, BL=color entrada)
imprimir_color:
    pusha
    push bx
    mov [cs:color_actual], bl
    mov bh, bl          ; BH = color para AH=09
.pop_rep:
    lodsb
    or al, al
    jz .pop_fin
    cmp al, 13
    je .pop_cr
    cmp al, 10
    je .pop_lf
    ; char imprimible: AH=09 escribe con color sin mover cursor
    pusha
    mov bl, [cs:color_actual]
    mov bh, 0x00
    mov cx, 0x0001
    mov ah, 0x09
    int 0x10
    popa
    ; avanzar cursor manualmente
    pusha
    mov ah, 0x03
    mov bh, 0x00
    int 0x10            ; DH=fila DL=col
    inc dl
    cmp dl, 80
    jb .pop_set
    mov dl, 0
    inc dh
    cmp dh, 25
    jb .pop_set
    ; scroll 1 linea
    mov ah, 0x06
    mov al, 0x01
    mov bh, 0x07
    mov cx, 0x0000
    mov dx, 0x184F
    int 0x10
    mov dh, 24
    mov dl, 0
.pop_set:
    mov ah, 0x02
    mov bh, 0x00
    int 0x10
    popa
    push bx
    mov bh, [cs:color_actual]
    pop bx
    call serial_putc
    jmp .pop_rep
.pop_cr:
    pusha
    mov ah, 0x03
    mov bh, 0x00
    int 0x10
    mov dl, 0
    mov ah, 0x02
    int 0x10
    popa
    call serial_putc
    jmp .pop_rep
.pop_lf:
    pusha
    mov ah, 0x03
    mov bh, 0x00
    int 0x10
    inc dh
    cmp dh, 25
    jb .pop_lf_set
    mov ah, 0x06
    mov al, 0x01
    mov bh, 0x07
    mov cx, 0x0000
    mov dx, 0x184F
    int 0x10
    mov dh, 24
.pop_lf_set:
    mov ah, 0x02
    mov bh, 0x00
    int 0x10
    popa
    call serial_putc
    jmp .pop_rep
.pop_fin:
    pop bx
    popa
    ret

; atajos de color
imprimir_ok:
    pusha
    mov bl, 0x0A
    call imprimir_color
    popa
    ret
imprimir_error:
    pusha
    mov bl, 0x0C
    call imprimir_color
    popa
    ret
imprimir_aviso:
    pusha
    mov bl, 0x0E
    call imprimir_color
    popa
    ret
imprimir_azul:
    pusha
    mov bl, 0x09
    call imprimir_color
    popa
    ret
imprimir_blanco:
    pusha
    mov bl, 0x0F
    call imprimir_color
    popa
    ret

nueva_linea:
    pusha
    mov si, msg_crlf
    call imprimir
    popa
    ret

; ---------- longitud SI -> CX ----------
longitud:
    push si
    push ax
    xor cx, cx
.lrep:
    lodsb
    or al, al
    jz .lfin
    inc cx
    jmp .lrep
.lfin:
    pop ax
    pop si
    ret

; ---------- comparar SI,DI -> AX 0=igual ----------
cadenas_iguales:
    pusha
.crep:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .dist
    or al, al
    jz .ig
    inc si
    inc di
    jmp .crep
.ig:
    mov word [cs:.res], 0
    jmp .cfin
.dist:
    mov word [cs:.res], 1
.cfin:
    popa
    mov ax, [cs:.res]
    ret
.res dw 0

; ---------- copiar SI -> DI (DS:SI -> DS:DI, ES=DS) ----------
copiar_cadena:
    pusha
    push es
    push ds
    pop es         ; ES = DS (0)
.coprep:
    lodsb
    stosb
    or al, al
    jnz .coprep
    pop es
    popa
    ret

; ---------- saltar espacios: SI avanza ----------
saltar_espacios:
    push ax
.srep:
    mov al, [si]
    cmp al, ' '
    jne .sfin
    inc si
    jmp .srep
.sfin:
    pop ax
    ret

; ---------- mostrar prompt ----------
mostrar_prompt:
    pusha
    mov si, msg_prompt_ini
    call imprimir
    call construir_ruta
    mov si, buffer_ruta
    call imprimir_azul
    mov si, msg_prompt_fin
    call imprimir
    popa
    ret

; La version simple anterior tenia un bug de pila. Usamos construir_ruta siempre:
; construir_ruta llena buffer_ruta con /nombre/nombre
construir_ruta:
    pusha
    push es
    ; vaciar buffer
    mov di, buffer_ruta
    mov byte [di], 0
    mov ax, [dir_actual]
    cmp ax, 0
    je .esraiz
    ; armar pila invertida
    xor cx, cx
    mov bx, ax
.ciclo:
    mov di, pila_ruta
    mov ax, cx
    shl ax, 1
    add di, ax
    mov [di], bx
    inc cx
    cmp cx, 64
    jae .volcar
    mov ax, bx
    call fs_leer_padre
    mov bx, ax
    cmp bx, 0
    je .volcar
    ; comprobar que no sea ciclo raiz (padre de raiz = 0)
    ; si bx==0 terminamos, si no seguimos pero si el padre es el mismo? raiz padre=0
    ; continuar con bx como siguiente (si bx es 0 salimos arriba)
    ; Pero necesitamos incluir hasta llegar a raiz: si padre==0, ya tenemos toda la cadena
    ; Ver: si bx actual es hijo directo de raiz, su padre es 0, ya guardamos el hijo, listo.
    ; Si es mas profundo, bx ahora es intermedio, hay que guardarlo tambien en prox iteracion.
    ; El codigo actual guarda bx antes de leer padre, pero despues pone bx=padre y si padre!=0 repite.
    ; Para el intermedio, en la siguiente iteracion lo guardara. Bien.
    ; Pero hay un error: despues de mov bx,ax, bx es padre. Si padre !=0, loop guarda ese padre. Correcto.
    jmp .ciclo
.volcar:
    ; volcar de atras hacia adelante: cx = cantidad
    mov di, buffer_ruta
.volcar_rep:
    dec cx
    js .finr
    push di
    push cx
    mov ax, cx
    shl ax, 1
    mov bx, pila_ruta
    add bx, ax
    mov ax, [bx]     ; indice
    pop cx
    pop di
    ; poner '/'
    mov byte [di], '/'
    inc di
    push cx
    push di
    mov bx, di
    ; copiar nombre de AX a buffer temporal y luego a ruta
    call fs_copiar_nombre ; AX indice -> escribe en DI (destino DS)
    pop di
    pop cx
    ; avanzar di hasta 0
.avdi:
    mov al, [di]
    or al, al
    jz .sig
    inc di
    jmp .avdi
.sig:
    jmp .volcar_rep
.finr:
    mov byte [di], 0
    pop es
    popa
    ret
.esraiz:
    mov di, buffer_ruta
    mov byte [di], '/'
    mov byte [di+1], 0
    pop es
    popa
    ret

; ---------- historial v1.1 (20 x 128 en 0x1A00:0) ----------
HIST_MAX equ 20
HIST_LEN equ 128
hist_count db 0
hist_pos db 0
en_editor db 0
; guardar SI (linea 0-term) en historial (si no vacia y no repetida)
hist_guardar:
    pusha
    cmp byte [cs:en_editor], 1
    je .hg_fin
    push es
    push ds
    pop es
    mov di, si
    call longitud
    cmp cx, 0
    je .hg_out
    ; comparar con ultima?
    mov al, [cs:hist_count]
    or al, al
    jz .hg_add
    dec al
    push si
    mov ah, 0
    mov bx, HIST_LEN
    mul bx
    mov bx, 0x1A00
    mov es, bx
    mov di, ax
    call comparar_es_hist
    pop si
    cmp ax, 0
    je .hg_out
.hg_add:
    mov al, [cs:hist_count]
    cmp al, HIST_MAX
    jb .hg_slot
    ; lleno: desplazar 1..19 -> 0..18
    push ds
    mov ax, 0x1A00
    mov ds, ax
    mov es, ax
    mov si, HIST_LEN
    mov di, 0
    mov cx, (HIST_MAX-1)*HIST_LEN
    rep movsb
    pop ds
    mov al, HIST_MAX-1
    mov [cs:hist_count], al
.hg_slot:
    mov ah, 0
    mov al, [cs:hist_count]
    mov bx, HIST_LEN
    mul bx
    mov di, ax
    mov ax, 0x1A00
    mov es, ax
    push es
    push ds
    pop es
    ; copiar DS:SI -> ES:DI (ES=0x1A00)
    pop es
    push es
    mov bx, es
    mov ax, 0x1A00
    mov es, ax
.cop_h:
    lodsb
    mov [es:di], al
    inc di
    or al, al
    jnz .cop_h
    pop es
    inc byte [cs:hist_count]
.hg_out:
    mov al, [cs:hist_count]
    mov [cs:hist_pos], al
    pop es
.hg_fin:
    popa
    ret
; comparar DS:SI con ES:DI (hist) -> AX 0 igual (helper para hist_guardar)
comparar_es_hist:
    push si
    push di
    push bx
.ch2:
    mov al, [si]
    mov bl, [es:di]
    cmp al, bl
    jne .dh2
    or al, al
    jz .ih2
    inc si
    inc di
    jmp .ch2
.ih2:
    mov ax, 0
    jmp .cfh2
.dh2:
    mov ax, 1
.cfh2:
    pop bx
    pop di
    pop si
    ret
; cargar historial AL=idx a DS:DI (0-term). Preserva todo menos nada (usa pusha)
hist_cargar:
    pusha
    mov ah, 0
    mov bx, HIST_LEN
    mul bx
    mov si, ax
    mov ax, 0x1A00
    mov es, ax
.hc_rep:
    mov al, [es:si]
    mov [di], al
    inc si
    inc di
    or al, al
    jnz .hc_rep
    popa
    ret
; borrar linea actual en pantalla (CX=len, DI=fin). Deja CX=0 DI=inicio. Preserva BX,DX,SI
borrar_linea_pantalla:
    push ax
    push cx
.bl_rep:
    cmp cx, 0
    je .bl_fin
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
    dec cx
    jmp .bl_rep
.bl_fin:
    pop cx
    pop ax
    ret

; ---------- leer linea a buffer_entrada (con historial flechas) ----------
leer_linea:
    pusha
    mov di, buffer_entrada
    xor cx, cx          ; longitud
.lrep:
    mov ah, 0x00
    int 0x16            ; AL = tecla, AH = scan
    cmp al, 13          ; Enter
    je .enter
    cmp al, 8           ; Backspace
    je .back
    cmp al, 0           ; tecla extendida (flechas)
    je .extendida
    cmp cx, 120
    jae .lrep
    ; eco (video + serial para depurar)
    push ax
    mov ah, 0x0E
    mov bh, 0
    mov bl, 7
    int 0x10
    pop ax
    push ax
    call serial_putc
    pop ax
    mov [di], al
    inc di
    inc cx
    jmp .lrep
.extendida:
    cmp byte [cs:en_editor], 1
    je .lrep            ; en editor, ignorar flechas
    cmp ah, 0x48        ; arriba
    je .hist_arriba
    cmp ah, 0x50        ; abajo
    je .hist_abajo
    jmp .lrep
.hist_arriba:
    mov al, [cs:hist_pos]
    or al, al
    jz .lrep
    dec byte [cs:hist_pos]
    jmp .lrep_recargar
.hist_abajo:
    mov al, [cs:hist_pos]
    mov bl, [cs:hist_count]
    cmp al, bl
    jae .lrep
    inc byte [cs:hist_pos]
    jmp .lrep_recargar
.lrep_recargar:
    ; borrar linea actual de pantalla y recargar desde historial (o vaciar)
    ; CX=len actual, DI=fin. Inicio = DI-CX
    push ax
    push bx
    push si
    mov bx, di
    sub bx, cx          ; BX = inicio (buffer_entrada)
    push bx
    push cx
    mov di, bx
    ; borrar en pantalla CX chars
    call borrar_linea_pantalla
    pop cx
    pop bx              ; BX=inicio
    mov di, bx
    xor cx, cx
    mov al, [cs:hist_pos]
    mov bl, [cs:hist_count]
    cmp al, bl
    jae .rec_fin        ; al final -> linea vacia
    ; cargar hist[hist_pos] a buffer_entrada
    push di
    mov ah, 0
    mov di, ax
    mov al, [cs:hist_pos]
    mov ah, 0
    mov bx, HIST_LEN
    mul bx
    mov si, ax
    mov ax, 0x1A00
    mov es, ax
    pop di
.rec_cop:
    mov al, [es:si]
    mov [di], al
    or al, al
    jz .rec_eco
    ; eco char
    push ax
    mov ah, 0x0E
    mov bh, 0
    mov bl, 7
    int 0x10
    pop ax
    push ax
    call serial_putc
    pop ax
    inc si
    inc di
    inc cx
    jmp .rec_cop
.rec_eco:
    ; CX=len, DI apunta al 0 final (el 0 se copio sin inc). Listo para Enter o seguir escribiendo.
.rec_fin:
    pop si
    pop bx
    pop ax
    jmp .lrep
.back:
    cmp cx, 0
    je .lrep
    dec di
    dec cx
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
    jmp .lrep
.enter:
    mov byte [di], 0
    push si
    mov si, buffer_entrada
    call hist_guardar
    pop si
    mov si, msg_crlf
    call imprimir
    popa
    ret

; ---------- procesar linea ----------
procesar_linea:
    pusha
    ; dividir buffer_entrada en comando + args
    mov si, buffer_entrada
    call saltar_espacios
    cmp byte [si], 0
    je .finp2           ; linea vacia
    mov di, buffer_comando
    call copiar_palabra ; copia hasta espacio
    mov si, buffer_entrada
    call saltar_espacios
    ; avanzar si hasta fin de primera palabra
.av1:
    mov al, [si]
    cmp al, 0
    je .sinargs
    cmp al, ' '
    je .hayesp
    inc si
    jmp .av1
.hayesp:
    call saltar_espacios
.sinargs:
    mov di, buffer_args
    call copiar_cadena  ; copia resto (puede ser vacio)

    ; comparar comando
    mov si, buffer_comando
    mov di, cmd_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .c_ayuda
    mov si, buffer_comando
    mov di, cmd_ver
    call cadenas_iguales
    cmp ax, 0
    je .c_ver
    mov si, buffer_comando
    mov di, cmd_entrar
    call cadenas_iguales
    cmp ax, 0
    je .c_entrar
    mov si, buffer_comando
    mov di, cmd_crearc
    call cadenas_iguales
    cmp ax, 0
    je .c_crearc
    mov si, buffer_comando
    mov di, cmd_creara
    call cadenas_iguales
    cmp ax, 0
    je .c_creara
    mov si, buffer_comando
    mov di, cmd_escribir
    call cadenas_iguales
    cmp ax, 0
    je .c_escribir
    mov si, buffer_comando
    mov di, cmd_leer
    call cadenas_iguales
    cmp ax, 0
    je .c_leer
    mov si, buffer_comando
    mov di, cmd_borrara
    call cadenas_iguales
    cmp ax, 0
    je .c_borrara
    mov si, buffer_comando
    mov di, cmd_borrarc
    call cadenas_iguales
    cmp ax, 0
    je .c_borrarc
    mov si, buffer_comando
    mov di, cmd_donde
    call cadenas_iguales
    cmp ax, 0
    je .c_donde
    mov si, buffer_comando
    mov di, cmd_limpiar
    call cadenas_iguales
    cmp ax, 0
    je .c_limpiar
    mov si, buffer_comando
    mov di, cmd_limpiar2
    call cadenas_iguales
    cmp ax, 0
    je .c_limpiar
    mov si, buffer_comando
    mov di, cmd_eco
    call cadenas_iguales
    cmp ax, 0
    je .c_eco
    mov si, buffer_comando
    mov di, cmd_fecha
    call cadenas_iguales
    cmp ax, 0
    je .c_fecha
    mov si, buffer_comando
    mov di, cmd_quien
    call cadenas_iguales
    cmp ax, 0
    je .c_quien
    mov si, buffer_comando
    mov di, cmd_info
    call cadenas_iguales
    cmp ax, 0
    je .c_info
    mov si, buffer_comando
    mov di, cmd_version
    call cadenas_iguales
    cmp ax, 0
    je .c_version
    mov si, buffer_comando
    mov di, cmd_reiniciar
    call cadenas_iguales
    cmp ax, 0
    je .c_reiniciar
    mov si, buffer_comando
    mov di, cmd_apagar
    call cadenas_iguales
    cmp ax, 0
    je .c_apagar
    mov si, buffer_comando
    mov di, cmd_historial
    call cadenas_iguales
    cmp ax, 0
    je .c_historial
    mov si, buffer_comando
    mov di, cmd_limphist
    call cadenas_iguales
    cmp ax, 0
    je .c_limphist
    mov si, buffer_comando
    mov di, cmd_copiar
    call cadenas_iguales
    cmp ax, 0
    je .c_copiar
    mov si, buffer_comando
    mov di, cmd_mover
    call cadenas_iguales
    cmp ax, 0
    je .c_mover
    mov si, buffer_comando
    mov di, cmd_renombrar
    call cadenas_iguales
    cmp ax, 0
    je .c_renombrar
    mov si, buffer_comando
    mov di, cmd_buscar
    call cadenas_iguales
    cmp ax, 0
    je .c_buscar
    mov si, buffer_comando
    mov di, cmd_editar
    call cadenas_iguales
    cmp ax, 0
    je .c_editar
    mov si, buffer_comando
    mov di, cmd_espacio
    call cadenas_iguales
    cmp ax, 0
    je .c_espacio
    mov si, buffer_comando
    mov di, cmd_guardar
    call cadenas_iguales
    cmp ax, 0
    je .c_guardar

    mov si, msg_no_entendido
    call imprimir_error
    jmp .finp2

.c_ayuda: call h_ayuda
    jmp .finp2
.c_ver: call h_ver
    jmp .finp2
.c_entrar: call h_entrar
    jmp .finp2
.c_crearc: call h_crear_carpeta
    jmp .finp2
.c_creara: call h_crear_archivo
    jmp .finp2
.c_escribir: call h_escribir
    jmp .finp2
.c_leer: call h_leer
    jmp .finp2
.c_borrara: call h_borrar_archivo
    jmp .finp2
.c_borrarc: call h_borrar_carpeta
    jmp .finp2
.c_donde: call h_donde
    jmp .finp2
.c_limpiar: call h_limpiar
    jmp .finp2
.c_eco: call h_eco
    jmp .finp2
.c_fecha: call h_fecha
    jmp .finp2
.c_quien: call h_quien
    jmp .finp2
.c_info: call h_info
    jmp .finp2
.c_version: call h_version
    jmp .finp2
.c_reiniciar: call h_reiniciar
    jmp .finp2
.c_apagar: call h_apagar
    jmp .finp2
.c_historial: call h_historial
    jmp .finp2
.c_limphist: call h_limpiar_historial
    jmp .finp2
.c_copiar: call h_copiar
    jmp .finp2
.c_mover: call h_mover
    jmp .finp2
.c_renombrar: call h_renombrar
    jmp .finp2
.c_buscar: call h_buscar_texto
    jmp .finp2
.c_editar: call h_editar
    jmp .finp2
.c_espacio: call h_espacio
    jmp .finp2
.c_guardar: call h_guardar_cmd
    jmp .finp2
.finp2:
    popa
    ret

; copia palabra SI -> DI (hasta espacio o 0), no avanza SI original? Usa lods/stos con copia local
copiar_palabra:
    pusha
.cpp:
    mov al, [si]
    cmp al, 0
    je .cpfin
    cmp al, ' '
    je .cpfin
    mov [di], al
    inc si
    inc di
    jmp .cpp
.cpfin:
    mov byte [di], 0
    popa
    ret

; ================= SISTEMA DE ARCHIVOS =================
; Tabla en 0x1000:0, 64 entradas x 36 bytes
; Datos en 0x1200 + i*0x20 (512 bytes cada uno)

fs_inicializar:
    pusha
    push es
    mov ax, 0x1000
    mov es, ax
    xor di, di
    mov cx, 64*36
    xor al, al
.limpiar_tabla:
    mov [es:di], al
    inc di
    loop .limpiar_tabla
    ; entrada 0 = raiz
    mov byte [es:0], 1
    mov byte [es:1], 0
    mov byte [es:2], '/'
    mov byte [es:3], 0
    mov word [es:34], 0
    pop es
    mov word [dir_actual], 0
    popa
    ret

; fs_leer_tipo AX=indice -> AL=tipo (preserva todo menos AX)
fs_leer_tipo:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx, ax
    mov ax, 0x1000
    mov es, ax
    mov ax, bx
    push bx
    mov bx, 36
    mul bx              ; AX = indice*36 (DX=0)
    mov di, ax
    pop bx
    mov al, [es:di]
    xor ah, ah
    mov bx, ax
    pop es
    pop di
    pop si
    pop dx
    pop cx
    mov ax, bx
    pop bx
    ret

; fs_leer_padre AX=indice -> AX=padre (preserva todo menos AX)
fs_leer_padre:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx, ax
    mov ax, 0x1000
    mov es, ax
    mov ax, bx
    push bx
    mov bx, 36
    mul bx
    mov di, ax
    pop bx
    xor ah, ah
    mov al, [es:di+1]
    mov bx, ax
    pop es
    pop di
    pop si
    pop dx
    pop cx
    mov ax, bx
    pop bx
    ret

; fs_copiar_nombre AX=indice, DI=destino (DS)
fs_copiar_nombre:
    pusha
    push es
    mov bx, ax
    mov ax, 0x1000
    mov es, ax
    mov ax, bx
    mov bx, 36
    mul bx
    mov si, ax
    add si, 2           ; offset nombre en tabla
    ; copiar 32 bytes max hasta 0
    mov cx, 32
.cnrep:
    mov al, [es:si]
    mov [di], al
    inc si
    inc di
    or al, al
    jz .cnfin
    loop .cnrep
    mov byte [di-1], 0
.cnfin:
    pop es
    popa
    ret

; fs_leer_tam AX -> AX=tam (preserva todo menos AX)
fs_leer_tam:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx, ax
    mov ax, 0x1000
    mov es, ax
    mov ax, bx
    push bx
    mov bx, 36
    mul bx
    mov di, ax
    pop bx
    mov ax, [es:di+34]
    mov bx, ax
    pop es
    pop di
    pop si
    pop dx
    pop cx
    mov ax, bx
    pop bx
    ret

; fs_buscar: AX=padre, SI=nombre -> AX=indice o FFFF
fs_buscar:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov dx, ax          ; padre buscado
    mov ax, 0x1000
    mov es, ax
    xor bx, bx          ; indice 0..63
.buscar_rep:
    cmp bx, 64
    jae .nob
    mov ax, bx
    push bx
    push dx
    mov bx, 36
    mul bx
    mov di, ax
    pop dx
    pop bx
    mov al, [es:di]     ; tipo
    or al, al
    jz .sigb
    mov al, [es:di+1]   ; padre
    cmp al, dl
    jne .sigb
    ; comparar nombre: SI vs ES:DI+2
    push bx
    push si
    push di
    add di, 2
    call comparar_es    ; compara DS:SI con ES:DI
    pop di
    pop si
    pop bx
    cmp ax, 0
    je .encontrado
.sigb:
    inc bx
    jmp .buscar_rep
.nob:
    mov ax, 0xFFFF
    jmp .bfin
.encontrado:
    mov ax, bx
.bfin:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

; comparar DS:SI con ES:DI (cadenas 0-term) -> AX 0 igual
comparar_es:
    push si
    push di
    push bx
.crep2:
    mov al, [si]
    mov bl, [es:di]
    cmp al, bl
    jne .d2
    or al, al
    jz .i2
    inc si
    inc di
    jmp .crep2
.i2:
    mov ax, 0
    jmp .cf2
.d2:
    mov ax, 1
.cf2:
    pop bx
    pop di
    pop si
    ret

; fs_libre -> AX indice o FFFF
fs_libre:
    push es
    push di
    push bx
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
.librep:
    cmp bx, 64
    jae .nolib
    mov ax, bx
    push bx
    mov bx, 36
    mul bx
    mov di, ax
    pop bx
    mov al, [es:di]
    or al, al
    jz .libre
    inc bx
    jmp .librep
.libre:
    mov ax, bx
    jmp .libfin
.nolib:
    mov ax, 0xFFFF
.libfin:
    pop bx
    pop di
    pop es
    ret

; validar nombre SI -> AX 0 ok, 1 largo, 2 malo/vacio
validar_nombre:
    push si
    push cx
    push bx
    call longitud
    cmp cx, 0
    je .malo
    cmp cx, 31
    ja .largo
    ; revisar caracteres
    mov bx, si
.vrep:
    mov al, [bx]
    or al, al
    jz .okv
    cmp al, ' '
    je .malo
    cmp al, '/'
    je .malo
    cmp al, '\'
    je .malo
    inc bx
    jmp .vrep
.okv:
    mov ax, 0
    jmp .vfin
.largo:
    mov ax, 1
    jmp .vfin
.malo:
    mov ax, 2
.vfin:
    pop bx
    pop cx
    pop si
    ret

tmp_crear_tipo db 0
tmp_crear_padre db 0
; fs_crear: BL=tipo(1/2), AX=padre, SI=nombre -> AX indice o FFFF
fs_crear:
    push bx
    push cx
    push dx
    push di
    push es
    push si
    mov [cs:tmp_crear_padre], al
    mov [cs:tmp_crear_tipo], bl
    ; validar
    call validar_nombre
    cmp ax, 0
    jne .c_error
    ; existe?
    pop si
    push si
    mov al, [cs:tmp_crear_padre]
    xor ah, ah
    mov ax, ax
    call fs_buscar
    cmp ax, 0xFFFF
    jne .c_existe
    ; libre?
    call fs_libre
    cmp ax, 0xFFFF
    je .c_lleno
    mov cx, ax          ; indice nuevo
    ; escribir entrada
    mov ax, 0x1000
    mov es, ax
    mov ax, cx
    push cx
    mov bx, 36
    mul bx
    mov di, ax
    pop cx
    pop si
    push cx
    mov al, [cs:tmp_crear_tipo]
    mov [es:di], al
    mov al, [cs:tmp_crear_padre]
    mov [es:di+1], al
    ; copiar nombre
    add di, 2
    push di
.copl:
    lodsb
    mov [es:di], al
    inc di
    or al, al
    jnz .copl
    pop di
    ; tam = 0 (di-2+34?) recalcular
    pop cx
    push cx
    mov ax, cx
    mov bx, 36
    mul bx
    mov di, ax
    mov word [es:di+34], 0
    pop ax              ; indice
    jmp .c_fin
.c_existe:
    pop si
    mov ax, 0xFFFE      ; existe
    jmp .c_fin
.c_lleno:
    pop si
    mov ax, 0xFFFF
    jmp .c_fin
.c_error:
    pop si
    mov ax, 0xFFFD
.c_fin:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    ret

; fs_borrar AX=indice (marca libre)
fs_borrar:
    pusha
    push es
    mov bx, ax
    mov ax, 0x1000
    mov es, ax
    mov ax, bx
    mov bx, 36
    mul bx
    mov di, ax
    mov byte [es:di], 0
    pop es
    popa
    ret

; tiene_hijos AX=indice -> AX 1 si tiene, 0 si no
tiene_hijos:
    push bx
    push cx
    push es
    push di
    mov bx, ax
    mov ax, 0x1000
    mov es, ax
    xor cx, cx
    xor di, di
.threp:
    cmp di, 64
    jae .thfin
    mov ax, di
    push bx
    push di
    mov bx, 36
    mul bx
    mov si, ax
    pop di
    pop bx
    mov al, [es:si]
    or al, al
    jz .thsig
    mov al, [es:si+1]
    cmp al, bl
    jne .thsig
    ; es hijo (pero no el mismo? raiz padre=0 y indice 0: si bx==0, indice 0 tiene padre 0, no contar como hijo de si mismo)
    cmp di, bx
    je .thsig
    mov cx, 1
    jmp .thfin
.thsig:
    inc di
    jmp .threp
.thfin:
    mov ax, cx
    pop di
    pop es
    pop cx
    pop bx
    ret

; encontrar primer hijo: BX=padre -> AX=hijo o FFFF (preserva BX,CX,DX,SI,DI,ES)
encontrar_hijo:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov cx, bx          ; padre en CX
    xor dx, dx          ; idx 0..63
.eh_rep:
    cmp dx, 64
    jae .eh_no
    mov ax, dx
    call fs_leer_tipo
    or al, al
    jz .eh_sig
    mov ax, dx
    call fs_leer_padre
    cmp al, cl
    jne .eh_sig
    cmp dx, cx
    je .eh_sig
    mov ax, dx
    jmp .eh_fin
.eh_sig:
    inc dx
    jmp .eh_rep
.eh_no:
    mov ax, 0xFFFF
.eh_fin:
    mov bx, ax
    pop es
    pop di
    pop si
    pop dx
    pop cx
    mov ax, bx
    pop bx
    ret

tmp_borrar_raiz dw 0
; borrar con hijos (iterativo, sin recursion): AX=indice padre a borrar
borrar_rec:
    pusha
    mov [cs:tmp_borrar_raiz], ax
.bucle_padre:
    mov bx, [cs:tmp_borrar_raiz]
    call encontrar_hijo ; BX=padre -> AX=hijo
    cmp ax, 0xFFFF
    je .borrar_raiz2
    mov cx, ax          ; CX = candidato, bajar hasta hoja
.bajar:
    mov bx, cx
    call encontrar_hijo ; hijo de CX?
    cmp ax, 0xFFFF
    je .es_hoja
    mov cx, ax
    jmp .bajar
.es_hoja:
    mov ax, cx
    call fs_borrar
    jmp .bucle_padre
.borrar_raiz2:
    mov ax, [cs:tmp_borrar_raiz]
    call fs_borrar
    popa
    ret

tmp_seg_ver dw 0
; segmento datos de archivo AX -> AX=segmento (preserva todo menos AX)
seg_datos:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx, ax
    mov ax, bx
    mov bx, 0x20
    mul bx
    add ax, 0x1200
    mov [cs:tmp_seg_ver], ax
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, [cs:tmp_seg_ver]
    ret
tmp_modo_ver db 0
tmp_cont_ver dw 0
tmp_idx_ver dw 0

; ================= DISCO PERSISTENTE v1.1 =================
; Disco de datos 0x80, 10MB. LBA 0=cabecera, 1-5=tabla, 6-69=datos.
; Cabecera 512B: magic[8]="FACILOS1", version dw=1, checksum dd, reservado.
DISCO_DL equ 0x80
DISCO_LBA_CAB equ 0
DISCO_LBA_TAB equ 1
DISCO_NTAB equ 5
DISCO_LBA_DAT equ 6
DISCO_NDAT equ 64
disco_ok db 0            ; 1 si hay disco valido
disco_sucio db 0
disk_packet times 16 db 0
msg_disco_ok db 'Disco de datos listo. Archivos recuperados.',13,10,0
msg_disco_nuevo db 'Disco de datos nuevo. Se guardara al usar guardar.',13,10,0
msg_disco_no db 'Sin disco de datos. Los cambios solo estan en memoria.',13,10,0
msg_disco_mal db 'Disco con error. Se usa memoria temporal.',13,10,0
msg_guardado_ok db 'Guardado en disco.',13,10,0
msg_guardado_mal db 'Error al guardar en disco.',13,10,0
msg_guardado_nodisco db 'Sin disco de datos. Nada que guardar.',13,10,0
msg_espacio_tit db 'Espacio en disco:',13,10,0
msg_espacio_usados db '  Ocupados: ',0
msg_espacio_libres db '  Libres: ',0
msg_espacio_bytes db '  Bytes en archivos: ',0
msg_espacio_max db ' / 32768',13,10,0
msg_espacio_nodisco db 'Sin disco: solo memoria (64 espacios).',13,10,0

; comprobar extensiones INT13 (DL=0x80) -> AX 1 si soporta LBA
disco_tiene_lba:
    push bx
    push cx
    push dx
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, DISCO_DL
    int 0x13
    jc .no
    cmp bx, 0xAA55
    jne .no
    test cx, 1
    jz .no
    mov ax, 1
    jmp .fin
.no:
    xor ax, ax
.fin:
    pop dx
    pop cx
    pop bx
    ret

; leer LBA: EAX=LBA, ES:BX=dest, CX=sectores, DL=unidad -> AX 0 ok
disco_leer:
    pusha
    push es
    mov [cs:disk_packet], byte 0x10
    mov [cs:disk_packet+1], byte 0
    mov [cs:disk_packet+2], cx
    mov [cs:disk_packet+4], bx
    mov [cs:disk_packet+6], es
    mov [cs:disk_packet+8], eax
    xor eax, eax
    mov [cs:disk_packet+12], eax
    mov si, disk_packet
    mov ah, 0x42
    ; DL ya es unidad
    int 0x13
    jnc .ok
    mov word [cs:.rc], 1
    jmp .out
.ok:
    mov word [cs:.rc], 0
.out:
    pop es
    popa
    mov ax, [cs:.rc]
    ret
.rc dw 0

; escribir LBA: EAX=LBA, DS:SI=origen? No, ES:BX=origen, CX=sectores -> AX 0 ok
; OJO: AH=43 usa DS:SI para el paquete (no ES:BX). Usamos DS:SI=packet.
disco_escribir:
    pusha
    push es
    push ds
    mov [cs:disk_packet], byte 0x10
    mov [cs:disk_packet+1], byte 0
    mov [cs:disk_packet+2], cx
    mov [cs:disk_packet+4], bx
    mov [cs:disk_packet+6], es
    mov [cs:disk_packet+8], eax
    xor eax, eax
    mov [cs:disk_packet+12], eax
    push cs
    pop ds
    mov si, disk_packet
    mov ah, 0x43
    mov al, 0x00
    int 0x13
    jnc .ok2
    mov word [cs:.rc2], 1
    jmp .out2
.ok2:
    mov word [cs:.rc2], 0
.out2:
    pop ds
    pop es
    popa
    mov ax, [cs:.rc2]
    ret
.rc2 dw 0

; checksum32 de tabla(2304B en 0x1000:0) + datos(32768B) -> DX:AX
disco_checksum:
    pusha
    push es
    xor bx, bx          ; suma baja
    xor cx, cx          ; suma alta (acarreo simple)
    mov ax, 0x1000
    mov es, ax
    xor di, di
    mov dx, 2304
.cs_tab:
    mov al, [es:di]
    xor ah, ah
    add bx, ax
    adc cx, 0
    inc di
    dec dx
    jnz .cs_tab
    ; datos 64*512
    xor dx, dx          ; idx archivo 0..63
.cs_arc:
    cmp dx, 64
    jae .cs_fin
    push dx
    push bx
    push cx
    mov ax, dx
    call seg_datos      ; AX=seg
    mov es, ax
    pop cx
    pop bx
    xor di, di
    mov si, 512
.cs_dat:
    mov al, [es:di]
    xor ah, ah
    add bx, ax
    adc cx, 0
    inc di
    dec si
    jnz .cs_dat
    pop dx
    inc dx
    jmp .cs_arc
.cs_fin:
    mov [cs:.sum_lo], bx
    mov [cs:.sum_hi], cx
    pop es
    popa
    mov ax, [cs:.sum_lo]
    mov dx, [cs:.sum_hi]
    ret
.sum_lo dw 0
.sum_hi dw 0

; cargar disco al arrancar (llamar tras fs_inicializar). No borra si falla.
cargar_disco:
    pusha
    call disco_tiene_lba
    cmp ax, 1
    jne .sin_disco
    ; leer cabecera a 0x7000:0
    mov ax, 0x7000
    mov es, ax
    xor bx, bx
    mov eax, DISCO_LBA_CAB
    mov cx, 1
    mov dl, DISCO_DL
    call disco_leer
    cmp ax, 0
    jne .error_io
    ; comprobar magic "FACILOS1"
    push es
    mov ax, 0x7000
    mov es, ax
    mov si, 0
    mov di, .magic
    mov cx, 8
.cmp_mag:
    mov al, [es:si]
    mov bl, [cs:di]
    cmp al, bl
    jne .mag_mal
    inc si
    inc di
    loop .cmp_mag
    pop es
    ; magic ok: leer tabla 5 sectores a 0x1000:0
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov eax, DISCO_LBA_TAB
    mov cx, DISCO_NTAB
    mov dl, DISCO_DL
    call disco_leer
    cmp ax, 0
    jne .error_io
    ; leer datos 64 sectores: bucle de 8 en 8 para no cruzar 64KB
    mov dx, 0           ; idx 0..63
.rd_dat:
    cmp dx, 64
    jae .verificar
    push dx
    mov ax, dx
    call seg_datos
    mov es, ax
    xor bx, bx
    pop dx
    push dx
    mov eax, edx
    and eax, 0xFFFF
    add eax, DISCO_LBA_DAT
    mov cx, 1
    mov dl, DISCO_DL
    call disco_leer
    pop dx
    cmp ax, 0
    jne .error_io
    inc dx
    jmp .rd_dat
.verificar:
    ; comparar checksum con cabecera (0x7000:8 = lo, 10 = hi)
    call disco_checksum ; DX:AX calculado
    push ax
    push dx
    mov bx, 0x7000
    mov es, bx
    mov bx, [es:8]
    mov cx, [es:10]
    pop dx
    pop ax
    cmp ax, bx
    jne .checksum_mal
    cmp dx, cx
    jne .checksum_mal
    mov byte [cs:disco_ok], 1
    mov si, msg_disco_ok
    call imprimir_ok
    popa
    ret
.mag_mal:
    pop es
    ; disco presente pero vacio/nuevo -> inicializar vacio y avisar
    call fs_inicializar
    mov byte [cs:disco_ok], 1
    mov si, msg_disco_nuevo
    call imprimir_aviso
    popa
    ret
.checksum_mal:
    call fs_inicializar
    mov byte [cs:disco_ok], 0
    mov si, msg_disco_mal
    call imprimir_error
    popa
    ret
.error_io:
    call fs_inicializar
    mov byte [cs:disco_ok], 0
    mov si, msg_disco_mal
    call imprimir_error
    popa
    ret
.sin_disco:
    mov byte [cs:disco_ok], 0
    mov si, msg_disco_no
    call imprimir_aviso
    popa
    ret
.magic db 'FACILOS1'

; guardar disco (forzado). -> AX 0 ok, 1 sin disco, 2 error
guardar_disco:
    pusha
    call disco_tiene_lba
    cmp ax, 1
    jne .g_nodisco
    call disco_checksum ; DX:AX
    push ax
    push dx
    ; construir cabecera en 0x7000:0
    mov bx, 0x7000
    mov es, bx
    xor di, di
    mov si, .mg2
    mov cx, 8
.g_cop:
    mov al, [cs:si]
    mov [es:di], al
    inc si
    inc di
    loop .g_cop
    mov word [es:8], 1  ; version? offset 8: usamos 8-9 version, 10-11? Definimos: 0-7 magic, 8-9 version, 10-11 reservado, 12-13 sum_lo? No, arriba leemos 8=lo,10=hi. Unificar: 8-9 sum_lo, 10-11 sum_hi, 12-13 version.
    ; Para compat con cargar (lee 8=lo,10=hi): guardar lo en 8, hi en 10, version en 12
    pop dx
    pop ax
    mov [es:8], ax
    mov [es:10], dx
    mov word [es:12], 1
    ; escribir cabecera
    mov ax, 0x7000
    mov es, ax
    xor bx, bx
    mov eax, DISCO_LBA_CAB
    mov cx, 1
    mov dl, DISCO_DL
    call disco_escribir
    cmp ax, 0
    jne .g_error
    ; escribir tabla
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov eax, DISCO_LBA_TAB
    mov cx, DISCO_NTAB
    mov dl, DISCO_DL
    call disco_escribir
    cmp ax, 0
    jne .g_error
    ; escribir datos 1 a 1
    mov dx, 0
.g_dat:
    cmp dx, 64
    jae .g_ok
    push dx
    mov ax, dx
    call seg_datos
    mov es, ax
    xor bx, bx
    pop dx
    push dx
    mov eax, edx
    and eax, 0xFFFF
    add eax, DISCO_LBA_DAT
    mov cx, 1
    mov dl, DISCO_DL
    call disco_escribir
    pop dx
    cmp ax, 0
    jne .g_error
    inc dx
    jmp .g_dat
.g_ok:
    mov byte [cs:disco_ok], 1
    mov word [cs:.grc], 0
    jmp .g_out
.g_nodisco:
    mov word [cs:.grc], 1
    jmp .g_out
.g_error:
    mov word [cs:.grc], 2
.g_out:
    popa
    mov ax, [cs:.grc]
    ret
.grc dw 0
.mg2 db 'FACILOS1'

; autosave silencioso tras mutaciones (solo avisa en rojo si falla con disco)
autosave:
    pusha
    call guardar_disco
    cmp ax, 0
    je .a_fin
    cmp ax, 1
    je .a_fin           ; sin disco: silencio (ya avisado al arrancar)
    mov si, msg_guardado_mal
    call imprimir_error
.a_fin:
    popa
    ret

; ================= HANDLERS =================

h_ayuda:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .corta
    mov di, opt_todo
    call cadenas_iguales
    cmp ax, 0
    je .todo
    ; buscar comando especifico
    mov di, cmd_ver
    call cadenas_iguales
    cmp ax, 0
    je .dver
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_entrar
    call cadenas_iguales
    cmp ax, 0
    je .dentrar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_crearc
    call cadenas_iguales
    cmp ax, 0
    je .dcrearc
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_creara
    call cadenas_iguales
    cmp ax, 0
    je .dcreara
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_escribir
    call cadenas_iguales
    cmp ax, 0
    je .describir
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_leer
    call cadenas_iguales
    cmp ax, 0
    je .dleer
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_borrara
    call cadenas_iguales
    cmp ax, 0
    je .dborrara
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_borrarc
    call cadenas_iguales
    cmp ax, 0
    je .dborrarc
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_donde
    call cadenas_iguales
    cmp ax, 0
    je .ddonde
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_limpiar
    call cadenas_iguales
    cmp ax, 0
    je .dlimpiar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_eco
    call cadenas_iguales
    cmp ax, 0
    je .deco
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_fecha
    call cadenas_iguales
    cmp ax, 0
    je .dfecha
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_quien
    call cadenas_iguales
    cmp ax, 0
    je .dquien
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_info
    call cadenas_iguales
    cmp ax, 0
    je .dinfo
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_version
    call cadenas_iguales
    cmp ax, 0
    je .dversion
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_reiniciar
    call cadenas_iguales
    cmp ax, 0
    je .dreiniciar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_apagar
    call cadenas_iguales
    cmp ax, 0
    je .dapagar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .dayuda
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_copiar
    call cadenas_iguales
    cmp ax, 0
    je .dcopiar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_mover
    call cadenas_iguales
    cmp ax, 0
    je .dmover
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_renombrar
    call cadenas_iguales
    cmp ax, 0
    je .drenombrar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_buscar
    call cadenas_iguales
    cmp ax, 0
    je .dbuscar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_editar
    call cadenas_iguales
    cmp ax, 0
    je .deditar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_espacio
    call cadenas_iguales
    cmp ax, 0
    je .despacio
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_guardar
    call cadenas_iguales
    cmp ax, 0
    je .dguardar
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_historial
    call cadenas_iguales
    cmp ax, 0
    je .dhistorial
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_limphist
    call cadenas_iguales
    cmp ax, 0
    je .dlimphist
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_limpiar2
    call cadenas_iguales
    cmp ax, 0
    je .dlimpiar
    mov si, msg_uso_ayuda
    call imprimir_error
    jmp .fin
.corta:
    mov si, msg_ayuda_corta
    call imprimir
    jmp .fin
.todo:
    mov si, msg_ayuda_todo
    call imprimir
    jmp .fin
.dver: mov si, msg_desc_ver
    call imprimir
    jmp .fin
.dentrar: mov si, msg_desc_entrar
    call imprimir
    jmp .fin
.dcrearc: mov si, msg_desc_crearc
    call imprimir
    jmp .fin
.dcreara: mov si, msg_desc_creara
    call imprimir
    jmp .fin
.describir: mov si, msg_desc_escribir
    call imprimir
    jmp .fin
.dleer: mov si, msg_desc_leer
    call imprimir
    jmp .fin
.dborrara: mov si, msg_desc_borrara
    call imprimir
    jmp .fin
.dborrarc: mov si, msg_desc_borrarc
    call imprimir
    jmp .fin
.ddonde: mov si, msg_desc_donde
    call imprimir
    jmp .fin
.dlimpiar: mov si, msg_desc_limpiar
    call imprimir
    jmp .fin
.deco: mov si, msg_desc_eco
    call imprimir
    jmp .fin
.dfecha: mov si, msg_desc_fecha
    call imprimir
    jmp .fin
.dquien: mov si, msg_desc_quien
    call imprimir
    jmp .fin
.dinfo: mov si, msg_desc_info
    call imprimir
    jmp .fin
.dversion: mov si, msg_desc_version
    call imprimir
    jmp .fin
.dreiniciar: mov si, msg_desc_reiniciar
    call imprimir
    jmp .fin
.dapagar: mov si, msg_desc_apagar
    call imprimir
    jmp .fin
.dayuda: mov si, msg_desc_ayuda
    call imprimir
    jmp .fin
.dcopiar: mov si, msg_desc_copiar
    call imprimir
    jmp .fin
.dmover: mov si, msg_desc_mover
    call imprimir
    jmp .fin
.drenombrar: mov si, msg_desc_renombrar
    call imprimir
    jmp .fin
.dbuscar: mov si, msg_desc_buscar
    call imprimir
    jmp .fin
.deditar: mov si, msg_desc_editar
    call imprimir
    jmp .fin
.despacio: mov si, msg_desc_espacio
    call imprimir
    jmp .fin
.dguardar: mov si, msg_desc_guardar
    call imprimir
    jmp .fin
.dhistorial: mov si, msg_desc_historial
    call imprimir
    jmp .fin
.dlimphist: mov si, msg_desc_limphist
    call imprimir
.fin:
    popa
    ret

h_ver:
    pusha
    mov si, buffer_args
    call saltar_espacios
    ; modos: 0 todo, 1 carpetas, 2 archivos, 3 detalle(todo)
    mov bx, 0
    cmp byte [si], 0
    je .listar
    mov di, opt_carpetas
    call cadenas_iguales
    cmp ax, 0
    je .modo1
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_archivos
    call cadenas_iguales
    cmp ax, 0
    je .modo2
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_todo
    call cadenas_iguales
    cmp ax, 0
    je .modo3
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ay
    mov si, msg_desc_ver
    call imprimir
    jmp .finv
.ay: mov si, msg_desc_ver
    call imprimir
    jmp .finv
.modo1: mov bx, 1
    jmp .listar
.modo2: mov bx, 2
    jmp .listar
.modo3: mov bx, 3
.listar:
    mov [cs:tmp_modo_ver], bl
    mov word [cs:tmp_cont_ver], 0
    mov word [cs:tmp_idx_ver], 0
.vrep:
    mov ax, [cs:tmp_idx_ver]
    cmp ax, 64
    jae .vfin
    mov cx, [dir_actual]
    cmp ax, cx
    je .vsig               ; no listar la carpeta actual (evita mostrar / en raiz)
    call fs_leer_tipo      ; AL=tipo, preserva DX/CX
    mov bl, al
    or bl, bl
    jz .vsig
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_padre     ; AX=padre
    mov cx, [dir_actual]
    cmp ax, cx
    jne .vsig
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_tipo
    mov bl, al             ; BL=tipo
    mov bh, [cs:tmp_modo_ver] ; BH=modo
    cmp bh, 0
    je .mostrar
    cmp bh, 3
    je .mostrar_det
    cmp bh, 1
    je .fcarp
    cmp bh, 2
    je .farch
    jmp .vsig
.fcarp:
    cmp bl, 1
    jne .vsig
    jmp .mostrar
.farch:
    cmp bl, 2
    jne .vsig
    jmp .mostrar
.mostrar_det:
    cmp bl, 1
    je .detc
    mov si, msg_et_archivo
    call imprimir_blanco
    jmp .detn
.detc:
    mov si, msg_et_carpeta
    call imprimir_azul
.detn:
    mov ax, [cs:tmp_idx_ver]
    mov di, buffer_nombre
    call fs_copiar_nombre
    mov si, buffer_nombre
    cmp bl, 1
    je .detn_azul
    call imprimir_blanco
    jmp .detn_sig
.detn_azul:
    call imprimir_azul
.detn_sig:
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_tipo
    cmp al, 2
    jne .detnl
    mov al, ' '
    call imprimir_char
    mov al, '('
    call imprimir_char
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_tam
    call imprimir_numero
    mov al, ')'
    call imprimir_char
.detnl:
    call nueva_linea
    inc word [cs:tmp_cont_ver]
    jmp .vsig
.mostrar:
    mov ax, [cs:tmp_idx_ver]
    mov di, buffer_nombre
    call fs_copiar_nombre
    mov si, buffer_nombre
    cmp bl, 1
    je .most_azul
    call imprimir_blanco
    jmp .most_nl
.most_azul:
    call imprimir_azul
.most_nl:
    call nueva_linea
    inc word [cs:tmp_cont_ver]
    jmp .vsig
.vsig:
    inc word [cs:tmp_idx_ver]
    jmp .vrep
.vfin:
    cmp word [cs:tmp_cont_ver], 0
    jne .finv
    mov si, msg_vacia
    call imprimir
.finv:
    popa
    ret

h_entrar:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .falta
    mov di, opt_padre
    call cadenas_iguales
    cmp ax, 0
    je .padre
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_raiz
    call cadenas_iguales
    cmp ax, 0
    je .raiz2
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ay2
    ; buscar carpeta hija
    mov si, buffer_args
    call saltar_espacios
    ; copiar nombre (hasta espacio)
    mov di, buffer_nombre
    call copiar_palabra
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .noex
    mov bx, ax
    mov ax, bx
    call fs_leer_tipo
    cmp al, 1
    jne .no_carp
    mov [dir_actual], bx
    jmp .fine
.padre:
    mov ax, [dir_actual]
    cmp ax, 0
    je .fine
    call fs_leer_padre
    mov [dir_actual], ax
    jmp .fine
.raiz2:
    mov word [dir_actual], 0
    jmp .fine
.ay2:
    mov si, msg_desc_entrar
    call imprimir
    jmp .fine
.falta:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .fine
.noex:
    mov si, msg_no_existe
    call imprimir_error
    jmp .fine
.no_carp:
    mov si, msg_no_es_carpeta
    call imprimir_error
.fine:
    popa
    ret

h_crear_carpeta:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .falta2
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ay3
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_nombre
    call copiar_palabra
    mov si, buffer_nombre
    call validar_nombre
    cmp ax, 1
    je .largo
    cmp ax, 2
    je .malo
    mov si, buffer_nombre
    mov ax, [dir_actual]
    mov bl, 1
    call fs_crear
    cmp ax, 0xFFFE
    je .existe
    cmp ax, 0xFFFF
    je .lleno
    cmp ax, 0xFFFD
    je .malo
    mov si, msg_creado_carpeta
    call imprimir_ok
    call autosave
    jmp .fin3
.ay3: mov si, msg_desc_crearc
    call imprimir
    jmp .fin3
.falta2: mov si, msg_falta_nombre
    call imprimir
    jmp .fin3
.largo:     mov si, msg_nombre_largo
    call imprimir_error
    jmp .fin3
.malo: mov si, msg_nombre_malo
    call imprimir
    jmp .fin3
.existe: mov si, msg_ya_existe
    call imprimir
    jmp .fin3
.lleno: mov si, msg_sin_espacio
    call imprimir
.fin3:
    popa
    ret

h_crear_archivo:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .falta2b
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ay3b
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_nombre
    call copiar_palabra
    mov si, buffer_nombre
    call validar_nombre
    cmp ax, 1
    je .largob
    cmp ax, 2
    je .malob
    mov si, buffer_nombre
    mov ax, [dir_actual]
    mov bl, 2
    call fs_crear
    cmp ax, 0xFFFE
    je .existeb
    cmp ax, 0xFFFF
    je .llenob
    cmp ax, 0xFFFD
    je .malob
    mov si, msg_creado_archivo
    call imprimir_ok
    call autosave
    jmp .fin3b
.ay3b: mov si, msg_desc_creara
    call imprimir
    jmp .fin3b
.falta2b: mov si, msg_falta_nombre
    call imprimir
    jmp .fin3b
.largob:     mov si, msg_nombre_largo
    call imprimir_error
    jmp .fin3b
.malob: mov si, msg_nombre_malo
    call imprimir
    jmp .fin3b
.existeb: mov si, msg_ya_existe
    call imprimir
    jmp .fin3b
.llenob: mov si, msg_sin_espacio
    call imprimir
.fin3b:
    popa
    ret

; escribir_archivo nombre texto | nombre --agregar texto
h_escribir:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .faltae
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .aye
    ; extraer nombre
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_nombre
    call copiar_palabra
    ; avanzar si al segundo token
    mov si, buffer_args
    call saltar_espacios
.avn:
    mov al, [si]
    cmp al, 0
    je .faltat
    cmp al, ' '
    je .sp1
    inc si
    jmp .avn
.sp1:
    call saltar_espacios
    cmp byte [si], 0
    je .faltat
    ; si es --agregar?
    push si
    mov di, opt_agregar
    ; comparar solo primera palabra de si con --agregar
    ; copiar primera palabra a buffer_texto temp para comparar
    mov di, buffer_texto
    call copiar_palabra
    mov si, buffer_texto
    mov di, opt_agregar
    call cadenas_iguales
    pop si
    cmp ax, 0
    je .modo_agregar
    ; modo sobrescribir: texto = resto (si)
    mov di, buffer_texto
    call copiar_cadena
    mov byte [cs:.modo], 0
    jmp .buscar_arch
.modo_agregar:
    ; saltar --agregar
.av2:
    mov al, [si]
    cmp al, 0
    je .faltat
    cmp al, ' '
    je .sp2
    inc si
    jmp .av2
.sp2:
    call saltar_espacios
    cmp byte [si], 0
    je .faltat
    mov di, buffer_texto
    call copiar_cadena
    mov byte [cs:.modo], 1
.buscar_arch:
    ; buscar archivo
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .noe
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .noarch
    ; longitud texto
    mov si, buffer_texto
    call longitud      ; CX = len
    cmp cx, 512
    ja .largot
    ; si modo agregar, sumar tam actual
    cmp byte [cs:.modo], 1
    jne .sob
    mov ax, bx
    call fs_leer_tam   ; AX actual
    add ax, cx
    cmp ax, 512
    ja .largot
    ; copiar al final
    push bx
    push cx
    mov ax, bx
    call fs_leer_tam   ; AX offset
    mov dx, ax
    mov ax, bx
    call seg_datos     ; AX segmento
    mov es, ax
    mov di, dx
    mov si, buffer_texto
    pop cx
    ; copiar CX bytes
    push cx
.acop:
    cmp cx, 0
    je .afin
    lodsb
    mov [es:di], al
    inc di
    dec cx
    jmp .acop
.afin:
    pop cx
    pop bx
    ; actualizar tam
    mov ax, bx
    call fs_leer_tam
    add ax, cx
    ; guardar
    push ax
    mov ax, bx
    mov bx, 36
    push bx
    mov bx, ax
    mov ax, bx
    pop bx
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    pop ax
    mov [es:di+34], ax
    mov si, msg_escrito
    call imprimir_ok
    call autosave
    jmp .fine2
.sob:
    ; sobrescribir desde 0
    push bx
    push cx
    mov ax, bx
    call seg_datos
    mov es, ax
    xor di, di
    mov si, buffer_texto
    cmp cx, 0
    je .sfin
.scop:
    lodsb
    mov [es:di], al
    inc di
    loop .scop
.sfin:
    pop cx
    pop bx
    ; guardar tam = cx
    push cx
    push bx
    mov ax, bx
    mov bx, 36
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    pop bx
    pop ax
    ; AX=cx? cuidado pila: push cx, push bx -> pop bx=bx, pop ax=cx. Bien pero invertimos?
    ; arriba: push cx, push bx, luego ... pop bx (bx), pop ax (cx). Correcto: AX=tam
    mov [es:di+34], ax
    mov si, msg_escrito
    call imprimir_ok
    call autosave
    jmp .fine2
.faltae:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .fine2
.faltat:
    mov si, msg_falta_texto
    call imprimir_error
    jmp .fine2
.aye:
    mov si, msg_desc_escribir
    call imprimir
    jmp .fine2
.noe:
    mov si, msg_no_existe
    call imprimir_error
    jmp .fine2
.noarch:
    mov si, msg_no_es_archivo
    call imprimir_error
    jmp .fine2
.largot:
    mov si, msg_texto_largo
    call imprimir_error
.fine2:
    popa
    ret
.modo db 0

h_leer:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .faltal
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ayl
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_nombre
    call copiar_palabra
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .noel
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .noarchl
    mov ax, bx
    call fs_leer_tam
    mov cx, ax
    cmp cx, 0
    je .finl
    mov ax, bx
    call seg_datos
    mov es, ax
    xor si, si
.lrep2:
    cmp cx, 0
    je .elnl
    mov al, [es:si]
    inc si
    dec cx
    push es
    push si
    push cx
    call imprimir_char
    pop cx
    pop si
    pop es
    jmp .lrep2
.elnl:
    call nueva_linea
    jmp .finl
.faltal:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .finl
.ayl:
    mov si, msg_desc_leer
    call imprimir
    jmp .finl
.noel:
    mov si, msg_no_existe
    call imprimir_error
    jmp .finl
.noarchl:
    mov si, msg_no_es_archivo
    call imprimir_error
.finl:
    popa
    ret

h_borrar_archivo:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .faltab
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ayb
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_nombre
    call copiar_palabra
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .noeb
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .noarchb
    mov ax, bx
    call fs_borrar
    mov si, msg_borrado
    call imprimir_ok
    call autosave
    jmp .finb
.faltab:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .finb
.ayb:
    mov si, msg_desc_borrara
    call imprimir
    jmp .finb
.noeb:
    mov si, msg_no_existe
    call imprimir_error
    jmp .finb
.noarchb:
    mov si, msg_no_es_archivo
    call imprimir_error
.finb:
    popa
    ret

h_borrar_carpeta:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .faltac
    ; detectar --forzar en args (puede ser segundo token)
    ; copiar primer token a nombre
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_nombre
    call copiar_palabra
    ; buscar --forzar en resto
    mov si, buffer_args
    call saltar_espacios
.av3:
    mov al, [si]
    cmp al, 0
    je .sin_forzar
    cmp al, ' '
    je .sp3
    inc si
    jmp .av3
.sp3:
    call saltar_espacios
    ; si es --ayuda?
    push si
    mov di, buffer_texto
    call copiar_palabra
    mov si, buffer_texto
    mov di, opt_ayuda
    call cadenas_iguales
    pop si
    cmp ax, 0
    je .ayc
    push si
    mov di, buffer_texto
    call copiar_palabra
    mov si, buffer_texto
    mov di, opt_forzar
    call cadenas_iguales
    pop si
    cmp ax, 0
    je .con_forzar
.sin_forzar:
    mov byte [cs:.forzar], 0
    jmp .doc
.con_forzar:
    mov byte [cs:.forzar], 1
.doc:
    ; si nombre es --ayuda?
    mov si, buffer_nombre
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ayc
    ; si nombre es --forzar (orden inverso: borrar_carpeta --forzar nombre)? soportar?
    mov si, buffer_nombre
    mov di, opt_forzar
    call cadenas_iguales
    cmp ax, 0
    je .faltac
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .noec
    cmp ax, 0
    je .noraiz
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 1
    jne .noarcc
    mov ax, bx
    call tiene_hijos
    cmp ax, 0
    je .borra_simple
    cmp byte [cs:.forzar], 1
    je .borra_forz
    mov si, msg_carpeta_no_vacia
    call imprimir_aviso
    jmp .finc
.borra_forz:
    mov ax, bx
    call borrar_rec
    mov si, msg_borrado
    call imprimir_ok
    call autosave
    jmp .finc
.borra_simple:
    mov ax, bx
    call fs_borrar
    mov si, msg_borrado
    call imprimir_ok
    call autosave
    jmp .finc
.faltac:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .finc
.ayc:
    mov si, msg_desc_borrarc
    call imprimir
    jmp .finc
.noec:
    mov si, msg_no_existe
    call imprimir_error
    jmp .finc
.noraiz:
    mov si, msg_no_es_carpeta
    call imprimir_error
    jmp .finc
.noarcc:
    mov si, msg_no_es_carpeta
    call imprimir_error
.finc:
    popa
    ret
.forzar db 0

h_donde:
    pusha
    call construir_ruta
    mov si, buffer_ruta
    call imprimir
    call nueva_linea
    popa
    ret

h_limpiar:
    pusha
    mov ax, 0x0003
    int 0x10
    popa
    ret

h_historial:
    pusha
    mov al, [cs:hist_count]
    or al, al
    jz .hh_vacio
    xor bx, bx
.hh_rep:
    mov al, [cs:hist_count]
    cmp bl, al
    jae .hh_fin
    push bx
    mov ax, bx
    inc ax
    call imprimir_numero
    mov si, msg_punto
    call imprimir
    pop bx
    push bx
    mov al, bl
    mov di, buffer_texto
    call hist_cargar
    mov si, buffer_texto
    call imprimir
    call nueva_linea
    pop bx
    inc bx
    jmp .hh_rep
.hh_fin:
    popa
    ret
.hh_vacio:
    mov si, msg_sin_hist
    call imprimir
    popa
    ret

h_limpiar_historial:
    pusha
    mov byte [cs:hist_count], 0
    mov byte [cs:hist_pos], 0
    mov si, msg_hist_ok
    call imprimir_ok
    popa
    ret

h_eco:
    pusha
    mov si, buffer_args
    call imprimir
    call nueva_linea
    popa
    ret

; imprimir numero AX decimal
imprimir_numero:
    pusha
    cmp ax, 0
    jne .nz
    mov al, '0'
    call imprimir_char
    jmp .fn
.nz:
    xor cx, cx
    mov bx, 10
.divrep:
    xor dx, dx
    div bx
    push dx
    inc cx
    or ax, ax
    jnz .divrep
.popl:
    pop dx
    mov al, dl
    add al, '0'
    call imprimir_char
    loop .popl
.fn:
    popa
    ret

h_fecha:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .ambas
    mov di, opt_hora
    call cadenas_iguales
    cmp ax, 0
    je .solo_hora
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_fecha
    call cadenas_iguales
    cmp ax, 0
    je .solo_fecha
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ayf
    mov si, msg_desc_fecha
    call imprimir
    jmp .finf
.ayf:
    mov si, msg_desc_fecha
    call imprimir
    jmp .finf
.ambas:
    call mostrar_fecha
    mov al, ' '
    call imprimir_char
    call mostrar_hora
    call nueva_linea
    jmp .finf
.solo_hora:
    call mostrar_hora
    call nueva_linea
    jmp .finf
.solo_fecha:
    call mostrar_fecha
    call nueva_linea
.finf:
    popa
    ret

mostrar_fecha:
    pusha
    mov ah, 0x04
    int 0x1A        ; DH=dia BCD, DL=mes BCD, CX=ano (CH siglo, CL ano)
    ; dia
    mov al, dh
    call imprimir_bcd
    mov al, '/'
    call imprimir_char
    mov al, dl
    call imprimir_bcd
    mov al, '/'
    call imprimir_char
    mov al, ch
    call imprimir_bcd
    mov al, cl
    call imprimir_bcd
    popa
    ret

mostrar_hora:
    pusha
    mov ah, 0x02
    int 0x1A        ; CH=hora BCD, CL=min BCD, DH=seg BCD
    mov al, ch
    call imprimir_bcd
    mov al, ':'
    call imprimir_char
    mov al, cl
    call imprimir_bcd
    mov al, ':'
    call imprimir_char
    mov al, dh
    call imprimir_bcd
    popa
    ret

imprimir_bcd:
    pusha
    mov bl, al
    mov al, bl
    shr al, 4
    add al, '0'
    call imprimir_char
    mov al, bl
    and al, 0x0F
    add al, '0'
    call imprimir_char
    popa
    ret

h_quien:
    pusha
    mov si, msg_quien
    call imprimir
    popa
    ret

h_info:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .corta2
    mov di, opt_todo
    call cadenas_iguales
    cmp ax, 0
    je .todo2
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ayi
    mov si, msg_desc_info
    call imprimir
    jmp .fini
.ayi:
    mov si, msg_desc_info
    call imprimir
    jmp .fini
.corta2:
    mov si, msg_info
    call imprimir
    int 0x12        ; AX = KB base
    push ax
    mov si, msg_mem
    call imprimir
    pop ax
    call imprimir_numero
    call nueva_linea
    jmp .fini
.todo2:
    mov si, msg_info_todo
    call imprimir
    int 0x12
    push ax
    mov si, msg_mem
    call imprimir
    pop ax
    call imprimir_numero
    call nueva_linea
.fini:
    popa
    ret

h_version:
    pusha
    mov si, msg_version
    call imprimir
    popa
    ret

h_reiniciar:
    pusha
    mov si, msg_reiniciar
    call imprimir
    ; esperar un poco
    mov cx, 0x0F
    mov dx, 0x4240
    mov ah, 0x86
    int 0x15
    jmp 0xFFFF:0x0000
    popa
    ret

h_apagar:
    pusha
    mov si, msg_apagar
    call imprimir
.cli_hlt:
    cli
    hlt
    jmp .cli_hlt
    popa
    ret

; ---------- dividir args en 2 nombres: buffer_args -> buffer_nombre + buffer_nombre2 ----------
; -> AX 0 ok, 1 falta primero, 2 falta segundo
dividir_dos_nombres:
    push si
    push di
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .dd_falta1
    mov di, buffer_nombre
    call copiar_palabra
    ; avanzar al segundo
.av_dd:
    mov al, [si]
    cmp al, 0
    je .dd_falta2
    cmp al, ' '
    je .dd_sp
    inc si
    jmp .av_dd
.dd_sp:
    call saltar_espacios
    cmp byte [si], 0
    je .dd_falta2
    mov di, buffer_nombre2
    call copiar_palabra
    mov ax, 0
    jmp .dd_fin
.dd_falta1:
    mov ax, 1
    jmp .dd_fin
.dd_falta2:
    mov ax, 2
.dd_fin:
    pop di
    pop si
    ret

tmp_cp_origen dw 0
tmp_cp_dest dw 0
tmp_cp_tam dw 0
; ---------- copiar archivo (mismo dir o a subcarpeta existente) ----------
h_copiar:
    pusha
    call dividir_dos_nombres
    cmp ax, 0
    jne .cp_falta
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .cp_noex
    mov [cs:tmp_cp_origen], ax
    push ax
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .cp_noarch
    mov bx, [cs:tmp_cp_origen]
    ; destino es carpeta existente?
    mov si, buffer_nombre2
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .cp_nuevo
    push ax
    call fs_leer_tipo
    pop dx
    cmp al, 1
    jne .cp_existe
    ; copiar dentro de carpeta DX con mismo nombre origen
    push dx
    mov si, buffer_nombre
    mov ax, dx
    call fs_buscar
    pop dx
    cmp ax, 0xFFFF
    jne .cp_existe
    mov si, buffer_nombre
    mov ax, dx
    mov bl, 2
    call fs_crear
    cmp ax, 0xFFFE
    je .cp_existe
    cmp ax, 0xFFFF
    je .cp_lleno
    mov [cs:tmp_cp_dest], ax
    jmp .cp_doit
.cp_nuevo:
    mov si, buffer_nombre2
    call validar_nombre
    cmp ax, 0
    jne .cp_malo
    mov si, buffer_nombre2
    mov ax, [dir_actual]
    mov bl, 2
    call fs_crear
    cmp ax, 0xFFFE
    je .cp_existe
    cmp ax, 0xFFFF
    je .cp_lleno
    mov [cs:tmp_cp_dest], ax
.cp_doit:
    mov ax, [cs:tmp_cp_origen]
    call fs_leer_tam
    mov [cs:tmp_cp_tam], ax
    mov cx, ax
    or cx, cx
    jz .cp_tam_ok
    xor si, si
.cp_byte2:
    cmp si, cx
    jae .cp_tam_ok
    push si
    push cx
    mov ax, [cs:tmp_cp_origen]
    call seg_datos
    mov fs, ax
    mov al, [fs:si]
    push ax
    mov ax, [cs:tmp_cp_dest]
    call seg_datos
    mov es, ax
    pop ax
    pop cx
    pop si
    push si
    push cx
    push ax
    mov ax, [cs:tmp_cp_dest]
    call seg_datos
    mov es, ax
    pop ax
    pop cx
    pop si
    mov [es:si], al
    inc si
    jmp .cp_byte2
.cp_tam_ok:
    mov ax, [cs:tmp_cp_dest]
    mov bx, 36
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    mov ax, [cs:tmp_cp_tam]
    mov [es:di+34], ax
    mov si, msg_copiado
    call imprimir_ok
    call autosave
    jmp .cp_fin
.cp_falta:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .cp_fin
.cp_noex:
    mov si, msg_no_existe
    call imprimir_error
    jmp .cp_fin
.cp_noarch:
    mov si, msg_no_es_archivo
    call imprimir_error
    jmp .cp_fin
.cp_existe:
    mov si, msg_ya_existe
    call imprimir_error
    jmp .cp_fin
.cp_lleno:
    mov si, msg_sin_espacio
    call imprimir_error
    jmp .cp_fin
.cp_malo:
    mov si, msg_nombre_malo
    call imprimir_error
.cp_fin:
    popa
    ret

buffer_tmp_nombre times 32 db 0
; copiar nombre AX -> buffer_tmp_nombre (helper copiar)
fs_copiar_nombre_tmp:
    pusha
    mov di, buffer_tmp_nombre
    call fs_copiar_nombre
    popa
    ret

; ---------- mover / renombrar ----------
h_renombrar:
    pusha
    call dividir_dos_nombres
    cmp ax, 0
    jne .rn_falta
    mov si, buffer_nombre2
    call validar_nombre
    cmp ax, 0
    jne .rn_malo
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .rn_noex
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .rn_noarch
    mov si, buffer_nombre2
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    jne .rn_existe
    ; renombrar: copiar nombre nuevo sobre entrada BX
    mov ax, bx
    mov bx, 36
    push bx
    mov bx, ax
    mov ax, bx
    pop bx
    mul bx
    mov di, ax
    add di, 2
    mov ax, 0x1000
    mov es, ax
    mov si, buffer_nombre2
.rn_cop:
    lodsb
    mov [es:di], al
    inc di
    or al, al
    jnz .rn_cop
    mov si, msg_renombrado
    call imprimir_ok
    call autosave
    jmp .rn_fin
.rn_falta:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .rn_fin
.rn_noex:
    mov si, msg_no_existe
    call imprimir_error
    jmp .rn_fin
.rn_noarch:
    mov si, msg_no_es_archivo
    call imprimir_error
    jmp .rn_fin
.rn_existe:
    mov si, msg_ya_existe
    call imprimir_error
    jmp .rn_fin
.rn_malo:
    mov si, msg_nombre_malo
    call imprimir_error
.rn_fin:
    popa
    ret

h_mover:
    pusha
    call dividir_dos_nombres
    cmp ax, 0
    jne .mv_falta
    ; origen debe ser archivo
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .mv_noex
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .mv_noarch
    ; destino es carpeta existente?
    mov si, buffer_nombre2
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .mv_rename
    ; existe: debe ser carpeta
    push ax
    call fs_leer_tipo
    pop dx
    cmp al, 1
    jne .mv_existe
    ; mover BX a carpeta DX (comprobar choque)
    push bx
    push dx
    mov si, buffer_nombre
    mov ax, dx
    call fs_buscar
    pop dx
    pop bx
    cmp ax, 0xFFFF
    jne .mv_existe
    ; cambiar padre de BX a DL
    mov ax, bx
    mov bx, 36
    push bx
    mov bx, ax
    mov ax, bx
    pop bx
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    mov al, dl
    mov [es:di+1], al
    mov si, msg_movido
    call imprimir_ok
    call autosave
    jmp .mv_fin
.mv_rename:
    ; renombrar en mismo dir (reusar logica renombrar)
    mov si, buffer_nombre2
    call validar_nombre
    cmp ax, 0
    jne .mv_malo
    mov ax, bx
    mov bx, 36
    push bx
    mov bx, ax
    mov ax, bx
    pop bx
    mul bx
    mov di, ax
    add di, 2
    mov ax, 0x1000
    mov es, ax
    mov si, buffer_nombre2
.mv_cop:
    lodsb
    mov [es:di], al
    inc di
    or al, al
    jnz .mv_cop
    mov si, msg_movido
    call imprimir_ok
    call autosave
    jmp .mv_fin
.mv_falta:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .mv_fin
.mv_noex:
    mov si, msg_no_existe
    call imprimir_error
    jmp .mv_fin
.mv_noarch:
    mov si, msg_no_es_archivo
    call imprimir_error
    jmp .mv_fin
.mv_existe:
    mov si, msg_ya_existe
    call imprimir_error
    jmp .mv_fin
.mv_malo:
    mov si, msg_nombre_malo
    call imprimir_error
.mv_fin:
    popa
    ret

; ---------- buscar texto ----------
h_buscar_texto:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .bs_falta
    mov di, buffer_nombre
    call copiar_palabra
    ; avanzar al texto
.av_bs:
    mov al, [si]
    cmp al, 0
    je .bs_falta2
    cmp al, ' '
    je .bs_sp
    inc si
    jmp .av_bs
.bs_sp:
    call saltar_espacios
    cmp byte [si], 0
    je .bs_falta2
    mov di, buffer_texto
    call copiar_cadena
    ; buscar archivo
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .bs_noex
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .bs_noarch
    mov [cs:.bs_file], bx
    ; cargar tam y buscar substring buffer_texto en datos
    mov si, buffer_texto
    call longitud          ; CX = len patron
    push cx
    mov ax, bx
    call fs_leer_tam
    mov dx, ax             ; DX = tam archivo
    pop cx                 ; CX = len patron
    cmp cx, 0
    je .bs_falta2
    cmp cx, dx
    ja .bs_no
    ; bucle pos 0..tam-pat
    xor bx, bx             ; BX = pos
.bs_pos:
    mov ax, dx
    sub ax, cx
    cmp bx, ax
    ja .bs_no
    push bx
    push cx
    push dx
    ; comparar patron en buffer_texto con datos en pos BX
    xor si, si             ; SI = offset patron
.bs_cmp:
    cmp si, cx
    jae .bs_si
    push bx
    push cx
    push si
    mov ax, [cs:.bs_file]
    call seg_datos
    mov es, ax
    pop si
    pop cx
    pop bx
    push bx
    push cx
    push si
    mov al, [es:bx+si]
    mov di, buffer_texto
    add di, si
    mov ah, [di]
    cmp al, ah
    pop si
    pop cx
    pop bx
    jne .bs_next
    inc si
    jmp .bs_cmp
.bs_si:
    pop dx
    pop cx
    pop bx
    mov ax, bx
    mov si, msg_encontrado
    call imprimir_ok
    call imprimir_numero
    call nueva_linea
    jmp .bs_fin
.bs_next:
    pop dx
    pop cx
    pop bx
    inc bx
    jmp .bs_pos
.bs_no:
    mov si, msg_no_encontrado
    call imprimir_error
    jmp .bs_fin
.bs_falta:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .bs_fin
.bs_falta2:
    mov si, msg_falta_texto
    call imprimir_error
    jmp .bs_fin
.bs_noex:
    mov si, msg_no_existe
    call imprimir_error
    jmp .bs_fin
.bs_noarch:
    mov si, msg_no_es_archivo
    call imprimir_error
.bs_fin:
    popa
    ret
.bs_file dw 0

; ---------- editar archivo ----------
h_editar:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .ed_falta
    mov di, buffer_nombre
    call copiar_palabra
    mov si, buffer_nombre
    mov ax, [dir_actual]
    call fs_buscar
    cmp ax, 0xFFFF
    je .ed_crear
    mov bx, ax
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .ed_noarch
    jmp .ed_abrir
.ed_crear:
    mov si, buffer_nombre
    call validar_nombre
    cmp ax, 0
    jne .ed_malo
    mov si, buffer_nombre
    mov ax, [dir_actual]
    mov bl, 2
    call fs_crear
    cmp ax, 0xFFFF
    je .ed_lleno
    cmp ax, 0xFFFE
    je .ed_existe
    mov bx, ax
.ed_abrir:
    mov [cs:.ed_idx], bx
    ; cargar contenido actual a buffer_texto y mostrarlo
    mov ax, bx
    call fs_leer_tam
    mov [cs:.ed_len], ax
    mov ax, bx
    call seg_datos
    mov es, ax
    xor si, si
    mov di, buffer_texto
    mov cx, [cs:.ed_len]
.ed_cop_ini:
    cmp si, cx
    jae .ed_ini_fin
    mov al, [es:si]
    mov [di], al
    inc si
    inc di
    jmp .ed_cop_ini
.ed_ini_fin:
    mov byte [di], 0
    mov si, msg_edit_ini
    call imprimir
    mov ax, [cs:.ed_len]
    cmp ax, 0
    jne .ed_mostrar
    mov si, msg_edit_vacio
    call imprimir
    jmp .ed_loop
.ed_mostrar:
    mov si, buffer_texto
    call imprimir_blanco
    call nueva_linea
.ed_loop:
    mov si, msg_edit_prompt
    call imprimir
    mov byte [cs:en_editor], 1
    call leer_linea_editor
    mov byte [cs:en_editor], 0
    mov si, buffer_entrada
    mov di, cmd_guardar_dos
    call cadenas_iguales
    cmp ax, 0
    je .ed_guardar
    mov si, buffer_entrada
    mov di, cmd_salir_dos
    call cadenas_iguales
    cmp ax, 0
    je .ed_salir
    ; linea normal: agregar a buffer_texto con CRLF si cabe
    mov si, buffer_entrada
    call longitud
    mov dx, cx             ; DX=len linea
    mov ax, [cs:.ed_len]
    mov bx, ax
    cmp bx, 0
    je .ed_sin_crlf
    add ax, 2
.ed_sin_crlf:
    add ax, dx
    cmp ax, 512
    ja .ed_largo
    ; agregar CRLF si no vacio
    mov di, buffer_texto
    add di, bx
    cmp bx, 0
    je .ed_cop_lin
    mov byte [di], 13
    inc di
    mov byte [di], 10
    inc di
    add bx, 2
.ed_cop_lin:
    mov si, buffer_entrada
.ed_cop2:
    lodsb
    mov [di], al
    inc di
    or al, al
    jnz .ed_cop2
    dec di                ; quitar 0, calcular nueva len
    mov ax, di
    sub ax, buffer_texto
    mov [cs:.ed_len], ax
    jmp .ed_loop
.ed_largo:
    mov si, msg_texto_largo
    call imprimir_error
    jmp .ed_loop
.ed_guardar:
    ; escribir buffer_texto[0..ed_len] a archivo
    mov bx, [cs:.ed_idx]
    mov cx, [cs:.ed_len]
    push bx
    push cx
    mov ax, bx
    call seg_datos
    mov es, ax
    xor di, di
    mov si, buffer_texto
    cmp cx, 0
    je .ed_tam0
.ed_w:
    lodsb
    mov [es:di], al
    inc di
    loop .ed_w
.ed_tam0:
    pop cx
    pop bx
    push bx
    push cx
    mov ax, bx
    mov bx, 36
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    pop cx
    pop bx
    mov ax, cx
    mov [es:di+34], ax
    mov si, msg_edit_guardado
    call imprimir_ok
    call autosave
    jmp .ed_fin
.ed_salir:
    mov si, msg_edit_salir
    call imprimir_aviso
    jmp .ed_fin
.ed_falta:
    mov si, msg_falta_nombre
    call imprimir_error
    jmp .ed_fin
.ed_noarch:
    mov si, msg_no_es_archivo
    call imprimir_error
    jmp .ed_fin
.ed_malo:
    mov si, msg_nombre_malo
    call imprimir_error
    jmp .ed_fin
.ed_lleno:
    mov si, msg_sin_espacio
    call imprimir_error
    jmp .ed_fin
.ed_existe:
    mov si, msg_ya_existe
    call imprimir_error
.ed_fin:
    popa
    ret
.ed_idx dw 0
.ed_len dw 0

; leer linea para editor (sin historial, sin guardar en hist)
leer_linea_editor:
    pusha
    mov di, buffer_entrada
    xor cx, cx
.le_rep:
    mov ah, 0x00
    int 0x16
    cmp al, 13
    je .le_enter
    cmp al, 8
    je .le_back
    cmp cx, 120
    jae .le_rep
    push ax
    mov ah, 0x0E
    mov bh, 0
    mov bl, 7
    int 0x10
    pop ax
    push ax
    call serial_putc
    pop ax
    mov [di], al
    inc di
    inc cx
    jmp .le_rep
.le_back:
    cmp cx, 0
    je .le_rep
    dec di
    dec cx
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
    jmp .le_rep
.le_enter:
    mov byte [di], 0
    mov si, msg_crlf
    call imprimir
    popa
    ret

; ---------- espacio disco / guardar ----------
tmp_esp_ocup dw 0
tmp_esp_lib dw 0
tmp_esp_bytes dw 0
tmp_esp_idx dw 0
h_espacio:
    pusha
    call disco_tiene_lba
    cmp ax, 1
    jne .esp_nodisco
    mov si, msg_espacio_tit
    call imprimir
    mov word [cs:tmp_esp_ocup], 0
    mov word [cs:tmp_esp_lib], 0
    mov word [cs:tmp_esp_bytes], 0
    mov word [cs:tmp_esp_idx], 0
.esp_rep:
    mov ax, [cs:tmp_esp_idx]
    cmp ax, 64
    jae .esp_fin
    call fs_leer_tipo
    or al, al
    jz .esp_libre
    inc word [cs:tmp_esp_ocup]
    cmp al, 2
    jne .esp_next
    mov ax, [cs:tmp_esp_idx]
    call fs_leer_tam
    add [cs:tmp_esp_bytes], ax
    jmp .esp_next
.esp_libre:
    inc word [cs:tmp_esp_lib]
.esp_next:
    inc word [cs:tmp_esp_idx]
    jmp .esp_rep
.esp_fin:
    mov si, msg_espacio_usados
    call imprimir
    mov ax, [cs:tmp_esp_ocup]
    call imprimir_numero
    call nueva_linea
    mov si, msg_espacio_libres
    call imprimir
    mov ax, [cs:tmp_esp_lib]
    call imprimir_numero
    call nueva_linea
    mov si, msg_espacio_bytes
    call imprimir
    mov ax, [cs:tmp_esp_bytes]
    call imprimir_numero
    mov si, msg_espacio_max
    call imprimir
    popa
    ret
.esp_nodisco:
    mov si, msg_espacio_nodisco
    call imprimir_aviso
    popa
    ret

h_guardar_cmd:
    pusha
    call guardar_disco
    cmp ax, 0
    je .g_ok
    cmp ax, 1
    je .g_nod
    mov si, msg_guardado_mal
    call imprimir_error
    jmp .g_fin
.g_ok:
    mov si, msg_guardado_ok
    call imprimir_ok
    jmp .g_fin
.g_nod:
    mov si, msg_guardado_nodisco
    call imprimir_aviso
.g_fin:
    popa
    ret

; relleno para que el tamano sea visible (nasm calcula)
times 1024 db 0
