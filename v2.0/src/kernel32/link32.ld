/* FacilOS32: enlazado PE (ld MinGW solo sabe PE) y luego objcopy a plano.
   .text queda EXACTAMENTE en 0x100000 para que copiar fichero[0..] a
   0x100000 deje todas las direcciones absolutas correctas.
   v2.2: se coloca explicitamente .rdata (MinGW; antes huerfana y su
   colocacion dependia del azar: rompio vfs_actual al crecer). */
SECTIONS
{
    . = 0x100000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) *(.rdata*) }
    .data : { *(.data*) *(.data.rel.ro*) *(.data.rel.ro.*) }
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    }
    /DISCARD/ : {
        *(.comment*) *(.note*) *(.eh_frame*)
        *(.reloc) *(.idata*) *(.edata*) *(.pdata*)
        *(.debug*) *(.rdata$zzz*)
    }
}
