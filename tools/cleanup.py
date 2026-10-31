#!/usr/bin/env python3
"""Limpia andamiaje de depuracion del kernel (marcadores, SP-prints, Qloops)."""
import re, pathlib
p = pathlib.Path(__file__).parent.parent / "src" / "kernel.asm"
src = p.read_text(encoding="utf-8", errors="ignore")
n0 = len(src)

def rx(pat):
    return re.compile(pat)

# 1. marcadores serial sueltos: push ax / mov al,'X' / call serial_putc / pop ax
src = rx(r"    push ax\n    mov al, '[A-Z]'\n    call serial_putc\n    pop ax\n").sub("", src)
# 2. bloques SP-print con marco bx/si/di
src = rx(r"    push ax\n    push bx\n    push si\n(?:    push di\n)?    mov bx, sp\n(?:    mov ax, \[ss:bx\+\d+\]\n    call imprimir_numero\n    mov al, ' '\n    call imprimir_char\n)+    pop (?:di\n    pop )?si\n    pop bx\n    pop ax\n").sub("", src)
# 3. bloques SP-print simples
src = rx(r"    push ax\n    mov ax, sp\n    call imprimir_numero\n    mov al, ' '\n    call imprimir_char\n    pop ax\n").sub("", src)
# 4. qloops muertos
src = rx(r"\.qloop2:\n(?:    .*\n)+?(?=\n)").sub("", src)
# 5. mensajes DBG
src = rx(r"msg_disco_eio db .*\nmsg_disco_sum db .*\nmsg_disco_val db .*\n").sub("", src)
# 6. tmp_sp_entry y su uso
src = rx(r"tmp_sp_entry dw 0\n").sub("", src)
src = rx(r"    mov \[cs:tmp_sp_entry\], sp\n").sub("", src)
src = rx(r"    mov ax, \[cs:tmp_sp_entry\]\n    call imprimir_numero\n    call nueva_linea\n").sub("", src)
src = rx(r"    mov ax, \[cs:tmp_sp_entry\]\n    call imprimir_numero\n    mov al, ' '\n    call imprimir_char\n").sub("", src)

p.write_text(src, encoding="utf-8")
print("antes:", n0, "despues:", len(src))
