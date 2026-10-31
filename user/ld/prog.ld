/* Programas F32X: UNA sola seccion plana en 0x200000 (sin relleno PE).
   Entrada _start al inicio (start.o primero). BSS se queda con VMA
   (NOLOAD, sin contenido en fichero); el cargador la pone a cero
   segun cabecera (version 2). */
SECTIONS
{
    . = 0x200000;
    .text : { *(.text*) *(.rdata*) *(.data*) }
    .bss : {
        __pbss = .;
        *(.bss*)
        *(COMMON)
        __ebss = .;
    }
    /DISCARD/ : {
        *(.comment*) *(.note*) *(.eh_frame*)
        *(.reloc) *(.idata*) *(.edata*) *(.pdata*)
        *(.debug*)
    }
}
