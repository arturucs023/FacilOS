# INFORME-RATON-ETAPA4 (E4: driver + hallazgo IOAPIC)

Estado: IMPLEMENTADO + PROBADO (E4 cerrada en `d8cfab8`, reverificado E6).

## Driver (`ps2.c`, IRQ12, paquetes 3 B, cola + bloqueo, syscalls 38/39)
Init con `cli` (la IRQ1 robaba respuestas), CTR bit1+bit5, reset FA/AA,
F6/F4 con ACK. ABI anterior intacto.

## Hallazgo (evidencia: `info pic` + contadores)
El i8042 señaliza (IRQ12×6 en PIC) pero la CPU nunca vectoriza: ni IRQ12,
ni IRQ2, ni RTC/IRQ8 (segundo testigo). Máscara 0xEF correcta, puerta
IDT44 verificada byte a byte, re-init tardío inútil. Diagnóstico: la
cascada del i8259 no entrega el esclavo en este QEMU (con 8259 solo
funcionan IRQ0/1).
Solución mínima: GSI12 por IOAPIC (pin 12→vector 44) + EOI al LAPIC
(`apic.c`, mapeo en `pag_inicia`, fallback i8259 sin APIC). IRQ0/1
siguen por i8259.

## Pruebas (píxeles, logs v40_*)
Init, cursor (100 px), movimiento→coords→texto MOU (226 px), recorte
12px, clic→foco (títulos), rueda sin rueda (3 B, documentado), shell viva,
sin PANIC. E6: clic/drag/resize sobre el escritorio.
