; FacilOS v1.2 - Kernel + Shell en espanol facil
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
buffer_ruta2 times 128 db 0
buffer_comp times 32 db 0
pila_ruta times 64 dw 0

msg_banner db 13,10
    db '========================================',13,10
    db '  FacilOS v1.3 - Sistema facil en espanol',13,10
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
    db '  escribir_archivo <nombre> [--agregar] <texto>',13,10
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
    db '  teclado [es|us] ............... espanol o americano',13,10
    db '  resolucion [ANCHOxALTO|80x25] . cambia (Ej: 1920x1080)',13,10
    db '  prueba_grafica ............. test visual (modo grafico)',13,10
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
msg_desc_escribir db 'escribir_archivo <nombre> [--agregar] <texto> : escribe texto.',13,10
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
msg_version db 'FacilOS version 1.3 - Shell facil en espanol.',13,10,0
msg_info db 'FacilOS v1.3 - Hecho desde cero en ensamblador.',13,10,0
msg_info_todo db 'FacilOS v1.3',13,10
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
cmd_resolucion db 'resolucion',0
msg_desc_resolucion db 'resolucion [ANCHOxALTO|80x25] : lista o cambia (Ej: resolucion 1920x1080).',13,10,0
tmp_res_w dw 0
tmp_res_h dw 0
cmd_teclado db 'teclado',0
cmd_prueba db 'prueba_grafica',0
msg_desc_prueba db 'prueba_grafica : test visual (requiere modo grafico).',13,10,0
opt_es db 'es',0
opt_espanol db 'espanol',0
opt_us db 'us',0
opt_americano db 'americano',0
msg_tec_es db 'Teclado espanol (ES) activado.',13,10,0
msg_tec_us db 'Teclado americano (US) activado.',13,10,0
msg_tec_ahora_es db 'Teclado actual: espanol (ES). Usa: teclado es|us',13,10,0
msg_tec_ahora_us db 'Teclado actual: americano (US). Usa: teclado es|us',13,10,0
msg_desc_teclado db 'teclado [es|us] : muestra o cambia (espanol/americano).',13,10,0
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
opt_punto db '.',0
msg_no_borrar_raiz db 'No se puede borrar la raiz (/).',13,10,0
msg_ruta_mala db 'Ruta no valida o no existe.',13,10,0

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

; ---------- imprimir cadena SI (gris; respeta VESA) ----------
imprimir:
    pusha
.imp_rep:
    lodsb
    or al, al
    jz .imp_fin
    push ax
    push bx
    call serial_putc
    pop bx
    pop ax
    cmp byte [cs:video_graf], 0
    je .imp_txt
    mov bl, 0x07
    call video_putc
    jmp .imp_rep
.imp_txt:
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp .imp_rep
.imp_fin:
    popa
    ret

imprimir_char:
    pusha
    push ax
    push bx
    call serial_putc
    pop bx
    pop ax
    cmp byte [cs:video_graf], 0
    je .imc_txt
    mov bl, 0x07
    call video_putc
    jmp .imc_fin
.imc_txt:
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
.imc_fin:
    popa
    ret

; ---------- colores v1.3 (attrs CGA; ver video_color_prep) ----------
COLOR_NEGRO equ 0x00
COLOR_AZUL equ 0x01
COLOR_VERDE equ 0x02
COLOR_CYAN equ 0x03
COLOR_ROJO equ 0x04
COLOR_MAGENTA equ 0x05
COLOR_MARRON equ 0x06
COLOR_GRIS equ 0x07
COLOR_GRIS_OSCURO equ 0x08
COLOR_AZUL_CLARO equ 0x09
COLOR_VERDE_CLARO equ 0x0A
COLOR_CYAN_CLARO equ 0x0B
COLOR_ROJO_CLARO equ 0x0C
COLOR_MAGENTA_CLARO equ 0x0D
COLOR_AMARILLO equ 0x0E
COLOR_BLANCO equ 0x0F
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
    cmp byte [cs:video_graf], 0
    je .pop_txt
    push ax
    push bx
    call serial_putc
    pop bx
    pop ax
    call video_putc
    jmp .pop_rep
.pop_txt:
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
    call eco_backspace
    dec cx
    jmp .bl_rep
.bl_fin:
    pop cx
    pop ax
    ret

; ---------- teclado espanol v1.2 (traduccion scancode -> ES, compatible US) ----------
; 1=espanol (defecto), 0=americano (BIOS directo). Dual: - _ / . , : accesibles.
teclado_es db 1
; traducir_es: entrada AX de INT16 (AH=scan AL=ascii) -> AL=char ES (AH intacto).
; Preserva BX,CX,DX,SI,DI,ES. Lee shift via INT16 AH=02.
traducir_es:
    pusha
    push es
    push ds
    pop es              ; ES=DS para tabla CS? usamos CS: directamente
    cmp byte [cs:teclado_es], 0
    je .tr_pasar
    cmp al, 0
    je .tr_pasar        ; extendida (flechas): no tocar
    push ax             ; guarda scan/ascii
    mov ah, 0x02
    int 0x16            ; AL=flags shift
    mov bl, al
    pop ax              ; AX=scan/ascii original
    ; buscar scan en tabla
    mov si, tabla_es
.tr_buscar:
    mov cl, [cs:si]
    or cl, cl
    jz .tr_pasar        ; fin tabla (0)
    cmp cl, ah
    je .tr_hallado
    add si, 3
    jmp .tr_buscar
.tr_hallado:
    test bl, 0x03       ; shift?
    jz .tr_base
    mov al, [cs:si+2]
    jmp .tr_mayus
.tr_base:
    mov al, [cs:si+1]
.tr_mayus:
    ; CapsLock (bit 0x40) invierte letras a-z/Ã±
    test bl, 0x40
    jz .tr_fin
    cmp al, 'a'
    jb .tr_fin
    cmp al, 'z'
    jbe .tr_inv
    cmp al, 0xA4
    jne .tr_fin
    mov al, 0xA5
    jmp .tr_fin
.tr_inv:
    sub al, 32
    jmp .tr_fin
.tr_pasar:
    ; AL intacto
.tr_fin:
    mov [cs:.tr_al], al
    pop es
    popa
    mov al, [cs:.tr_al]
    ret
.tr_al db 0
; tabla (scan, base, shift), fin 0. Letras minusculas, ES con enie.
tabla_es:
    db 0x10,'q','Q', 0x11,'w','W', 0x12,'e','E', 0x13,'r','R', 0x14,'t','T'
    db 0x15,'y','Y', 0x16,'u','U', 0x17,'i','I', 0x18,'o','O', 0x19,'p','P'
    db 0x1E,'a','A', 0x1F,'s','S', 0x20,'d','D', 0x21,'f','F', 0x22,'g','G'
    db 0x23,'h','H', 0x24,'j','J', 0x25,'k','K', 0x26,'l','L', 0x27,0xA4,0xA5
    db 0x2C,'z','Z', 0x2D,'x','X', 0x2E,'c','C', 0x2F,'v','V', 0x30,'b','B'
    db 0x31,'n','N', 0x32,'m','M'
    db 0x02,'1','!', 0x03,'2','"', 0x04,'3',250, 0x05,'4','$', 0x06,'5','%'
    db 0x07,'6','&', 0x08,'7','/', 0x09,'8','(', 0x0A,'9',')', 0x0B,'0','='
    db 0x29,167,166, 0x0C,'-','_', 0x0D,173,168, 0x1B,'+','*'
    db 0x33,',',';', 0x34,'.',':', 0x35,'/','_'
    db 0
; 250='Â·' 167='Âº' 166='Âª' 173='Â¡' 168='Â¿' 164='Ã±' 165='Ã‘' (CP437)
; 0x35 dual ('/' directo para rutas y tests; Shift+7 tambien da '/')
; AltGr ([ ] { } @ # | ~ \ â‚¬) no disponible en v1.2 (sin AltGr por INT16).

; ---------- leer linea a buffer_entrada (con historial flechas) ----------
leer_linea:
    pusha
    mov di, buffer_entrada
    xor cx, cx          ; longitud
.lrep:
    mov ah, 0x00
    int 0x16            ; AL = tecla, AH = scan
    call traducir_es
    cmp al, 13          ; Enter
    je .enter
    cmp al, 8           ; Backspace
    je .back
    cmp al, 0           ; tecla extendida (flechas)
    je .extendida
    cmp cx, 120
    jae .lrep
    call eco_putc
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
    call eco_putc
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
    call eco_backspace
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
    mov si, buffer_comando
    mov di, cmd_resolucion
    call cadenas_iguales
    cmp ax, 0
    je .c_resolucion
    mov si, buffer_comando
    mov di, cmd_teclado
    call cadenas_iguales
    cmp ax, 0
    je .c_teclado
    mov si, buffer_comando
    mov di, cmd_prueba
    call cadenas_iguales
    cmp ax, 0
    je .c_prueba

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
.c_resolucion: call h_resolucion
    jmp .finp2
.c_teclado: call h_teclado
    jmp .finp2
.c_prueba: call h_prueba_grafica
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
; Tabla v1.2 en 0x1000:0, 64 entradas x 42 bytes:
; 0:tipo 1:padre 2-33:nombre(32) 34-35:tam 36-38:cre(D,M,A) 39-41:mod(D,M,A)
; Datos en 0x1200 + i*0x20 (512 bytes cada uno)

fs_inicializar:
    pusha
    push es
    mov ax, 0x1000
    mov es, ax
    xor di, di
    mov cx, 64*42
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
    mov byte [es:36], 0
    mov byte [es:37], 0
    mov byte [es:38], 0
    mov byte [es:39], 0
    mov byte [es:40], 0
    mov byte [es:41], 0
    pop es
    mov word [dir_actual], 0
    popa
    push ax
    xor ax, ax
    call estampar_nueva
    pop ax
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
    mov bx, 42
    mul bx              ; AX = indice*42 (DX=0)
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
    mov bx, 42
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
    mov bx, 42
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
    mov bx, 42
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
    mov bx, 42
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
    mov bx, 42
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
    mov bx, 42
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
    mov bx, 42
    mul bx
    mov di, ax
    mov word [es:di+34], 0
    mov byte [es:di+36], 0
    mov byte [es:di+37], 0
    mov byte [es:di+38], 0
    mov byte [es:di+39], 0
    mov byte [es:di+40], 0
    mov byte [es:di+41], 0
    pop ax              ; indice
    push ax
    call estampar_nueva
    pop ax
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
    mov bx, 42
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
    mov bx, 42
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
tmp_touch_idx dw 0
tmp_path_padre dw 0
tmp_ren_padre dw 0
tmp_mv_opadre dw 0
; SI=token -> AX 1 si contiene '/', 0 si no. Preserva todo menos AX.
tiene_barra:
    push si
.tb_rep:
    mov al, [si]
    or al, al
    jz .tb_no
    cmp al, '/'
    je .tb_si
    inc si
    jmp .tb_rep
.tb_si:
    pop si
    mov ax, 1
    ret
.tb_no:
    pop si
    xor ax, ax
    ret
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
tmp_ver_dir dw 0
msg_ver_carpetas db 'CARPETAS:',13,10,0
msg_ver_archivos db 'ARCHIVOS:',13,10,0
msg_ver_unidad db ' B',0

; ================= RUTAS v1.2 =================
; resolver_dir SI=ruta -> AX=idx dir o 0xFFFF. Acepta /, ., .., abs, rel, //, trailing /.
; No escapa de /. Requiere que cada componente exista y sea carpeta (salvo . y ..).
resolver_dir:
    pusha
    push es
    call saltar_espacios
    cmp byte [si], 0
    je .rd_fail
    mov bx, [dir_actual]    ; BX = actual
    cmp byte [si], '/'
    jne .rd_loop
    xor bx, bx              ; absoluta desde raiz
    inc si
.rd_loop:
    ; saltar barras repetidas
.rd_slash:
    cmp byte [si], '/'
    jne .rd_comp
    inc si
    jmp .rd_slash
.rd_comp:
    cmp byte [si], 0
    je .rd_ok               ; fin (p.ej. "/" o "a/" )
    ; extraer componente hasta '/' o 0 a buffer_comp
    mov di, buffer_comp
    xor cx, cx
.rd_cop:
    mov al, [si]
    cmp al, 0
    je .rd_cop_fin
    cmp al, '/'
    je .rd_cop_fin
    cmp cx, 31
    jae .rd_toolong
    mov [di], al
    inc di
    inc si
    inc cx
    jmp .rd_cop
.rd_cop_fin:
    mov byte [di], 0
    cmp cx, 0
    je .rd_loop             ; componente vacio (//), seguir
    ; comparar con "." y ".."
    push si
    push bx
    mov si, buffer_comp
    mov di, opt_punto
    call cadenas_iguales
    pop bx
    pop si
    cmp ax, 0
    je .rd_loop             ; "." = seguir
    push si
    push bx
    mov si, buffer_comp
    mov di, opt_padre
    call cadenas_iguales
    pop bx
    pop si
    cmp ax, 0
    je .rd_dotdot
    ; buscar hijo carpeta (BX=padre)
    push si
    push bx
    mov si, buffer_comp
    mov ax, bx
    call fs_buscar
    pop bx
    pop si
    cmp ax, 0xFFFF
    je .rd_fail
    push ax                 ; guarda idx hijo
    call fs_leer_tipo       ; AX=tipo (preserva pila)
    mov cx, ax
    pop ax                  ; AX=idx hijo
    cmp cl, 1
    jne .rd_fail
    mov bx, ax              ; BX=idx hijo
    jmp .rd_loop
.rd_dotdot:
    cmp bx, 0
    je .rd_loop             ; en raiz, .. se queda
    push si
    mov ax, bx
    call fs_leer_padre
    mov bx, ax
    pop si
    jmp .rd_loop
.rd_toolong:
    jmp .rd_fail
.rd_ok:
    mov [cs:.rd_idx], bx
    pop es
    popa
    mov ax, [cs:.rd_idx]
    ret
.rd_fail:
    pop es
    popa
    mov ax, 0xFFFF
    ret
.rd_idx dw 0

; descomponer_ruta SI=ruta -> AX=padre idx o FFFF, base en buffer_nombre.
; La base NO se valida aqui (el llamador rechaza ./.. y valida caracteres).
descomponer_ruta:
    pusha
    push es
    call saltar_espacios
    cmp byte [si], 0
    je .dc_fail
    ; copiar ruta a buffer_ruta2 y buscar ultima '/'
    mov di, buffer_ruta2
    call copiar_cadena
    ; localizar ultima '/'
    mov si, buffer_ruta2
    mov bx, -1              ; pos ultima barra (-1 = ninguna)
    xor cx, cx              ; pos actual
.dc_scan:
    mov al, [si]
    or al, al
    jz .dc_split
    cmp al, '/'
    jne .dc_next
    mov bx, cx
.dc_next:
    inc si
    inc cx
    jmp .dc_scan
.dc_split:
    cmp bx, -1
    je .dc_solo
    ; hay barra en pos BX: dir = primeros BX chars, base = resto
    mov si, buffer_ruta2
    add si, bx
    inc si                  ; SI = base (tras barra)
    mov di, buffer_nombre
    call copiar_cadena      ; base (puede ser "" si trailing / -> error luego)
    ; terminar dir en BX con 0 temporal
    mov si, buffer_ruta2
    add si, bx
    mov byte [si], 0
    mov si, buffer_ruta2
    cmp byte [si], 0
    jne .dc_resdir
    ; dir vacio antes de barra => ruta como "/base" (absoluta, padre=raiz)
    mov word [cs:.dc_padre], 0
    jmp .dc_ok
.dc_resdir:
    call resolver_dir       ; SI=buffer_ruta2 (dir)
    cmp ax, 0xFFFF
    je .dc_fail
    mov [cs:.dc_padre], ax
    jmp .dc_ok
.dc_solo:
    ; sin barra: padre = dir_actual, base = ruta completa
    mov si, buffer_ruta2
    mov di, buffer_nombre
    call copiar_cadena
    mov ax, [dir_actual]
    mov [cs:.dc_padre], ax
.dc_ok:
    pop es
    popa
    mov ax, [cs:.dc_padre]
    ret
.dc_fail:
    pop es
    popa
    mov ax, 0xFFFF
    ret
.dc_padre dw 0

; ================= DISCO PERSISTENTE v1.2 =================
; Disco de datos 0x80, 10MB (20480 sectores).
; v1.2: LBA0=cabecera(magic FACILOS2, sum_lo, sum_hi, version=2), LBA1-6=tabla(64x42),
;       LBA7-70=datos(64x512). Total sistema 71 sectores.
; v1.1 (migracion, solo lectura): magic FACILOS1, tabla 64x36 LBA1-5, datos LBA6-69.
; NUNCA se formatea/escribe automatico un disco desconocido o corrupto.
DISCO_DL equ 0x80
DISCO_LBA_CAB equ 0
DISCO_LBA_TAB equ 1
DISCO_NTAB equ 6
DISCO_LBA_DAT equ 7
DISCO_NDAT equ 64
DISCO_V11_NTAB equ 5
DISCO_V11_LBA_DAT equ 6
disco_ok db 0            ; 1 si hay disco valido (v1.2 o v1.1 migrado en RAM)
disco_migrar db 0        ; 1 si RAM viene de v1.1 (guardar escribira v1.2)
disco_sucio db 0
disk_packet times 16 db 0
msg_disco_ok db 'Disco de datos listo. Archivos recuperados.',13,10,0
msg_disco_mig db 'Disco v1.1 detectado. Se migrara a v1.2 al usar guardar.',13,10,0
msg_disco_nuevo db 'Disco de datos nuevo. Se guardara al usar guardar.',13,10,0
msg_disco_no db 'Sin disco de datos. Los cambios solo estan en memoria.',13,10,0
msg_disco_mal db 'Disco con error o desconocido. Memoria temporal, sin escribir.',13,10,0
msg_guardado_ok db 'Guardado en disco.',13,10,0
msg_guardado_mal db 'Error al guardar en disco.',13,10,0
msg_guardado_nodisco db 'Sin disco de datos. Nada que guardar.',13,10,0
msg_espacio_tit db 'Espacio en disco:',13,10,0
msg_espacio_total db '  Capacidad: 64 entradas, 32768 B en archivos (disco 10 MB)',13,10,0
msg_espacio_usados db '  Ocupados: ',0
msg_espacio_libres db '  Libres: ',0
msg_espacio_arch db '  Archivos: ',0
msg_espacio_dir db '  Carpetas: ',0
msg_espacio_bytes db '  Bytes en archivos: ',0
msg_espacio_max db ' / 32768',13,10,0
msg_espacio_nodisco db 'Sin disco: solo memoria (64 espacios).',13,10,0

tmp_sp_bios dw 0
; comprobar extensiones INT13 (DL=0x80) -> AX 1 si soporta LBA
; (resincroniza SP: algunos BIOS dejan basura en la pila)
disco_tiene_lba:
    push bx
    push cx
    push dx
    mov [cs:tmp_sp_bios], sp
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, DISCO_DL
    int 0x13
    mov sp, [cs:tmp_sp_bios]
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

; checksum32 de tabla(2688B en 0x1000:0) + datos(32768B) -> DX:AX
disco_checksum:
    pusha
    push es
    xor bx, bx          ; suma baja
    xor cx, cx          ; suma alta (acarreo simple)
    mov ax, 0x1000
    mov es, ax
    xor di, di
    mov dx, 2688
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

; validar_tabla en RAM (0x1000:0, 64x42) -> AX 0 ok, 1 corrupta
validar_tabla:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov ax, 0x1000
    mov es, ax
    xor dx, dx          ; idx
.vt_rep:
    cmp dx, 64
    jae .vt_ok
    mov ax, dx
    push dx
    mov bx, 42
    mul bx
    mov si, ax
    pop dx
    mov al, [es:si]     ; tipo
    cmp al, 2
    ja .vt_mal
    or al, al
    jz .vt_next         ; libre: resto debe ser 0? Tolerar basura salvo tipo/padre? Exigir tam=0:
    mov bl, [es:si+1]   ; padre
    cmp bl, 64
    jae .vt_mal
    cmp dx, 0
    je .vt_raiz
    cmp al, 1
    je .vt_dir
    ; archivo: tam<=512, nombre valido, fechas validas
    mov cx, [es:si+34]
    cmp cx, 512
    ja .vt_mal
    jmp .vt_nom
.vt_dir:
    mov cx, [es:si+34]
    cmp cx, 0
    jne .vt_mal
.vt_nom:
    ; nombre: 1-31 chars, 0-term en 32B, sin '/' (salvo raiz), sin '\'
    push dx
    push si
    add si, 2
    xor cx, cx
.vt_cn:
    mov bl, [es:si]
    or bl, bl
    jz .vt_cn_fin
    inc cx
    cmp cx, 32
    jae .vt_mal2
    cmp bl, '/'
    je .vt_mal2
    cmp bl, '\'
    je .vt_mal2
    cmp bl, ' '
    je .vt_mal2
    inc si
    jmp .vt_cn
.vt_cn_fin:
    cmp cx, 0
    je .vt_mal2
    cmp cx, 32
    jae .vt_mal2
    pop si
    pop dx
    ; fechas: dia<=31 mes<=12 ano<=99
    mov al, [es:si+36]
    cmp al, 31
    ja .vt_mal
    mov al, [es:si+37]
    cmp al, 12
    ja .vt_mal
    mov al, [es:si+38]
    cmp al, 99
    ja .vt_mal
    mov al, [es:si+39]
    cmp al, 31
    ja .vt_mal
    mov al, [es:si+40]
    cmp al, 12
    ja .vt_mal
    mov al, [es:si+41]
    cmp al, 99
    ja .vt_mal
    jmp .vt_next
.vt_raiz:
    ; idx0: tipo=1 padre=0 nombre="/" tam=0
    cmp al, 1
    jne .vt_mal
    cmp bl, 0
    jne .vt_mal
    cmp byte [es:si+2], '/'
    jne .vt_mal
    cmp byte [es:si+3], 0
    jne .vt_mal
    mov cx, [es:si+34]
    cmp cx, 0
    jne .vt_mal
    jmp .vt_next
.vt_mal2:
    pop si
    pop dx
.vt_mal:
    mov ax, 1
    jmp .vt_fin
.vt_next:
    inc dx
    jmp .vt_rep
.vt_ok:
    xor ax, ax
.vt_fin:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

; comparar magic ES:SI (8B) con CS:DI -> AX 0 igual
comparar_magic8:
    push bx
    push cx
    mov cx, 8
.cm8:
    mov al, [es:si]
    mov bl, [cs:di]
    cmp al, bl
    jne .cm8_no
    inc si
    inc di
    loop .cm8
    xor ax, ax
    jmp .cm8_fin
.cm8_no:
    mov ax, 1
.cm8_fin:
    pop cx
    pop bx
    ret

tmp_sp_entry dw 0
; cargar disco al arrancar (tras fs_inicializar). Jamas escribe. -> disco_ok/migrar
cargar_disco:
    pusha
    mov [cs:tmp_sp_entry], sp
    mov byte [cs:disco_migrar], 0
    call disco_tiene_lba
    cmp ax, 1
    jne .sin_disco
    mov ax, 0x7000
    mov es, ax
    xor bx, bx
    mov eax, DISCO_LBA_CAB
    mov cx, 1
    mov dl, DISCO_DL
    call disco_leer
    cmp ax, 0
    jne .error_io
    ; probar v1.2
    mov ax, 0x7000
    mov es, ax
    mov si, 0
    mov di, .magic2
    call comparar_magic8
    cmp ax, 0
    je .es_v12
    ; probar v1.1 (migracion)
    mov ax, 0x7000
    mov es, ax
    mov si, 0
    mov di, .magic1
    call comparar_magic8
    cmp ax, 0
    je .es_v11
    jmp .mag_mal
.es_v12:
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov eax, DISCO_LBA_TAB
    mov cx, DISCO_NTAB
    mov dl, DISCO_DL
    call disco_leer
    cmp ax, 0
    jne .error_io
    mov dx, 0
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
    call disco_checksum ; DX:AX calculado
    push ax
    push dx
    mov ax, 0x7000
    mov es, ax
    mov bx, [es:8]
    mov cx, [es:10]
    mov si, [es:12]
    pop dx
    pop ax
    cmp ax, bx
    jne .checksum_mal
    cmp dx, cx
    jne .checksum_mal
    cmp si, 2
    jne .checksum_mal
    mov al, [es:14]
    cmp al, 1
    jbe .lay_ok
    mov al, 1
.lay_ok:
    mov [cs:teclado_es], al
    call validar_tabla
    cmp ax, 0
    jne .checksum_mal
.carga_ok12:
    mov byte [cs:disco_ok], 1
    mov si, msg_disco_ok
    call imprimir_ok
    mov sp, [cs:tmp_sp_entry]
    popa
    ret
.es_v11:
    ; migracion v1.1: tabla 36B LBA1-5 a scratch 0x6000, datos LBA6-69 (64s) a live
    mov ax, 0x6000
    mov es, ax
    xor bx, bx
    mov eax, 1
    mov cx, 5
    mov dl, DISCO_DL
    call disco_leer
    cmp ax, 0
    jne .error_io
    mov ax, 0x1200
    mov es, ax
    xor bx, bx
    mov eax, 6
    mov cx, 64
    mov dl, DISCO_DL
    call disco_leer
    cmp ax, 0
    jne .error_io
.ver11:
    ; checksum v1.1: scratch 2304B + datos live
    push es
    xor bx, bx
    xor cx, cx
    mov ax, 0x6000
    mov es, ax
    xor di, di
    mov si, 2304
.cs11_tab:
    mov al, [es:di]
    xor ah, ah
    add bx, ax
    adc cx, 0
    inc di
    dec si
    jnz .cs11_tab
    pop es
    xor dx, dx
.cs11_arc:
    cmp dx, 64
    jae .cs11_fin
    push dx
    push bx
    push cx
    mov ax, dx
    call seg_datos
    mov es, ax
    pop cx
    pop bx
    xor di, di
    mov si, 512
.cs11_dat:
    mov al, [es:di]
    xor ah, ah
    add bx, ax
    adc cx, 0
    inc di
    dec si
    jnz .cs11_dat
    pop dx
    inc dx
    jmp .cs11_arc
.cs11_fin:
    push bx
    push cx
    mov ax, 0x7000
    mov es, ax
    mov bx, [es:8]
    mov cx, [es:10]
    pop dx
    pop ax
    cmp ax, bx
    jne .checksum_mal
    cmp dx, cx
    jne .checksum_mal
    ; convertir scratch 36B -> live 42B (fechas 0 = desconocida)
    xor dx, dx
.conv11:
    cmp dx, 64
    jae .conv_fin
    push dx
    mov ax, dx
    mov bx, 36
    mul bx
    mov si, ax
    pop ax
    push ax
    mov bx, 42
    mul bx
    mov di, ax
    pop dx
    push dx
    mov ax, 0x6000
    mov es, ax
    mov al, [es:si]
    mov bl, al
    mov al, [es:si+1]
    mov bh, al
    push si
    push di
    push bx
    mov cx, 32
.conv_nom:
    mov al, [es:si+2]
    mov bl, al
    push es
    mov ax, 0x1000
    mov es, ax
    mov al, bl
    mov [es:di+2], al
    pop es
    inc si
    inc di
    loop .conv_nom
    pop bx
    pop di
    pop si
    mov ax, [es:si+34]
    push es
    mov cx, 0x1000
    mov es, cx
    mov [es:di+34], ax
    mov [es:di], bl
    mov [es:di+1], bh
    mov byte [es:di+36], 0
    mov byte [es:di+37], 0
    mov byte [es:di+38], 0
    mov byte [es:di+39], 0
    mov byte [es:di+40], 0
    mov byte [es:di+41], 0
    pop es
    pop dx
    inc dx
    jmp .conv11
.conv_fin:
    call validar_tabla
    cmp ax, 0
    jne .checksum_mal
    mov byte [cs:disco_ok], 1
    mov byte [cs:disco_migrar], 1
    mov si, msg_disco_mig
    call imprimir_aviso
    mov sp, [cs:tmp_sp_entry]
    popa
    ret
.mag_mal:
    call fs_inicializar
    mov byte [cs:disco_ok], 1
    mov byte [cs:disco_migrar], 0
    mov si, msg_disco_nuevo
    call imprimir_aviso
    mov sp, [cs:tmp_sp_entry]
    popa
    ret
.checksum_mal:
    call fs_inicializar
    mov byte [cs:disco_ok], 0
    mov byte [cs:disco_migrar], 0
    mov si, msg_disco_mal
    call imprimir_error
    mov sp, [cs:tmp_sp_entry]
    popa
    ret
.error_io:
    call fs_inicializar
    mov byte [cs:disco_ok], 0
    mov byte [cs:disco_migrar], 0
    mov si, msg_disco_mal
    call imprimir_error
    mov sp, [cs:tmp_sp_entry]
    popa
    ret
.sin_disco:
    mov byte [cs:disco_ok], 0
    mov byte [cs:disco_migrar], 0
    mov si, msg_disco_no
    call imprimir_aviso
    mov sp, [cs:tmp_sp_entry]
    popa
    ret
.magic2 db 'FACILOS2'
.magic1 db 'FACILOS1'

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
    mov word [es:12], 2
    mov al, [cs:teclado_es]
    mov [es:14], al
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
.mg2 db 'FACILOS2'

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

; ================= VIDEO VESA v1.2 =================
; Consola texto en modo texto 03 (80x25) o en LFB VESA (p.ej. 1920x1080x32).
; video_graf=0 texto (BIOS), =1 grafico (propio sobre LFB).
video_graf db 0
video_lfb_lo dw 0
video_lfb_hi dw 0
video_pitch dw 0
video_bpp db 0
video_mr_size db 0
video_mr_pos db 0
video_mg_size db 0
video_mg_pos db 0
video_mb_size db 0
video_mb_pos db 0
video_w dw 0
video_h dw 0
video_cx dw 0
video_cy dw 0
video_cols dw 80
video_filas dw 25
msg_res_novbe db 'Sin VESA en este equipo. Sigo en 80x25.',13,10,0
msg_res_mal db 'Modo no disponible. Usa resolucion para ver la lista.',13,10,0
msg_res_ok db 'Resolucion cambiada.',13,10,0
msg_res_txt db 'Modo texto 80x25.',13,10,0
msg_res_ahora db 'Actual: ',0
msg_res_lista db 'Modos VESA (32 bits preferido):',13,10,0
msg_res_ej db 'Ej: resolucion 1920x1080, resolucion 80x25',13,10,0
; video_putc AL=char BL=attr. En texto usa BIOS; en grafico dibuja en LFB.
video_putc:
    pusha
    cmp byte [cs:video_graf], 0
    je .vp_bios
    call video_cursor_borrar
    cmp al, 13
    je .vp_cr
    cmp al, 10
    je .vp_lf
    cmp al, 8
    je .vp_bs
    call video_glifo
    jmp .vp_adv
.vp_cr:
    mov word [cs:video_cx], 0
    jmp .vp_gfin
.vp_lf:
    push ax
    mov ax, [cs:video_cy]
    inc ax
    mov [cs:video_cy], ax
    mov cx, [cs:video_filas]
    cmp ax, cx
    jb .vp_lf_ok
    call video_scroll
    mov ax, [cs:video_filas]
    dec ax
    mov [cs:video_cy], ax
.vp_lf_ok:
    pop ax
    jmp .vp_gfin
.vp_bs:
    mov ax, [cs:video_cx]
    or ax, ax
    jz .vp_gfin
    dec ax
    mov [cs:video_cx], ax
    mov al, ' '
    mov bl, 0x07
    call video_glifo
    jmp .vp_gfin
.vp_adv:
    mov ax, [cs:video_cx]
    inc ax
    mov [cs:video_cx], ax
    mov cx, [cs:video_cols]
    cmp ax, cx
    jb .vp_gfin
    mov word [cs:video_cx], 0
    mov ax, [cs:video_cy]
    inc ax
    mov [cs:video_cy], ax
    mov cx, [cs:video_filas]
    cmp ax, cx
    jb .vp_gfin
    call video_scroll
    mov ax, [cs:video_filas]
    dec ax
    mov [cs:video_cy], ax
    jmp .vp_gfin
.vp_bios:
    mov ah, 0x0E
    mov bh, 0x00
    int 0x10
    jmp .vp_fin
.vp_gfin:
    call video_cursor_dibujar
.vp_fin:
    popa
    ret
; desplaza la consola una linea de texto hacia arriba (rellena abajo en negro).
; Usa ES irreal (origen y destino en plano). Preserva todo.
video_scroll:
    pushad
    push es
    push ds
    push fs
    call bigfs_init     ; FS=ES=irreal 4GB
    movzx eax, word [cs:video_pitch]
    shl eax, 4            ; EAX = bytes de una fila de texto
    mov ebp, eax          ; EBP = tam fila
    movzx ebx, word [cs:video_h]
    shr ebx, 4            ; filas de texto
    dec ebx               ; filas a mover
    mul ebx               ; EAX = bytes a mover
    movzx ebx, word [cs:video_lfb_lo]
    movzx ecx, word [cs:video_lfb_hi]
    shl ecx, 16
    or ebx, ecx           ; EBX = lfb
    mov esi, ebx
    add esi, ebp          ; origen = lfb + 1 fila
    mov edi, ebx          ; destino = lfb
    mov ecx, eax
    shr ecx, 2            ; dwords
    es a32 rep movsd      ; ES:ESI -> ES:EDI
    ; ultima fila en negro
    mov ecx, ebp
    shr ecx, 2
    xor eax, eax
    es a32 rep stosd      ; ES:EDI
.vs_fin:
    pop fs
    pop ds
    pop es
    popad
    ret
; limpia toda la pantalla en negro y cursor a casa. Preserva todo.
video_clear:
    pushad
    push es
    push fs
    call bigfs_init     ; ES irreal
    movzx ebx, word [cs:video_lfb_lo]
    movzx ecx, word [cs:video_lfb_hi]
    shl ecx, 16
    or ebx, ecx
    mov edi, ebx
    movzx ecx, word [cs:video_pitch]
    movzx eax, word [cs:video_h]
    mul ecx               ; EAX = bytes pantalla
    mov ecx, eax
    shr ecx, 2            ; dwords
    xor eax, eax
    es a32 rep stosd
    mov word [cs:video_cx], 0
    mov word [cs:video_cy], 0
    mov byte [cs:video_cur_vis], 0
    call video_cursor_dibujar
    pop fs
    pop es
    popad
    ret
; video_glifo: dibuja AL (BL=attr CGA 0-15) en celda (video_cx,video_cy) sobre LFB.
; 8x16, fuente en 0x5000:0 (ver video_font). Soporta bpp 32/24/16. Fondo negro.
; No usa BIOS: funciona en QEMU, VirtualBox y real. Preserva todo.
; video_color_prep: BL=attr CGA -> vg_r/g/b, vg_p32, vg_p16.
; 32bpp XRGB, 24bpp BGR, 16bpp con mascaras reales VBE. Preserva todo.
video_color_prep:
    pushad
    mov [cs:vg_attr], bl
    mov al, 170
    test bl, 8
    jz .vcp_nv
    mov al, 255
.vcp_nv:
    mov [cs:vg_lvl], al
    mov al, [cs:vg_attr]
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    test al, 4
    jz .vcp_r0
    mov bl, [cs:vg_lvl]
.vcp_r0:
    test al, 2
    jz .vcp_g0
    mov cl, [cs:vg_lvl]
.vcp_g0:
    test al, 1
    jz .vcp_b0
    mov dl, [cs:vg_lvl]
.vcp_b0:
    mov [cs:vg_r], bl
    mov [cs:vg_g], cl
    mov [cs:vg_b], dl
    mov ebp, ebx
    shl ebp, 16
    mov edi, ecx
    shl edi, 8
    or ebp, edi
    or ebp, edx
    mov [cs:vg_p32], ebp
    movzx esi, byte [cs:vg_r]
    movzx ecx, byte [cs:video_mr_size]
    mov eax, 8
    sub eax, ecx
    mov ecx, eax
    shr esi, cl
    movzx ecx, byte [cs:video_mr_pos]
    shl esi, cl
    mov ebp, esi
    movzx esi, byte [cs:vg_g]
    movzx ecx, byte [cs:video_mg_size]
    mov eax, 8
    sub eax, ecx
    mov ecx, eax
    shr esi, cl
    movzx ecx, byte [cs:video_mg_pos]
    shl esi, cl
    or ebp, esi
    movzx esi, byte [cs:vg_b]
    movzx ecx, byte [cs:video_mb_size]
    mov eax, 8
    sub eax, ecx
    mov ecx, eax
    shr esi, cl
    movzx ecx, byte [cs:video_mb_pos]
    shl esi, cl
    or ebp, esi
    mov [cs:vg_p16], ebp
    popad
    ret
video_glifo:
    pushad
    push es
    push ds
    push fs
    movzx esi, al
    shl esi, 4
    call video_color_prep
    mov word [cs:vg_rows], 16
    mov word [cs:vg_cols], 8
    call bigfs_init
    movzx edi, word [cs:video_lfb_lo]
    movzx ebp, word [cs:video_lfb_hi]
    shl ebp, 16
    or edi, ebp
    movzx eax, word [cs:video_cy]
    shl eax, 4
    movzx ebx, word [cs:video_pitch]
    mul ebx
    add edi, eax
    movzx eax, word [cs:video_cx]
    shl eax, 3
    movzx ebx, byte [cs:video_bpp]
    cmp bl, 24
    je .vg_px3
    cmp bl, 16
    je .vg_px2
    mov ebx, 4
    mul ebx
    add edi, eax
    mov ax, 0x5000
    mov ds, ax
    mov ebx, [cs:vg_p32]
    movzx ebp, word [cs:vg_rows]
    call video_glifo_rows32
    jmp .vg_fin
.vg_px3:
    mov ebx, 3
    mul ebx
    add edi, eax
    mov ax, 0x5000
    mov ds, ax
    movzx ebp, word [cs:vg_rows]
    call video_glifo_rows24
    jmp .vg_fin
.vg_px2:
    mov ebx, 2
    mul ebx
    add edi, eax
    mov ax, 0x5000
    mov ds, ax
    mov bx, [cs:vg_p16]
    movzx ebp, word [cs:vg_rows]
    call video_glifo_rows16
    jmp .vg_fin
.vg_fin:
    pop fs
    pop ds
    pop es
    popad
    ret
vg_attr db 0
vg_lvl db 0
vg_r db 0
vg_g db 0
vg_b db 0
vg_p32 dd 0
vg_p16 dd 0
vg_rows dw 16
vg_cols dw 8
; filas de glifo compartidas: EDI=base fb, ESI=off fuente, EBP=filas.
; DS=fuente 0x5000, ES=plano. No preserva nada (salvo ESP).
video_glifo_rows32:
.vg_r32:
    mov al, [esi]
    inc esi
    mov ah, 0x80
    push edi
    mov cx, [cs:vg_cols]
.vg_rc32:
    test al, ah
    jz .vg_rz32
    mov [es:edi], ebx
    jmp .vg_ra32
.vg_rz32:
    mov dword [es:edi], 0
.vg_ra32:
    add edi, 4
    shr ah, 1
    loop .vg_rc32
    pop edi
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    dec ebp
    jnz .vg_r32
    ret
video_glifo_rows24:
.vg_r24:
    mov al, [esi]
    inc esi
    mov ah, 0x80
    push edi
    mov cx, [cs:vg_cols]
.vg_rc24:
    test al, ah
    jz .vg_rz24
    push ax
    mov al, [cs:vg_b]
    mov [es:edi], al
    mov al, [cs:vg_g]
    mov [es:edi+1], al
    mov al, [cs:vg_r]
    mov [es:edi+2], al
    pop ax
    jmp .vg_ra24
.vg_rz24:
    mov byte [es:edi], 0
    mov byte [es:edi+1], 0
    mov byte [es:edi+2], 0
.vg_ra24:
    add edi, 3
    shr ah, 1
    loop .vg_rc24
    pop edi
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    dec ebp
    jnz .vg_r24
    ret
video_glifo_rows16:
.vg_r16:
    mov al, [esi]
    inc esi
    mov ah, 0x80
    push edi
    mov cx, [cs:vg_cols]
.vg_rc16:
    test al, ah
    jz .vg_rz16
    mov [es:edi], bx
    jmp .vg_ra16
.vg_rz16:
    mov word [es:edi], 0
.vg_ra16:
    add edi, 2
    shr ah, 1
    loop .vg_rc16
    pop edi
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    dec ebp
    jnz .vg_r16
    ret
; ================= PRIMITIVAS GRAFICAS v1.3 =================
; Requieren modo grafico. Todas con clipping (comparaciones sin signo:
; coordenadas negativas llegan como >W/H y se recortan solas). Preservan todo.
; video_pixel: AX=x BX=y DL=color(attr). Dibuja 1 pixel.
video_pixel:
    pushad
    push es
    push fs
    movzx esi, ax
    cmp si, [cs:video_w]
    jae .vpx_fin
    movzx edi, bx
    cmp di, [cs:video_h]
    jae .vpx_fin
    mov bl, dl
    call video_color_prep
    call bigfs_init
    mov eax, edi
    movzx ebx, word [cs:video_pitch]
    mul ebx
    movzx edi, word [cs:video_lfb_lo]
    movzx ebp, word [cs:video_lfb_hi]
    shl ebp, 16
    or edi, ebp
    add edi, eax
    mov eax, esi
    movzx ebx, byte [cs:video_bpp]
    cmp bl, 24
    je .vpx_24
    cmp bl, 16
    je .vpx_16
    shl eax, 2
    add edi, eax
    mov eax, [cs:vg_p32]
    mov [es:edi], eax
    jmp .vpx_fin
.vpx_24:
    mov ebx, eax
    shl eax, 1
    add eax, ebx
    add edi, eax
    mov al, [cs:vg_b]
    mov [es:edi], al
    mov al, [cs:vg_g]
    mov [es:edi+1], al
    mov al, [cs:vg_r]
    mov [es:edi+2], al
    jmp .vpx_fin
.vpx_16:
    shl eax, 1
    add edi, eax
    mov ax, [cs:vg_p16]
    mov [es:edi], ax
.vpx_fin:
    pop fs
    pop es
    popad
    ret
; video_rect_fill: EDI=base fila0, ECX=ancho>0, EDX=alto>0.
; ES plano, color en temps. Relleno eficiente (rep stosd/stosw). Preserva todo.
video_rect_fill:
    pushad
    or ecx, ecx
    jz .vrf_fin
    or edx, edx
    jz .vrf_fin
    mov ebp, ecx
    mov esi, edx
    movzx ebx, byte [cs:video_bpp]
    cmp bl, 24
    je .vrf_24
    cmp bl, 16
    je .vrf_16
    mov eax, [cs:vg_p32]
.vrf_r32:
    push edi
    mov ecx, ebp
    a32 rep stosd
    pop edi
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    dec esi
    jnz .vrf_r32
    jmp .vrf_fin
.vrf_16:
    mov ax, [cs:vg_p16]
.vrf_r16:
    push edi
    mov ecx, ebp
    a32 rep stosw
    pop edi
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    dec esi
    jnz .vrf_r16
    jmp .vrf_fin
.vrf_24:
.vrf_r24:
    push edi
    mov ecx, ebp
.vrf_c24:
    mov al, [cs:vg_b]
    mov [es:edi], al
    mov al, [cs:vg_g]
    mov [es:edi+1], al
    mov al, [cs:vg_r]
    mov [es:edi+2], al
    add edi, 3
    loop .vrf_c24
    pop edi
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    dec esi
    jnz .vrf_r24
.vrf_fin:
    popad
    ret
; video_rectangulo: AX=x BX=y CX=ancho DX=alto SI=color. Relleno con clip.
video_rectangulo:
    pushad
    push es
    push fs
    or cx, cx
    jz .vrc_fin
    or dx, dx
    jz .vrc_fin
    mov [cs:vrc_col], si
    movsx esi, ax
    movsx edi, bx
    movzx ecx, cx
    movzx edx, dx
    mov eax, esi
    add eax, ecx
    mov ebx, edi
    add ebx, edx
    movzx ebp, word [cs:video_w]
    cmp esi, ebp
    jge .vrc_fin
    movzx ebp, word [cs:video_h]
    cmp edi, ebp
    jge .vrc_fin
    cmp eax, 0
    jle .vrc_fin
    cmp ebx, 0
    jle .vrc_fin
    or esi, esi
    jns .vrc_x0
    xor esi, esi
.vrc_x0:
    movzx ebp, word [cs:video_w]
    cmp eax, ebp
    jle .vrc_x1
    mov eax, ebp
.vrc_x1:
    sub eax, esi
    jle .vrc_fin
    mov ecx, eax
    or edi, edi
    jns .vrc_y0
    xor edi, edi
.vrc_y0:
    movzx ebp, word [cs:video_h]
    cmp ebx, ebp
    jle .vrc_y1
    mov ebx, ebp
.vrc_y1:
    sub ebx, edi
    jle .vrc_fin
    mov edx, ebx
    push edx
    mov bx, [cs:vrc_col]
    call video_color_prep
    call bigfs_init
    mov eax, edi
    movzx ebx, word [cs:video_pitch]
    mul ebx
    movzx edi, word [cs:video_lfb_lo]
    movzx ebp, word [cs:video_lfb_hi]
    shl ebp, 16
    or edi, ebp
    add edi, eax
    mov eax, esi
    movzx ebx, byte [cs:video_bpp]
    cmp bl, 24
    je .vrc_b3
    cmp bl, 16
    je .vrc_b2
    shl eax, 2
    jmp .vrc_bg
.vrc_b3:
    mov ebx, eax
    shl eax, 1
    add eax, ebx
    jmp .vrc_bg
.vrc_b2:
    shl eax, 1
.vrc_bg:
    add edi, eax
    pop edx
    call video_rect_fill
.vrc_fin:
    pop fs
    pop es
    popad
    ret
; video_limpiar: BL=color. Rellena todo el framebuffer (respeta pitch).
video_limpiar:
    pushad
    push es
    push fs
    call video_color_prep
    call bigfs_init
    movzx edi, word [cs:video_lfb_lo]
    movzx ebp, word [cs:video_lfb_hi]
    shl ebp, 16
    or edi, ebp
    movzx ecx, word [cs:video_w]
    movzx edx, word [cs:video_h]
    call video_rect_fill
    pop fs
    pop es
    popad
    ret
vrc_col dw 0
gl_x1 dw 0
gl_y1 dw 0
gl_x2 dw 0
gl_y2 dw 0
gl_dx dw 0
gl_dy dw 0
gl_sx dw 0
gl_sy dw 0
; video_linea: AX=x1 BX=y1 CX=x2 DX=y2 SI=color. Bresenham + clip por punto.
video_linea:
    pushad
    push es
    push fs
    mov [cs:gl_x1], ax
    mov [cs:gl_y1], bx
    mov [cs:gl_x2], cx
    mov [cs:gl_y2], dx
    mov [cs:gl_col], si
    movsx eax, word [cs:gl_x2]
    movsx ebx, word [cs:gl_x1]
    sub eax, ebx
    jns .gln_dxpos
    neg eax
    mov word [cs:gl_sx], -1
    jmp .gln_dxd
.gln_dxpos:
    mov word [cs:gl_sx], 1
.gln_dxd:
    mov [cs:gl_dx], ax
    movsx eax, word [cs:gl_y2]
    movsx ebx, word [cs:gl_y1]
    sub eax, ebx
    jns .gln_dypos
    neg eax
    mov word [cs:gl_sy], -1
    jmp .gln_dyd
.gln_dypos:
    mov word [cs:gl_sy], 1
.gln_dyd:
    mov [cs:gl_dy], ax
    mov bx, [cs:gl_col]
    call video_color_prep
    call bigfs_init
    movsx esi, word [cs:gl_x1]
    movsx edi, word [cs:gl_y1]
    movzx eax, word [cs:gl_dx]
    movzx ebx, word [cs:gl_dy]
    sub eax, ebx
    mov ebp, eax
.gln_loop:
    or esi, esi
    js .gln_skip
    movzx ebx, word [cs:video_w]
    cmp esi, ebx
    jae .gln_skip
    or edi, edi
    js .gln_skip
    movzx ebx, word [cs:video_h]
    cmp edi, ebx
    jae .gln_skip
    mov eax, edi
    movzx ebx, word [cs:video_pitch]
    mul ebx
    movzx edx, word [cs:video_lfb_lo]
    movzx ebx, word [cs:video_lfb_hi]
    shl ebx, 16
    or edx, ebx
    add edx, eax
    mov eax, esi
    movzx ebx, byte [cs:video_bpp]
    cmp bl, 24
    je .gln_b3
    cmp bl, 16
    je .gln_b2
    shl eax, 2
    add edx, eax
    mov eax, [cs:vg_p32]
    mov [es:edx], eax
    jmp .gln_skip
.gln_b3:
    mov ebx, eax
    shl eax, 1
    add eax, ebx
    add edx, eax
    mov al, [cs:vg_b]
    mov [es:edx], al
    mov al, [cs:vg_g]
    mov [es:edx+1], al
    mov al, [cs:vg_r]
    mov [es:edx+2], al
    jmp .gln_skip
.gln_b2:
    shl eax, 1
    add edx, eax
    mov ax, [cs:vg_p16]
    mov [es:edx], ax
.gln_skip:
    mov eax, esi
    cmp ax, [cs:gl_x2]
    jne .gln_cont
    mov eax, edi
    cmp ax, [cs:gl_y2]
    jne .gln_cont
    jmp .gln_fin
.gln_cont:
    mov eax, ebp
    shl eax, 1
    movzx ebx, word [cs:gl_dy]
    neg ebx
    cmp eax, ebx
    jle .gln_nox
    movzx ebx, word [cs:gl_dy]
    sub ebp, ebx
    cmp word [cs:gl_sx], 0
    jge .gln_xp
    sub esi, 1
    jmp .gln_nox
.gln_xp:
    add esi, 1
.gln_nox:
    movzx ebx, word [cs:gl_dx]
    cmp eax, ebx
    jge .gln_loop
    movzx ebx, word [cs:gl_dx]
    add ebp, ebx
    cmp word [cs:gl_sy], 0
    jge .gln_yp
    sub edi, 1
    jmp .gln_loop
.gln_yp:
    add edi, 1
    jmp .gln_loop
.gln_fin:
    pop fs
    pop es
    popad
    ret
gl_col dw 0
gt_x0 dw 0
gt_x dw 0
gt_y dw 0
gt_si dw 0
; video_texto_xy: AX=x BX=y SI=texto(DS) DL=color. Fuente 8x16, \n soportado.
; Clip por glifo (nunca escribe fuera). Preserva todo.
video_texto_xy:
    pushad
    push es
    push ds
    push fs
    mov [cs:gt_x0], ax
    mov [cs:gt_x], ax
    mov [cs:gt_y], bx
    mov [cs:gt_si], si
    mov bl, dl
    call video_color_prep
    call bigfs_init
.gt_loop:
    mov si, [cs:gt_si]
    mov al, [si]
    or al, al
    jz .gt_fin
    inc si
    mov [cs:gt_si], si
    cmp al, 10
    je .gt_nl
    movzx esi, al
    shl esi, 4
    movzx eax, word [cs:gt_x]
    movzx ebx, word [cs:video_w]
    cmp eax, ebx
    jae .gt_adv
    movzx ebx, word [cs:gt_y]
    movzx ebp, word [cs:video_h]
    cmp ebx, ebp
    jae .gt_adv
    movzx ecx, word [cs:video_h]
    sub ecx, ebx
    cmp ecx, 16
    jbe .gt_rok
    mov ecx, 16
.gt_rok:
    mov [cs:vg_rows], cx
    movzx ecx, word [cs:video_w]
    sub ecx, eax
    cmp ecx, 8
    jbe .gt_cok
    mov ecx, 8
.gt_cok:
    mov [cs:vg_cols], cx
    mov eax, ebx
    movzx ebx, word [cs:video_pitch]
    mul ebx
    movzx edi, word [cs:video_lfb_lo]
    movzx ebp, word [cs:video_lfb_hi]
    shl ebp, 16
    or edi, ebp
    add edi, eax
    movzx eax, word [cs:gt_x]
    movzx ebx, byte [cs:video_bpp]
    cmp bl, 24
    je .gt_b3
    cmp bl, 16
    je .gt_b2
    shl eax, 2
    add edi, eax
    mov ax, 0x5000
    mov ds, ax
    mov ebx, [cs:vg_p32]
    movzx ebp, word [cs:vg_rows]
    call video_glifo_rows32
    jmp .gt_dsok
.gt_b3:
    mov ebx, eax
    shl eax, 1
    add eax, ebx
    add edi, eax
    mov ax, 0x5000
    mov ds, ax
    movzx ebp, word [cs:vg_rows]
    call video_glifo_rows24
    jmp .gt_dsok
.gt_b2:
    shl eax, 1
    add edi, eax
    mov ax, 0x5000
    mov ds, ax
    mov bx, [cs:vg_p16]
    movzx ebp, word [cs:vg_rows]
    call video_glifo_rows16
.gt_dsok:
    xor ax, ax
    mov ds, ax
.gt_adv:
    mov ax, [cs:gt_x]
    add ax, 8
    mov [cs:gt_x], ax
    jmp .gt_loop
.gt_nl:
    mov ax, [cs:gt_x0]
    mov [cs:gt_x], ax
    mov ax, [cs:gt_y]
    add ax, 16
    mov [cs:gt_y], ax
    jmp .gt_loop
.gt_fin:
    pop fs
    pop ds
    pop es
    popad
    ret
; cursor propio en grafico: guion bajo XOR (filas 14-15 de la celda).
video_cur_vis db 0
video_cursor_borrar:
    pushad
    cmp byte [cs:video_cur_vis], 0
    je .vcb_fin
    call video_cursor_xor
    mov byte [cs:video_cur_vis], 0
.vcb_fin:
    popad
    ret
video_cursor_dibujar:
    pushad
    cmp byte [cs:video_cur_vis], 1
    je .vcd_fin
    call video_cursor_xor
    mov byte [cs:video_cur_vis], 1
.vcd_fin:
    popad
    ret
video_cursor_xor:
    pushad
    push es
    push fs
    call bigfs_init
    movzx edi, word [cs:video_lfb_lo]
    movzx ebp, word [cs:video_lfb_hi]
    shl ebp, 16
    or edi, ebp
    movzx eax, word [cs:video_cy]
    shl eax, 4
    add eax, 14
    movzx ebx, word [cs:video_pitch]
    mul ebx
    add edi, eax
    movzx eax, word [cs:video_cx]
    shl eax, 3
    movzx ebx, byte [cs:video_bpp]
    cmp bl, 24
    je .vcx_24
    cmp bl, 16
    je .vcx_16
    mov ebx, 4
    mul ebx
    add edi, eax
    mov ebp, 0x00FFFFFF
    call .vcx_f32
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    call .vcx_f32
    jmp .vcx_fin
.vcx_f32:
    push edi
    mov cx, 8
.vcx_c32:
    xor dword [es:edi], ebp
    add edi, 4
    loop .vcx_c32
    pop edi
    ret
.vcx_24:
    mov ebx, 3
    mul ebx
    add edi, eax
    call .vcx_f24
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    call .vcx_f24
    jmp .vcx_fin
.vcx_f24:
    push edi
    mov cx, 8
.vcx_c24:
    xor byte [es:edi], 0xFF
    xor byte [es:edi+1], 0xFF
    xor byte [es:edi+2], 0xFF
    add edi, 3
    loop .vcx_c24
    pop edi
    ret
.vcx_16:
    mov ebx, 2
    mul ebx
    add edi, eax
    call .vcx_f16
    movzx ecx, word [cs:video_pitch]
    add edi, ecx
    call .vcx_f16
    jmp .vcx_fin
.vcx_f16:
    push edi
    mov cx, 8
.vcx_c16:
    xor word [es:edi], 0xFFFF
    add edi, 2
    loop .vcx_c16
    pop edi
    ret
.vcx_fin:
    pop fs
    pop es
    popad
    ret
; copia la fuente 8x16 del BIOS a 0x5000:0 (4096 B). -> AX 0 ok. Preserva todo menos AX.
video_font:
    pusha
    push es
    push ds
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10              ; ES:BP = fuente
    mov si, bp
    mov dx, es
    mov ax, 0x5000
    mov es, ax
    xor di, di
    push ds
    mov ds, dx
    mov cx, 4096
    rep movsb
    pop ds
    pop ds
    pop es
    popa
    xor ax, ax
    ret
; ---- VBE: info en 0x7000:0 (512) y modo en 0x7000:0x200 (256) ----
; vbe_info -> AX 1 si hay VESA (firma VESA). Preserva todo menos AX.
vbe_info:
    pusha
    push es
    push ds
    mov ax, 0x7000
    mov es, ax
    xor di, di
    mov ax, [cs:.sgn0]
    mov [es:di], ax
    mov ax, [cs:.sgn1]
    mov [es:di+2], ax
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .vi_no
    mov ax, [es:0]
    cmp ax, 0x4556        ; 'VE'
    jne .vi_no
    mov ax, [es:2]
    cmp ax, 0x4153        ; 'SA'
    jne .vi_no
    mov ax, 1
    jmp .vi_fin
.vi_no:
    xor ax, ax
.vi_fin:
    mov [cs:.vi_rc], ax
    pop ds
    pop es
    popa
    mov ax, [cs:.vi_rc]
    ret
.sgn0 dw 0x4256           ; 'VB' -> pedimos VBE2 ("VBE2")
.sgn1 dw 0x3245           ; '2E'
.vi_rc dw 0
; vbe_modo_info CX=modo -> AX 1 si valido (lee a 0x7000:0x200). Preserva todo menos AX.
vbe_modo_info:
    pusha
    push es
    mov ax, 0x7000
    mov es, ax
    mov di, 0x200
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .vm_no
    mov ax, 1
    jmp .vm_fin
.vm_no:
    xor ax, ax
.vm_fin:
    mov [cs:.vm_rc], ax
    pop es
    popa
    mov ax, [cs:.vm_rc]
    ret
.vm_rc dw 0
; buscar modo W=AX H=BX BPP preferido -> CX=modo o 0. Usa lista de vbe_info.
; Preserva todo menos AX,CX.
vbe_buscar:
    push bx
    push dx
    push si
    push di
    push es
    mov [cs:.vb_w], ax
    mov [cs:.vb_h], bx
    mov word [cs:.vb_m32], 0
    mov word [cs:.vb_m24], 0
    mov word [cs:.vb_m16], 0
    call vbe_info
    cmp ax, 0
    je .vb_no
    mov ax, 0x7000
    mov es, ax
    mov si, [es:14]
    mov dx, [es:16]       ; DX:SI = puntero lista modos (seg:off)
    push ds
    mov ds, dx
.vb_loop:
    mov cx, [si]            ; DS:SI = lista (DS se preserva)
    cmp cx, 0xFFFF
    je .vb_fin_lista
    mov [cs:.vb_cx], cx
    push si
    call vbe_modo_info    ; CX=modo, preserva todo menos AX
    pop si
    cmp ax, 0
    je .vb_next
    push es
    mov ax, 0x7000
    mov es, ax
    mov dx, [es:0x200]  ; atributos: exigir LFB (bit 7)
    test dx, 0x0080
    jz .vb_nx
    mov dx, [es:0x200+18] ; X
    cmp dx, [cs:.vb_w]
    jne .vb_nx
    mov dx, [es:0x200+20] ; Y
    cmp dx, [cs:.vb_h]
    jne .vb_nx
    mov dl, [es:0x200+25] ; BPP
    pop es
    push cx
    mov cx, [cs:.vb_cx]
    cmp dl, 32
    je .vb_si32
    cmp dl, 24
    je .vb_si24
    cmp dl, 16
    je .vb_si16
    pop cx
    jmp .vb_next
.vb_si32:
    mov [cs:.vb_m32], cx
    pop cx
    jmp .vb_next
.vb_si24:
    pop cx
    push cx
    cmp word [cs:.vb_m32], 0
    jne .vb_popnext
    mov [cs:.vb_m24], cx
.vb_popnext:
    pop cx
    jmp .vb_next
.vb_si16:
    pop cx
    push cx
    cmp word [cs:.vb_m32], 0
    jne .vb_popnext
    cmp word [cs:.vb_m24], 0
    jne .vb_popnext
    mov [cs:.vb_m16], cx
    jmp .vb_popnext
.vb_nx:
    pop es
.vb_next:
    add si, 2
    jmp .vb_loop
.vb_fin_lista:
    pop ds
    mov cx, [cs:.vb_m32]
    or cx, cx
    jnz .vb_ok
    mov cx, [cs:.vb_m24]
    or cx, cx
    jnz .vb_ok
    mov cx, [cs:.vb_m16]
    or cx, cx
    jnz .vb_ok
.vb_no:
    xor cx, cx
    jmp .vb_fin
.vb_ok:
    mov ax, 1
.vb_fin:
    mov [cs:.vb_rc], ax
    mov [cs:.vb_cx], cx
    pop es
    pop di
    pop si
    pop dx
    pop bx
    mov ax, [cs:.vb_rc]
    mov cx, [cs:.vb_cx]
    ret
.vb_w dw 0
.vb_h dw 0
.vb_m32 dw 0
.vb_m24 dw 0
.vb_m16 dw 0
.vb_rc dw 0
.vb_cx dw 0
; fijar modo CX (VBE con LFB) y preparar consola. -> AX 1 ok. Preserva todo menos AX.
vbe_fijar:
    pusha
    push es
    mov [cs:.vf_cx], cx
    mov word [cs:.vf_set], 0
    mov bx, cx
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .vf_no
    mov word [cs:.vf_set], 1
    ; comprobar que el modo quedo activo (4F03)
    mov ax, 0x4F03
    int 0x10
    cmp ax, 0x004F
    jne .vf_no
    mov ax, bx
    and ax, 0x3FFF
    cmp ax, [cs:.vf_cx]
    jne .vf_no
    mov cx, [cs:.vf_cx]
    call vbe_modo_info
    cmp ax, 0
    je .vf_no
    mov ax, 0x7000
    mov es, ax
    mov ax, [es:0x200+18]
    mov [cs:video_w], ax
    mov cx, ax
    shr cx, 3
    mov [cs:video_cols], cx
    mov ax, [es:0x200+20]
    mov [cs:video_h], ax
    mov cx, ax
    shr cx, 4
    mov [cs:video_filas], cx
    mov ax, [es:0x200+16]
    mov [cs:video_pitch], ax
    mov al, [es:0x200+25]
    mov [cs:video_bpp], al
    mov al, [es:0x200+31]
    mov [cs:video_mr_size], al
    mov al, [es:0x200+32]
    mov [cs:video_mr_pos], al
    mov al, [es:0x200+33]
    mov [cs:video_mg_size], al
    mov al, [es:0x200+34]
    mov [cs:video_mg_pos], al
    mov al, [es:0x200+35]
    mov [cs:video_mb_size], al
    mov al, [es:0x200+36]
    mov [cs:video_mb_pos], al
    mov al, [cs:video_mr_size]
    or al, [cs:video_mg_size]
    or al, [cs:video_mb_size]
    jnz .vf_maskok
    mov byte [cs:video_mr_size], 5
    mov byte [cs:video_mr_pos], 11
    mov byte [cs:video_mg_size], 6
    mov byte [cs:video_mg_pos], 5
    mov byte [cs:video_mb_size], 5
    mov byte [cs:video_mb_pos], 0
.vf_maskok:
    mov ax, [es:0x200+40]
    mov [cs:video_lfb_lo], ax
    mov ax, [es:0x200+42]
    mov [cs:video_lfb_hi], ax
    ; validar cordura del modo (nunca dejar pantalla ciega)
    mov ax, [es:0x200+18]
    cmp ax, 640
    jb .vf_no
    mov ax, [es:0x200+20]
    cmp ax, 400
    jb .vf_no
    mov ax, [es:0x200+16]
    or ax, ax
    jz .vf_no
    cmp ax, 16384
    ja .vf_no
    mov al, [es:0x200+25]
    cmp al, 32
    je .vf_bppok
    cmp al, 24
    je .vf_bppok
    cmp al, 16
    jne .vf_no
.vf_bppok:
    mov ax, [es:0x200+40]
    or ax, [es:0x200+42]
    jz .vf_no
    mov byte [cs:video_graf], 1
    mov word [cs:video_cx], 0
    mov word [cs:video_cy], 0
    call video_font     ; fuente 8x16 a 0x5000 (texto propio, sin BIOS)
    ; comprobar fuente no vacia (glifos A+B)
    push ds
    mov ax, 0x5000
    mov ds, ax
    mov si, 0x410
    mov cx, 32
    xor ax, ax
.vf_fchk:
    or al, [si]
    inc si
    loop .vf_fchk
    pop ds
    or al, al
    jz .vf_nofont
    call video_clear
    mov ax, 1
    jmp .vf_fin
.vf_nofont:
    jmp .vf_no
.vf_no:
    cmp word [cs:.vf_set], 0
    je .vf_rc0
    call video_texto
.vf_rc0:
    xor ax, ax
    jmp .vf_fin
.vf_fin:
    mov [cs:.vf_rc], ax
    pop es
    popa
    mov ax, [cs:.vf_rc]
    ret
.vf_cx dw 0
.vf_set dw 0
.vf_rc dw 0
; volver a texto 80x25. Preserva todo.
video_texto:
    pusha
    mov ax, 0x0003
    int 0x10
    mov byte [cs:video_graf], 0
    mov byte [cs:video_cur_vis], 0
    mov word [cs:video_cols], 80
    mov word [cs:video_filas], 25
    mov word [cs:video_cx], 0
    mov word [cs:video_cy], 0
    popa
    ret
; eco_putc AL=char (gris + serial). Respeta modo grafico/texto. Preserva todo.
eco_putc:
    pusha
    push ax
    call serial_putc
    pop ax
    cmp byte [cs:video_graf], 0
    je .ec_b
    mov bl, 0x07
    call video_putc
    jmp .ec_f
.ec_b:
    mov ah, 0x0E
    mov bh, 0
    mov bl, 7
    int 0x10
.ec_f:
    popa
    ret
; eco_backspace: borra una celda a la izquierda. Preserva todo.
eco_backspace:
    pusha
    cmp byte [cs:video_graf], 0
    je .eb_b
    mov al, 8
    mov bl, 0x07
    call video_putc
    jmp .eb_f
.eb_b:
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
.eb_f:
    popa
    ret
; activa big-real-mode en FS y ES (limite 4GB) para el LFB alto. Preserva todo.
; (Dejar limites a 4GB no afecta al codigo real: todos los offsets son <64KB.)
bigfs_init:
    pusha
    pushf
    cli
    lgdt [cs:.gdt_ptr]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp $+2
    mov bx, 0x10
    mov fs, bx
    mov es, bx
    and al, 0xFE
    mov cr0, eax
    jmp $+2
    popf
    popa
    ret
.gdt_ptr:
    dw .gdt_fin - .gdt - 1
    dd .gdt
.gdt:
    dd 0, 0
    dd 0x0000FFFF, 0x00CF9A00
    dd 0x0000FFFF, 0x00CF9300
.gdt_fin:

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
    mov di, cmd_limphist
    call cadenas_iguales
    cmp ax, 0
    je .dlimphist
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_teclado
    call cadenas_iguales
    cmp ax, 0
    je .dteclado
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_resolucion
    call cadenas_iguales
    cmp ax, 0
    je .dresolucion
    mov si, buffer_args
    call saltar_espacios
    mov di, cmd_prueba
    call cadenas_iguales
    cmp ax, 0
    je .dprueba
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
    jmp .fin
.dteclado: mov si, msg_desc_teclado
    call imprimir
    jmp .fin
.dresolucion: mov si, msg_desc_resolucion
    call imprimir
    jmp .fin
.dprueba: mov si, msg_desc_prueba
    call imprimir
    jmp .fin
.fin:
    popa
    ret

h_ver:
    pusha
    ; sintaxis: ver_archivos [ruta] [opt] | ver_archivos [opt] [ruta] | ver_archivos
    mov ax, [dir_actual]
    mov [cs:tmp_ver_dir], ax
    mov bx, 0               ; modo 0=normal 1=carp 2=arch 3=todo
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .listar
    ; tok1 -> buffer_ruta2
    mov di, buffer_ruta2
    call copiar_palabra
    ; tok2 = resto
    mov si, buffer_args
    call saltar_espacios
.vw_adv1:
    mov al, [si]
    cmp al, 0
    je .vw_tok2vacio
    cmp al, ' '
    je .vw_tok2ini
    inc si
    jmp .vw_adv1
.vw_tok2ini:
    call saltar_espacios
.vw_tok2vacio:
    mov di, buffer_nombre2
    call copiar_palabra   ; tok2 (puede quedar vacio; NO usar buffer_comp: lo usa resolver)
    ; clasificar tok1
    mov si, buffer_ruta2
    mov di, opt_carpetas
    call cadenas_iguales
    cmp ax, 0
    je .vw_m1
    mov si, buffer_ruta2
    mov di, opt_archivos
    call cadenas_iguales
    cmp ax, 0
    je .vw_m2
    mov si, buffer_ruta2
    mov di, opt_todo
    call cadenas_iguales
    cmp ax, 0
    je .vw_m3
    mov si, buffer_ruta2
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ay
    ; tok1 = ruta
    mov si, buffer_ruta2
    call resolver_dir
    cmp ax, 0xFFFF
    je .vw_malruta
    mov [cs:tmp_ver_dir], ax
    ; tok2 debe ser vacio u opt
    cmp byte [buffer_nombre2], 0
    je .listar
    mov si, buffer_nombre2
    mov di, opt_carpetas
    call cadenas_iguales
    cmp ax, 0
    je .modo1
    mov si, buffer_nombre2
    mov di, opt_archivos
    call cadenas_iguales
    cmp ax, 0
    je .modo2
    mov si, buffer_nombre2
    mov di, opt_todo
    call cadenas_iguales
    cmp ax, 0
    je .modo3
    mov si, msg_desc_ver
    call imprimir
    jmp .finv
.vw_m1:
    mov bx, 1
    jmp .vw_seg
.vw_m2:
    mov bx, 2
    jmp .vw_seg
.vw_m3:
    mov bx, 3
.vw_seg:
    ; tok1 era opt: tok2 vacio o ruta
    cmp byte [buffer_nombre2], 0
    je .listar
    mov si, buffer_nombre2
    call resolver_dir
    cmp ax, 0xFFFF
    je .vw_malruta
    mov [cs:tmp_ver_dir], ax
    jmp .listar
.vw_malruta:
    mov si, msg_ruta_mala
    call imprimir_error
    jmp .finv
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
    cmp bl, 0
    jne .vrep
    ; modo normal: formato por secciones CARPETAS:/ARCHIVOS:
    call .v_secciones
    jmp .finv
.v_secciones:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    ; preconteo: si no hay hijos, solo (vacio)
    mov word [cs:tmp_idx_ver], 0
.vs_pre:
    mov ax, [cs:tmp_idx_ver]
    cmp ax, 64
    jae .vs_pre_vacio
    mov cx, [cs:tmp_ver_dir]
    cmp ax, cx
    je .vs_pre_next
    call fs_leer_tipo
    or al, al
    jz .vs_pre_next
    mov bx, ax
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_padre
    mov cx, [cs:tmp_ver_dir]
    cmp ax, cx
    jne .vs_pre_next
    jmp .vs_hay
.vs_pre_next:
    inc word [cs:tmp_idx_ver]
    jmp .vs_pre
.vs_pre_vacio:
    mov si, msg_vacia
    call imprimir
    jmp .vs_ok
.vs_hay:
    mov si, msg_ver_carpetas
    call imprimir_azul
    mov word [cs:tmp_idx_ver], 0
.vs_pass1:
    mov ax, [cs:tmp_idx_ver]
    cmp ax, 64
    jae .vs_pass2
    mov cx, [cs:tmp_ver_dir]
    cmp ax, cx
    je .vs_next1
    call fs_leer_tipo
    cmp al, 1
    jne .vs_next1
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_padre
    mov cx, [cs:tmp_ver_dir]
    cmp ax, cx
    jne .vs_next1
    mov ax, [cs:tmp_idx_ver]
    mov di, buffer_nombre
    call fs_copiar_nombre
    mov si, msg_espacio
    call imprimir
    mov si, buffer_nombre
    call imprimir_azul
    call nueva_linea
    inc word [cs:tmp_cont_ver]
.vs_next1:
    inc word [cs:tmp_idx_ver]
    jmp .vs_pass1
.vs_pass2:
    mov si, msg_ver_archivos
    call imprimir_blanco
    mov word [cs:tmp_idx_ver], 0
.vs_rep2:
    mov ax, [cs:tmp_idx_ver]
    cmp ax, 64
    jae .vs_fin2
    mov cx, [cs:tmp_ver_dir]
    cmp ax, cx
    je .vs_next2
    call fs_leer_tipo
    cmp al, 2
    jne .vs_next2
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_padre
    mov cx, [cs:tmp_ver_dir]
    cmp ax, cx
    jne .vs_next2
    mov si, msg_espacio
    call imprimir
    mov ax, [cs:tmp_idx_ver]
    mov di, buffer_nombre
    call fs_copiar_nombre
    mov si, buffer_nombre
    call imprimir_blanco
    mov al, ' '
    call imprimir_char
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_tam
    call imprimir_numero
    mov si, msg_ver_unidad
    call imprimir
    call nueva_linea
    inc word [cs:tmp_cont_ver]
.vs_next2:
    inc word [cs:tmp_idx_ver]
    jmp .vs_rep2
.vs_fin2:
    cmp word [cs:tmp_cont_ver], 0
    jne .vs_ok
    mov si, msg_vacia
    call imprimir
.vs_ok:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
.vrep:
    mov ax, [cs:tmp_idx_ver]
    cmp ax, 64
    jae .vfin
    mov cx, [cs:tmp_ver_dir]
    cmp ax, cx
    je .vsig               ; no listar la carpeta actual
    call fs_leer_tipo      ; AL=tipo, preserva DX/CX
    mov bl, al
    or bl, bl
    jz .vsig
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_padre     ; AX=padre
    mov cx, [cs:tmp_ver_dir]
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
    jne .det_fechas
    mov al, ' '
    call imprimir_char
    mov al, '('
    call imprimir_char
    mov ax, [cs:tmp_idx_ver]
    call fs_leer_tam
    call imprimir_numero
    mov al, ')'
    call imprimir_char
.det_fechas:
    mov al, ' '
    call imprimir_char
    push es
    push di
    push bx
    mov ax, [cs:tmp_idx_ver]
    mov bx, 42
    mul bx
    mov di, ax
    mov bx, 0x1000
    mov es, bx
    mov al, [es:di+36]
    mov ah, [es:di+37]
    mov bl, [es:di+38]
    call imprimir_fecha_dma
    mov al, ' '
    call imprimir_char
    mov al, [es:di+39]
    mov ah, [es:di+40]
    mov bl, [es:di+41]
    call imprimir_fecha_dma
    pop bx
    pop di
    pop es
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
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ay2
    ; ruta (absoluta, relativa, .., /, .). Copiar primer token a buffer_ruta2.
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_ruta2
    call copiar_palabra
    mov si, buffer_ruta2
    call resolver_dir
    cmp ax, 0xFFFF
    je .noex
    mov [dir_actual], ax
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
    mov di, buffer_ruta2
    call copiar_palabra
    mov si, buffer_ruta2
    call descomponer_ruta   ; AX=padre, base en buffer_nombre
    cmp ax, 0xFFFF
    je .noruta
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .falta2
    call validar_nombre
    cmp ax, 1
    je .largo
    cmp ax, 2
    je .malo
    ; rechazar . y .. como base
    mov si, buffer_nombre
    mov di, opt_punto
    call cadenas_iguales
    cmp ax, 0
    je .malo
    mov si, buffer_nombre
    mov di, opt_padre
    call cadenas_iguales
    cmp ax, 0
    je .malo
    mov ax, [cs:tmp_path_padre]
    mov si, buffer_nombre
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
    call imprimir_error
    jmp .fin3
.noruta: mov si, msg_ruta_mala
    call imprimir_error
    jmp .fin3
.largo:     mov si, msg_nombre_largo
    call imprimir_error
    jmp .fin3
.malo: mov si, msg_nombre_malo
    call imprimir_error
    jmp .fin3
.existe: mov si, msg_ya_existe
    call imprimir_error
    jmp .fin3
.lleno: mov si, msg_sin_espacio
    call imprimir_error
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
    mov di, buffer_ruta2
    call copiar_palabra
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .norutab
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .falta2b
    call validar_nombre
    cmp ax, 1
    je .largob
    cmp ax, 2
    je .malob
    mov si, buffer_nombre
    mov di, opt_punto
    call cadenas_iguales
    cmp ax, 0
    je .malob
    mov si, buffer_nombre
    mov di, opt_padre
    call cadenas_iguales
    cmp ax, 0
    je .malob
    mov ax, [cs:tmp_path_padre]
    mov si, buffer_nombre
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
    call imprimir_error
    jmp .fin3b
.norutab: mov si, msg_ruta_mala
    call imprimir_error
    jmp .fin3b
.largob:     mov si, msg_nombre_largo
    call imprimir_error
    jmp .fin3b
.malob: mov si, msg_nombre_malo
    call imprimir_error
    jmp .fin3b
.existeb: mov si, msg_ya_existe
    call imprimir_error
    jmp .fin3b
.llenob: mov si, msg_sin_espacio
    call imprimir_error
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
    ; extraer ruta (puede contener /)
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_ruta2
    call copiar_palabra
    mov si, buffer_ruta2
    call descomponer_ruta   ; AX=padre, base en buffer_nombre
    cmp ax, 0xFFFF
    je .noe_ruta
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .faltat
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
    ; buscar archivo (base en padre de la ruta)
    mov si, buffer_nombre
    mov ax, [cs:tmp_path_padre]
    call fs_buscar
    cmp ax, 0xFFFF
    je .noe
    mov bx, ax
    mov [cs:tmp_touch_idx], bx
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
    mov bx, 42
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
    push ax
    mov ax, [cs:tmp_touch_idx]
    call tocar_mod
    pop ax
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
    mov bx, 42
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    pop bx
    pop ax
    ; AX=cx? cuidado pila: push cx, push bx -> pop bx=bx, pop ax=cx. Bien pero invertimos?
    ; arriba: push cx, push bx, luego ... pop bx (bx), pop ax (cx). Correcto: AX=tam
    mov [es:di+34], ax
    push ax
    push bx
    mov ax, bx
    call tocar_mod
    pop bx
    pop ax
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
.noe_ruta:
    mov si, msg_ruta_mala
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
    mov di, buffer_ruta2
    call copiar_palabra
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .noel_ruta
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .faltal
    mov ax, [cs:tmp_path_padre]
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
.noel_ruta:
    mov si, msg_ruta_mala
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
    mov di, buffer_ruta2
    call copiar_palabra
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .noeb_ruta
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .faltab
    mov ax, [cs:tmp_path_padre]
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
.noeb_ruta:
    mov si, msg_ruta_mala
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
    ; copiar primer token a ruta (puede llevar /)
    mov si, buffer_args
    call saltar_espacios
    mov di, buffer_ruta2
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
    ; si ruta es --ayuda?
    mov si, buffer_ruta2
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .ayc
    ; si ruta es --forzar (orden inverso)? soportar? No: pedir nombre.
    mov si, buffer_ruta2
    mov di, opt_forzar
    call cadenas_iguales
    cmp ax, 0
    je .faltac
    ; descomponer ruta -> padre + base (o dir directo si base vacia)
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .noec_ruta
    mov [cs:tmp_path_padre], ax
    cmp byte [buffer_nombre], 0
    je .doc_dir_directo
    mov si, buffer_nombre
    mov ax, [cs:tmp_path_padre]
    call fs_buscar
    cmp ax, 0xFFFF
    je .noec
    cmp ax, 0
    je .noraiz
    jmp .doc_check
.doc_dir_directo:
    mov si, buffer_ruta2
    call resolver_dir
    cmp ax, 0xFFFF
    je .noec
    cmp ax, 0
    je .noraiz2
.doc_check:
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
.noec_ruta:
    mov si, msg_ruta_mala
    call imprimir_error
    jmp .finc
.noraiz:
    mov si, msg_no_borrar_raiz
    call imprimir_error
    jmp .finc
.noraiz2:
    mov si, msg_no_borrar_raiz
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
    cmp byte [cs:video_graf], 0
    je .lim_txt
    call video_clear
    jmp .lim_fin
.lim_txt:
    mov ax, 0x0003
    int 0x10
.lim_fin:
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
    int 0x1A        ; real: DH=mes DL=dia (se muestra DD/MM espanol)
    ; dia (DL)
    mov al, dl
    call imprimir_bcd
    mov al, '/'
    call imprimir_char
    mov al, dh
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

; ---------- fechas v1.2 (D,M,A binarios; 0 = desconocida) ----------
tmp_fecha_d db 0
tmp_fecha_m db 0
tmp_fecha_a db 0
; bcd_a_bin AL(BCD) -> AL(bin). Preserva BX,CX,DX,SI,DI,ES.
bcd_a_bin:
    push bx
    mov bl, al
    shr al, 4
    mov bh, 10
    mul bh              ; AX = decena*10 (AL, AH=0)
    mov bh, al
    mov al, bl
    and al, 0x0F
    add al, bh
    pop bx
    ret
; fecha_hoy -> DH=dia DL=mes CL=ano(0-99) binarios, formato espanol DD/MM.
; El BIOS entrega DH=mes DL=dia (comprobado en QEMU/VBox): se corrige aqui.
; Preserva BX,SI,DI,ES. Clobbera AX,CX,DX.
fecha_hoy:
    push bx
    mov ah, 0x04
    int 0x1A            ; DH=mes DL=dia CL=ano (BCD)
    mov al, dl
    call bcd_a_bin
    mov [cs:tmp_fecha_d], al
    mov al, dh
    call bcd_a_bin
    mov [cs:tmp_fecha_m], al
    mov al, cl
    call bcd_a_bin
    mov [cs:tmp_fecha_a], al
    mov dh, [cs:tmp_fecha_d]
    mov dl, [cs:tmp_fecha_m]
    mov cl, [cs:tmp_fecha_a]
    pop bx
    ret
; estampar entrada AX=idx con cre=mod=hoy. Preserva todo.
estampar_nueva:
    pusha
    push es
    mov bx, ax          ; BX=idx
    call fecha_hoy      ; DH DL CL
    mov [cs:tmp_fecha_d], dh
    mov [cs:tmp_fecha_m], dl
    mov [cs:tmp_fecha_a], cl
    mov ax, bx
    mov bx, 42
    mul bx              ; AX=idx*42
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    mov al, [cs:tmp_fecha_d]
    mov [es:di+36], al
    mov [es:di+39], al
    mov al, [cs:tmp_fecha_m]
    mov [es:di+37], al
    mov [es:di+40], al
    mov al, [cs:tmp_fecha_a]
    mov [es:di+38], al
    mov [es:di+41], al
    pop es
    popa
    ret
; tocar_mod AX=idx pone mod=hoy. Preserva todo.
tocar_mod:
    pusha
    push es
    mov bx, ax
    call fecha_hoy
    mov [cs:tmp_fecha_d], dh
    mov [cs:tmp_fecha_m], dl
    mov [cs:tmp_fecha_a], cl
    mov ax, bx
    mov bx, 42
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    mov al, [cs:tmp_fecha_d]
    mov [es:di+39], al
    mov al, [cs:tmp_fecha_m]
    mov [es:di+40], al
    mov al, [cs:tmp_fecha_a]
    mov [es:di+41], al
    pop es
    popa
    ret
; imprimir fecha D=AL M=AH A=BL como DD/MM/AA (o --/--/-- si A=0 y D=0). Usa imprimir_char.
imprimir_fecha_dma:
    pusha
    cmp bl, 0
    jne .fd_num
    cmp al, 0
    jne .fd_num
    mov al, '-'
    call imprimir_char
    call imprimir_char
    mov al, '/'
    call imprimir_char
    mov al, '-'
    call imprimir_char
    call imprimir_char
    mov al, '/'
    call imprimir_char
    mov al, '-'
    call imprimir_char
    call imprimir_char
    jmp .fd_fin
.fd_num:
    push ax
    push bx
    mov bl, al
    mov al, bl
    xor ah, ah
    mov bh, 10
    div bh              ; AL=dec AH=uni
    add al, '0'
    call imprimir_char
    mov al, ah
    add al, '0'
    call imprimir_char
    pop bx
    pop ax
    push ax
    push bx
    mov al, '/'
    call imprimir_char
    pop bx
    pop ax
    push ax
    push bx
    mov al, ah
    xor ah, ah
    mov bh, 10
    div bh
    add al, '0'
    call imprimir_char
    mov al, ah
    add al, '0'
    call imprimir_char
    pop bx
    pop ax
    push ax
    push bx
    mov al, '/'
    call imprimir_char
    pop bx
    pop ax
    mov al, bl
    xor ah, ah
    mov bh, 10
    div bh
    add al, '0'
    call imprimir_char
    mov al, ah
    add al, '0'
    call imprimir_char
.fd_fin:
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

; ---------- resolucion ----------
; resolucion | resolucion 80x25 | resolucion ANCHOxALTO
h_resolucion:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .res_listar
    mov di, opt_ayuda
    call cadenas_iguales
    cmp ax, 0
    je .res_ayuda
    ; comparar con 80x25?
    mov si, buffer_args
    call saltar_espacios
    mov di, .s_txt
    call cadenas_iguales
    cmp ax, 0
    je .res_texto
    ; parsear ANCHO x ALTO
    mov si, buffer_args
    call saltar_espacios
    xor ax, ax
    mov [cs:tmp_res_w], ax
    mov [cs:tmp_res_h], ax
.rs_num1:
    mov al, [si]
    cmp al, 'x'
    je .rs_sepx
    cmp al, 'X'
    je .rs_sepx
    cmp al, 0
    je .rs_mal
    cmp al, ' '
    je .rs_mal
    sub al, '0'
    cmp al, 9
    ja .rs_mal
    xor ah, ah
    push ax
    mov ax, [cs:tmp_res_w]
    mov bx, 10
    mul bx
    pop bx
    add ax, bx
    cmp ax, 4000
    ja .rs_mal
    mov [cs:tmp_res_w], ax
    inc si
    jmp .rs_num1
.rs_sepx:
    inc si
.rs_num2:
    mov al, [si]
    cmp al, 0
    je .rs_go
    cmp al, ' '
    je .rs_finnum
    sub al, '0'
    cmp al, 9
    ja .rs_mal
    xor ah, ah
    push ax
    mov ax, [cs:tmp_res_h]
    mov bx, 10
    mul bx
    pop bx
    add ax, bx
    cmp ax, 3000
    ja .rs_mal
    mov [cs:tmp_res_h], ax
    inc si
    jmp .rs_num2
.rs_finnum:
    call saltar_espacios
    cmp byte [si], 0
    jne .rs_mal
.rs_go:
    mov ax, [cs:tmp_res_w]
    cmp ax, 640
    jb .rs_mal
    cmp ax, 2048
    ja .rs_mal
    mov bx, [cs:tmp_res_h]
    cmp bx, 400
    jb .rs_mal
    call vbe_buscar      ; AX=W BX=H -> AX ok CX=modo
    cmp ax, 0
    je .rs_nomod
    call vbe_fijar
    cmp ax, 0
    je .rs_nomod
    mov si, msg_res_ok
    call imprimir_ok
    jmp .res_fin
.rs_nomod:
    mov si, msg_res_mal
    call imprimir_error
    jmp .res_fin
.rs_mal:
    mov si, msg_desc_resolucion
    call imprimir_error
    jmp .res_fin
.res_texto:
    call video_texto
    mov si, msg_res_txt
    call imprimir_ok
    jmp .res_fin
.res_ayuda:
    mov si, msg_desc_resolucion
    call imprimir
    jmp .res_fin
.res_listar:
    mov si, msg_res_lista
    call imprimir
    call vbe_info
    cmp ax, 0
    je .res_novbe
    ; mostrar actual
    mov si, msg_res_ahora
    call imprimir
    cmp byte [cs:video_graf], 0
    jne .res_cur_g
    mov si, .s_txt
    call imprimir
    call nueva_linea
    jmp .res_modos
.res_cur_g:
    mov ax, [cs:video_w]
    call imprimir_numero
    mov al, 'x'
    call imprimir_char
    mov ax, [cs:video_h]
    call imprimir_numero
    call nueva_linea
.res_modos:
    ; recorrer lista e imprimir WxH unicos (max 24)
    mov ax, 0x7000
    mov es, ax
    mov si, [es:14]
    mov dx, [es:16]
    push ds
    mov ds, dx
    xor bx, bx           ; mostrados
    mov word [cs:.res_lw], 0
    mov word [cs:.res_lh], 0
.res_ml:
    mov cx, [si]
    cmp cx, 0xFFFF
    je .res_mfin
    cmp bx, 24
    jae .res_mfin
    push si
    push bx
    push cx
    push ds
    call vbe_modo_info
    pop ds
    pop cx
    pop bx
    pop si
    cmp ax, 0
    je .res_mnext
    push es
    push si
    push bx
    push cx
    mov ax, 0x7000
    mov es, ax
    mov dx, [es:0x200+18]
    cmp dx, 640
    jb .res_mpop
    mov ax, [es:0x200+20]
    cmp ax, 400
    jb .res_mpop
    mov al, [es:0x200+25]
    cmp al, 16
    jb .res_mpop
    cmp dx, [cs:.res_lw]
    jne .res_nodup
    mov ax, [es:0x200+20]
    cmp ax, [cs:.res_lh]
    je .res_mpop
.res_nodup:
    mov [cs:.res_lw], dx
    mov ax, [es:0x200+20]
    mov [cs:.res_lh], ax
    push dx
    mov ax, dx
    call imprimir_numero
    mov al, 'x'
    call imprimir_char
    pop dx
    mov ax, [es:0x200+20]
    call imprimir_numero
    mov al, 'x'
    call imprimir_char
    movzx ax, byte [es:0x200+25]
    call imprimir_numero
    call nueva_linea
    pop cx
    pop bx
    pop si
    pop es
    inc bx
    add si, 2
    jmp .res_ml
.res_mpop:
    pop cx
    pop bx
    pop si
    pop es
.res_mnext:
    add si, 2
    jmp .res_ml
.res_mfin:
    pop ds
    mov si, msg_res_ej
    call imprimir
    jmp .res_fin
.res_lw dw 0
.res_lh dw 0
.res_novbe:
    mov si, msg_res_novbe
    call imprimir_aviso
.res_fin:
    popa
    ret
.s_txt db '80x25',0

h_teclado:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .tec_mostrar
    mov di, opt_es
    call cadenas_iguales
    cmp ax, 0
    je .tec_es
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_espanol
    call cadenas_iguales
    cmp ax, 0
    je .tec_es
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_us
    call cadenas_iguales
    cmp ax, 0
    je .tec_us
    mov si, buffer_args
    call saltar_espacios
    mov di, opt_americano
    call cadenas_iguales
    cmp ax, 0
    je .tec_us
    mov si, msg_desc_teclado
    call imprimir_error
    jmp .tec_fin
.tec_mostrar:
    cmp byte [cs:teclado_es], 0
    je .tec_mus
    mov si, msg_tec_ahora_es
    call imprimir
    jmp .tec_fin
.tec_mus:
    mov si, msg_tec_ahora_us
    call imprimir
    jmp .tec_fin
.tec_es:
    mov byte [cs:teclado_es], 1
    mov si, msg_tec_es
    call imprimir_ok
    call autosave
    jmp .tec_fin
.tec_us:
    mov byte [cs:teclado_es], 0
    mov si, msg_tec_us
    call imprimir_ok
    call autosave
.tec_fin:
    popa
    ret

msg_pg_nogfx db 'Usa resolucion 800x600 antes de prueba_grafica.',13,10,0
msg_pg_tit db 'PRUEBA GRAFICA FACILOS',0
msg_pg_v13 db 'FacilOS v1.3',0
msg_pg_fbok db 'Framebuffer OK',0
msg_pg_res db 'Resolucion: ',0
msg_pg_bpp db 'BPP: ',0
msg_pg_pitch db 'Pitch: ',0
msg_pg_ok1 db 'Pixel OK',13,10,0
msg_pg_ok2 db 'Rectangulo OK',13,10,0
msg_pg_ok3 db 'Linea OK',13,10,0
msg_pg_ok4 db 'Texto OK',13,10,0
msg_pg_ok5 db 'Clip OK',13,10,0
msg_pg_borde db 'OK',0
pg_cols db 0x0C,0x0A,0x09,0x0E,0x0F,0x0B,0x0D,0x07
; prueba_grafica: test visual del renderer (requiere modo grafico).
h_prueba_grafica:
    pusha
    cmp byte [cs:video_graf], 0
    je .pg_nogfx
    mov bl, COLOR_NEGRO
    call video_limpiar
    mov byte [cs:video_cur_vis], 0
    call video_cursor_dibujar
    mov word [cs:video_cx], 0
    mov word [cs:video_cy], 0
    movzx ebp, word [cs:video_w]
    ; barra titulo
    mov ax, 0
    mov bx, 0
    mov cx, bp
    mov dx, 24
    mov si, COLOR_AZUL
    call video_rectangulo
    mov ax, 8
    mov bx, 4
    mov si, msg_pg_tit
    mov dl, COLOR_BLANCO
    call video_texto_xy
    ; cajas de color (8)
    mov ax, bp
    shr ax, 3
    mov [cs:pg_boxw], ax
    mov word [cs:pg_i], 0
    mov word [cs:pg_x], 0
.pg_cajas:
    mov ax, [cs:pg_i]
    cmp ax, 8
    jae .pg_lineas
    mov bx, ax
    mov al, [cs:pg_cols+bx]
    mov [cs:pg_col], al
    mov ax, [cs:pg_x]
    mov bx, 34
    mov cx, [cs:pg_boxw]
    sub cx, 2
    mov dx, 26
    movzx si, byte [cs:pg_col]
    call video_rectangulo
    mov ax, [cs:pg_x]
    add ax, [cs:pg_boxw]
    mov [cs:pg_x], ax
    inc word [cs:pg_i]
    jmp .pg_cajas
.pg_lineas:
    ; marco (8,70,W-16,84) gris
    mov ax, 8
    mov bx, 70
    mov cx, bp
    sub cx, 16
    mov dx, 1
    mov si, COLOR_GRIS
    call video_rectangulo
    mov ax, 8
    mov bx, 153
    mov cx, bp
    sub cx, 16
    mov dx, 1
    mov si, COLOR_GRIS
    call video_rectangulo
    mov ax, 8
    mov bx, 70
    mov cx, 1
    mov dx, 84
    mov si, COLOR_GRIS
    call video_rectangulo
    mov ax, bp
    sub ax, 9
    mov bx, 70
    mov cx, 1
    mov dx, 84
    mov si, COLOR_GRIS
    call video_rectangulo
    mov ax, bp
    sub ax, 16
    mov [cs:pg_x2], ax
    ; horizontal roja (16,110)-(x2,110)
    mov ax, 16
    mov bx, 110
    mov cx, [cs:pg_x2]
    mov dx, 110
    mov si, COLOR_ROJO
    call video_linea
    ; vertical verde (W/2,78)-(W/2,146)
    mov ax, bp
    shr ax, 1
    mov bx, 78
    mov cx, ax
    mov dx, 146
    mov si, COLOR_VERDE
    call video_linea
    ; diagonal amarilla (16,78)-(x2,146)
    mov ax, 16
    mov bx, 78
    mov cx, [cs:pg_x2]
    mov dx, 146
    mov si, COLOR_AMARILLO
    call video_linea
    ; diagonal cyan (x2,78)-(16,146)
    mov ax, [cs:pg_x2]
    mov bx, 78
    mov cx, 16
    mov dx, 146
    mov si, COLOR_CYAN
    call video_linea
    ; puntos (fila y=162, paso 32)
    mov word [cs:pg_x], 16
    xor cx, cx
.pg_puntos:
    mov ax, [cs:pg_x]
    mov bx, [cs:pg_x2]
    cmp ax, bx
    jae .pg_textos
    mov bx, 162
    test cx, 1
    jz .pg_dot0
    mov dl, COLOR_GRIS
    jmp .pg_dot1
.pg_dot0:
    mov dl, COLOR_BLANCO
.pg_dot1:
    push cx
    call video_pixel
    pop cx
    mov ax, [cs:pg_x]
    add ax, 32
    mov [cs:pg_x], ax
    inc cx
    jmp .pg_puntos
.pg_textos:
    mov ax, 16
    mov bx, 172
    mov si, msg_pg_v13
    mov dl, COLOR_BLANCO
    call video_texto_xy
    mov ax, 16
    mov bx, 190
    mov si, msg_pg_fbok
    mov dl, COLOR_VERDE_CLARO
    call video_texto_xy
    ; sondas clipping (recorte real, sin corrupcion)
    mov ax, -20
    mov bx, 216
    mov cx, 60
    mov dx, 20
    mov si, COLOR_ROJO
    call video_rectangulo
    mov ax, -30
    mov bx, 236
    mov cx, 200
    mov dx, 256
    mov si, COLOR_VERDE
    call video_linea
    mov ax, 9999
    mov bx, 9999
    mov dl, COLOR_BLANCO
    call video_pixel
    mov ax, [cs:video_w]
    sub ax, 4
    mov bx, 216
    mov si, msg_pg_borde
    mov dl, COLOR_AMARILLO
    call video_texto_xy
    ; bajar cursor putc bajo el dibujo (260px -> 18 lineas)
    mov cx, 18
.pg_nls:
    push cx
    mov si, msg_crlf
    call imprimir
    pop cx
    loop .pg_nls
    mov si, msg_pg_res
    call imprimir
    mov ax, [cs:video_w]
    call imprimir_numero
    mov al, 'x'
    call imprimir_char
    mov ax, [cs:video_h]
    call imprimir_numero
    mov si, msg_crlf
    call imprimir
    mov si, msg_pg_bpp
    call imprimir
    movzx ax, byte [cs:video_bpp]
    call imprimir_numero
    mov si, msg_crlf
    call imprimir
    mov si, msg_pg_pitch
    call imprimir
    mov ax, [cs:video_pitch]
    call imprimir_numero
    mov si, msg_crlf
    call imprimir
    mov si, msg_pg_ok1
    call imprimir
    mov si, msg_pg_ok2
    call imprimir
    mov si, msg_pg_ok3
    call imprimir
    mov si, msg_pg_ok4
    call imprimir
    mov si, msg_pg_ok5
    call imprimir
    jmp .pg_fin
.pg_nogfx:
    mov si, msg_pg_nogfx
    call imprimir_error
.pg_fin:
    popa
    ret
pg_boxw dw 0
pg_x dw 0
pg_x2 dw 0
pg_col db 0
pg_i dw 0

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
    ; origen con ruta -> padre_o + base_o
    mov si, buffer_nombre
    mov di, buffer_ruta2
    call copiar_cadena
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .cp_noex
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .cp_falta
    mov ax, [cs:tmp_path_padre]
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
    ; destino: probar como carpeta existente (ruta completa)
    mov si, buffer_nombre2
    mov di, buffer_ruta2
    call copiar_cadena
    mov si, buffer_ruta2
    call resolver_dir
    cmp ax, 0xFFFF
    je .cp_dest_nuevo
    ; es carpeta DX: copiar dentro con mismo nombre base origen
    mov dx, ax
    push ax
    call fs_leer_tipo
    pop dx
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
.cp_dest_nuevo:
    ; destino = ruta nueva: descomponer -> padre_d + base_d
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .cp_noex
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .cp_falta
    call validar_nombre
    cmp ax, 0
    jne .cp_malo
    mov si, buffer_nombre
    mov di, opt_punto
    call cadenas_iguales
    cmp ax, 0
    je .cp_malo
    mov si, buffer_nombre
    mov di, opt_padre
    call cadenas_iguales
    cmp ax, 0
    je .cp_malo
    mov si, buffer_nombre
    mov ax, [cs:tmp_path_padre]
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
    mov bx, 42
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    mov ax, [cs:tmp_cp_tam]
    mov [es:di+34], ax
    push ax
    mov ax, [cs:tmp_cp_dest]
    call tocar_mod
    pop ax
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
    ; origen con ruta
    mov si, buffer_nombre
    mov di, buffer_ruta2
    call copiar_cadena
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .rn_noex
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .rn_falta
    mov ax, [cs:tmp_path_padre]
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
    mov [cs:tmp_touch_idx], bx
    mov ax, [cs:tmp_path_padre]
    mov [cs:tmp_ren_padre], ax
    ; destino: simple -> mismo padre que origen; con / -> debe quedar en mismo padre
    mov si, buffer_nombre2
    call tiene_barra
    cmp ax, 0
    jne .rn_con_ruta
    ; simple: base_d = token tal cual, padre_d = padre_o
    mov si, buffer_nombre2
    cmp byte [si], 0
    je .rn_falta
    mov di, buffer_nombre
    call copiar_cadena      ; base_d a buffer_nombre
    mov ax, [cs:tmp_ren_padre]
    push ax
    jmp .rn_val_nombre
.rn_con_ruta:
    mov si, buffer_nombre2
    mov di, buffer_ruta2
    call copiar_cadena
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .rn_noex
    push ax                 ; padre_d
.rn_val_nombre:
    mov si, buffer_nombre
    cmp byte [si], 0
    je .rn_falta2
    call validar_nombre
    cmp ax, 0
    jne .rn_malo
    mov si, buffer_nombre
    mov di, opt_punto
    call cadenas_iguales
    cmp ax, 0
    je .rn_malo
    mov si, buffer_nombre
    mov di, opt_padre
    call cadenas_iguales
    cmp ax, 0
    je .rn_malo
    pop ax                  ; AX=padre_d
    push ax
    mov cx, [cs:tmp_path_padre]
    cmp ax, cx
    pop ax
    jne .rn_distinto
    mov si, buffer_nombre
    call fs_buscar
    cmp ax, 0xFFFF
    jne .rn_existe
    ; renombrar: copiar nombre nuevo sobre entrada
    mov ax, [cs:tmp_touch_idx]
    mov bx, 42
    push bx
    mov bx, ax
    mov ax, bx
    pop bx
    mul bx
    mov di, ax
    add di, 2
    mov ax, 0x1000
    mov es, ax
    mov si, buffer_nombre
.rn_cop:
    lodsb
    mov [es:di], al
    inc di
    or al, al
    jnz .rn_cop
    mov ax, [cs:tmp_touch_idx]
    call tocar_mod
    mov si, msg_renombrado
    call imprimir_ok
    call autosave
    jmp .rn_fin
.rn_falta2:
    pop ax
.rn_distinto:
    mov si, msg_ruta_mala
    call imprimir_error
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
    ; origen con ruta -> idx_o (debe ser archivo)
    mov si, buffer_nombre
    mov di, buffer_ruta2
    call copiar_cadena
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .mv_noex
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .mv_falta
    mov ax, [cs:tmp_path_padre]
    call fs_buscar
    cmp ax, 0xFFFF
    je .mv_noex
    mov bx, ax
    mov [cs:tmp_touch_idx], bx
    push bx
    mov ax, bx
    call fs_leer_tipo
    pop bx
    cmp al, 2
    jne .mv_noarch
    push bx
    mov ax, bx
    call fs_leer_padre
    mov [cs:tmp_mv_opadre], ax
    pop bx
    ; destino: probar como carpeta existente (ruta completa)
    mov si, buffer_nombre2
    mov di, buffer_ruta2
    call copiar_cadena
    mov si, buffer_ruta2
    call resolver_dir
    cmp ax, 0xFFFF
    je .mv_dest_nuevo
    mov dx, ax
    push ax
    call fs_leer_tipo
    pop dx
    push dx
    cmp al, 1
    pop dx
    jne .mv_existe
    ; mover a carpeta DX con mismo nombre base origen
    push bx
    push dx
    mov si, buffer_nombre
    mov ax, dx
    call fs_buscar
    pop dx
    pop bx
    cmp ax, 0xFFFF
    jne .mv_existe
    mov ax, [cs:tmp_touch_idx]
    mov bx, 42
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
    mov ax, [cs:tmp_touch_idx]
    call tocar_mod
    mov si, msg_movido
    call imprimir_ok
    call autosave
    jmp .mv_fin
.mv_dest_nuevo:
    ; destino simple -> mismo padre que origen; con / -> ruta nueva
    mov si, buffer_nombre2
    call tiene_barra
    cmp ax, 0
    jne .mv_con_ruta
    mov si, buffer_nombre2
    cmp byte [si], 0
    je .mv_falta
    mov di, buffer_nombre
    call copiar_cadena
    mov ax, [cs:tmp_mv_opadre]
    mov [cs:tmp_path_padre], ax
    jmp .mv_val_base
.mv_con_ruta:
    ; destino = ruta nueva -> padre_d + base_d
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .mv_noex
    mov [cs:tmp_path_padre], ax
.mv_val_base:
    mov si, buffer_nombre
    cmp byte [si], 0
    je .mv_falta
    call validar_nombre
    cmp ax, 0
    jne .mv_malo
    mov si, buffer_nombre
    mov di, opt_punto
    call cadenas_iguales
    cmp ax, 0
    je .mv_malo
    mov si, buffer_nombre
    mov di, opt_padre
    call cadenas_iguales
    cmp ax, 0
    je .mv_malo
    ; choque en destino?
    mov si, buffer_nombre
    mov ax, [cs:tmp_path_padre]
    call fs_buscar
    cmp ax, 0xFFFF
    jne .mv_existe
    ; aplicar padre+nombre a idx origen
    mov ax, [cs:tmp_touch_idx]
    mov bx, 42
    push bx
    mov bx, ax
    mov ax, bx
    pop bx
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    mov al, [cs:tmp_path_padre]
    mov [es:di+1], al
    add di, 2
    mov si, buffer_nombre
.mv_cop2:
    lodsb
    mov [es:di], al
    inc di
    or al, al
    jnz .mv_cop2
    mov ax, [cs:tmp_touch_idx]
    call tocar_mod
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

tmp_bs_file dw 0
tmp_bs_patlen dw 0
tmp_bs_tam dw 0
tmp_bs_total dw 0
tmp_bs_dir dw 0
msg_bs_cab db 'Coincidencias: ',0
msg_bs_dosp db ': ',0
; buscar patron (buffer_texto) en archivo AX -> AX=pos o FFFF. Preserva BX,CX,DX,SI,DI,ES.
buscar_patron_en_archivo:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov [cs:tmp_bs_file], ax
    mov si, buffer_texto
    call longitud
    mov [cs:tmp_bs_patlen], cx
    mov ax, [cs:tmp_bs_file]
    call fs_leer_tam
    mov [cs:tmp_bs_tam], ax
    mov cx, [cs:tmp_bs_patlen]
    cmp cx, 0
    je .bpa_no
    cmp cx, ax
    ja .bpa_no
    xor bx, bx             ; pos
.bpa_pos:
    mov ax, [cs:tmp_bs_tam]
    sub ax, cx
    cmp bx, ax
    ja .bpa_no
    xor si, si             ; offset patron
.bpa_cmp:
    cmp si, cx
    jae .bpa_si
    push bx
    push cx
    push si
    mov ax, [cs:tmp_bs_file]
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
    jne .bpa_next
    inc si
    jmp .bpa_cmp
.bpa_si:
    mov ax, bx
    jmp .bpa_fin
.bpa_next:
    inc bx
    jmp .bpa_pos
.bpa_no:
    mov ax, 0xFFFF
.bpa_fin:
    mov bx, ax
    pop es
    pop di
    pop si
    pop dx
    pop cx
    mov ax, bx
    pop bx
    ret
; imprimir ruta completa de AX=idx (ej. /documentos/nota). Preserva todo.
imprimir_ruta_completa:
    pusha
    push es
    cmp ax, 0
    je .irc_raiz
    mov bx, ax
    xor cx, cx
.irc_up:
    mov di, pila_ruta
    mov ax, cx
    shl ax, 1
    add di, ax
    mov [di], bx
    inc cx
    cmp cx, 64
    jae .irc_down
    mov ax, bx
    call fs_leer_padre
    mov bx, ax
    cmp bx, 0
    je .irc_down
    jmp .irc_up
.irc_down:
    cmp cx, 0
    je .irc_fin
    dec cx
    mov ax, cx
    shl ax, 1
    mov bx, pila_ruta
    add bx, ax
    mov ax, [bx]
    push cx
    mov di, buffer_tmp_nombre
    call fs_copiar_nombre
    mov al, '/'
    call imprimir_char
    mov si, buffer_tmp_nombre
    call imprimir_blanco
    pop cx
    jmp .irc_down
.irc_fin:
    pop es
    popa
    ret
.irc_raiz:
    mov si, msg_barra
    call imprimir_azul
    pop es
    popa
    ret
; buscar recursivo en dir AX (imprime coincidencias, cuenta en tmp_bs_total)
buscar_dir_rec:
    pusha
    mov [cs:tmp_bs_dir], ax
    xor dx, dx             ; idx 0..63
.bdr_rep:
    cmp dx, 64
    jae .bdr_fin
    push dx
    mov ax, dx
    call fs_leer_tipo
    pop dx
    or al, al
    jz .bdr_next
    cmp dx, [cs:tmp_bs_dir]
    je .bdr_next           ; no listar el propio dir
    push dx
    push ax
    mov ax, dx
    call fs_leer_padre
    pop cx                 ; CX=tipo? No: pila ax,tipo? Rehacer: guardamos tipo en BL
    pop dx
    push dx
    push ax
    mov ax, dx
    call fs_leer_tipo
    mov bl, al
    pop ax                 ; AX=padre
    pop dx
    cmp ax, [cs:tmp_bs_dir]
    jne .bdr_next
    cmp bl, 2
    je .bdr_arch
    cmp bl, 1
    jne .bdr_next
    ; subcarpeta: recursar
    push dx
    mov ax, dx
    call buscar_dir_rec
    pop dx
    jmp .bdr_next
.bdr_arch:
    push dx
    mov ax, dx
    call buscar_patron_en_archivo
    pop dx
    cmp ax, 0xFFFF
    je .bdr_next
    push ax
    push dx
    mov ax, dx
    call imprimir_ruta_completa
    mov si, msg_bs_dosp
    call imprimir
    pop dx
    pop ax
    push ax
    push dx
    call imprimir_numero
    call nueva_linea
    pop dx
    pop ax
    inc word [cs:tmp_bs_total]
.bdr_next:
    inc dx
    jmp .bdr_rep
.bdr_fin:
    popa
    ret

; ---------- buscar texto ----------
h_buscar_texto:
    pusha
    mov si, buffer_args
    call saltar_espacios
    cmp byte [si], 0
    je .bs_falta
    mov di, buffer_ruta2
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
    ; objetivo: primero probar carpeta (ruta intacta), si no, archivo
    mov si, buffer_ruta2
    call resolver_dir
    cmp ax, 0xFFFF
    je .bs_no_dir
    ; es carpeta: buscar recursivo en ella
    jmp .bs_es_dir
.bs_no_dir:
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .bs_noex_ruta
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .bs_falta
    jmp .bs_como_archivo
.bs_es_dir:
    mov word [cs:tmp_bs_total], 0
    call buscar_dir_rec
    cmp word [cs:tmp_bs_total], 0
    je .bs_no
    mov si, msg_bs_cab
    call imprimir_ok
    mov ax, [cs:tmp_bs_total]
    call imprimir_numero
    call nueva_linea
    jmp .bs_fin
.bs_como_archivo:
    mov si, buffer_nombre
    mov ax, [cs:tmp_path_padre]
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
    mov ax, bx
    call buscar_patron_en_archivo
    cmp ax, 0xFFFF
    je .bs_no
    mov si, msg_encontrado
    call imprimir_ok
    call imprimir_numero
    call nueva_linea
    jmp .bs_fin
.bs_noex_ruta:
    mov si, msg_ruta_mala
    call imprimir_error
    jmp .bs_fin
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
    mov di, buffer_ruta2
    call copiar_palabra
    mov si, buffer_ruta2
    call descomponer_ruta
    cmp ax, 0xFFFF
    je .ed_ruta
    mov [cs:tmp_path_padre], ax
    mov si, buffer_nombre
    cmp byte [si], 0
    je .ed_falta
    mov ax, [cs:tmp_path_padre]
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
    mov di, opt_punto
    call cadenas_iguales
    cmp ax, 0
    je .ed_malo
    mov si, buffer_nombre
    mov di, opt_padre
    call cadenas_iguales
    cmp ax, 0
    je .ed_malo
    mov si, buffer_nombre
    mov ax, [cs:tmp_path_padre]
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
    mov bx, 42
    mul bx
    mov di, ax
    mov ax, 0x1000
    mov es, ax
    pop cx
    pop bx
    mov ax, cx
    mov [es:di+34], ax
    push ax
    push bx
    push cx
    mov ax, bx
    call tocar_mod
    pop cx
    pop bx
    pop ax
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
.ed_ruta:
    mov si, msg_ruta_mala
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
    call traducir_es
    cmp al, 13
    je .le_enter
    cmp al, 8
    je .le_back
    cmp cx, 120
    jae .le_rep
    call eco_putc
    mov [di], al
    inc di
    inc cx
    jmp .le_rep
.le_back:
    cmp cx, 0
    je .le_rep
    dec di
    dec cx
    call eco_backspace
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
tmp_esp_arch dw 0
tmp_esp_dir dw 0
h_espacio:
    pusha
    call disco_tiene_lba
    cmp ax, 1
    jne .esp_nodisco
    mov si, msg_espacio_tit
    call imprimir
    mov si, msg_espacio_total
    call imprimir
    mov word [cs:tmp_esp_ocup], 0
    mov word [cs:tmp_esp_lib], 0
    mov word [cs:tmp_esp_bytes], 0
    mov word [cs:tmp_esp_arch], 0
    mov word [cs:tmp_esp_dir], 0
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
    je .esp_esarch
    inc word [cs:tmp_esp_dir]
    jmp .esp_next
.esp_esarch:
    inc word [cs:tmp_esp_arch]
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
    mov si, msg_espacio_arch
    call imprimir
    mov ax, [cs:tmp_esp_arch]
    call imprimir_numero
    call nueva_linea
    mov si, msg_espacio_dir
    call imprimir
    mov ax, [cs:tmp_esp_dir]
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
