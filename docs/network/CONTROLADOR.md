# Controlador RTL8139 (F1)

Dispositivo: 10EC:8139 en PCI bus 0 (QEMU `pc/i440FX` lo cuelga
ahi; `-device rtl8139,netdev=red0,mac=52:54:00:12:34:56`).

PCI (`src/kernel32/pci.c`): mecanismo 1 (`0xCF8/0xCFC`, `inl/outl`
nuevos en `cpu.h`). Lee vendor/device, BAR0 (exige PIO; MEM no
soportado en F1), IRQ de 0x3C (informativa), activa IO+bus mastering
en CMD 0x04.

Secuencia (`drv_red.c`): CONFIG1=0 (encendido), reset CR=0x10 con
espera acotada por iteraciones (sin ticks en syscall), RBSTART al
anillo 16K alineado a 8K, RCR=APM|AB|WRAP+MXDMAmax+16K (`0xF8A`),
IMR=ROK|RER|TOK|TER, ISR limpio, CR=RE|TE.

Tx: TSAD+TSD, 1 en vuelo (flag propio), fin por TOK (no por OWN:
sale de reset con OWN=1). Timeout acotado por iteraciones.
Rx: sondeo del anillo (cabecera estado+longitud con vuelta),
valida 18..1518, quita CRC de 4 B, avanza con CAPR=(off-16).
Error RER -> repara receptor (offsets a 0). ISR se limpia por
bits (TOK/TER quien transmite, RER quien recibe) para no robar
avisos entre sondeos concurrentes.

Sin adaptador: `red_init` informa por serie y devuelve 0; el
arranque sigue igual. Todo error vuelve al llamador (-1/-2/-3).
