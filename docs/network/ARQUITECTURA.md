# Red FacilOS32 v2.3 — Arquitectura propia (F1 implementado)

Sin sockets POSIX, sin pila prestada. Capas con nombres FacilOS:

- `drv_red` (kernel): RTL8139 por PIO, Tx con 1 descriptor en vuelo,
  anillo Rx 16K, sondeo (sin IRQ en F1), reinicio suave ante errores.
  DMA por identidad: kheap `0x800000` virt==fis (<4 GB), buffers
  contiguos de `kmalloc`; anillo alineado a 8K por software.
- Tramas Ethernet II crudas hasta Ring3 (syscalls 53/54). Sin
  filtros en F1: el llamador ve todo lo que llega a su MAC +
  difusion (APM|AB); el descarte fino es de F2+.
- L3+ (ARP/IPv4/DHCP/ICMP/UDP/TCP) vivira en el nucleo en su fase;
  servicios (DHCP/DNS/HTTP) como apps Ring3 sobre la API propia.
  F3: ICMP (eco) en el nucleo, DHCP como app Ring3 sobre trama
  cruda; ARP corregido (reinspeccion de cache + reintentos).
  F4: UDP (ligas+colas+demux) en el nucleo, DNS como app Ring3
  sobre syscalls 58-60; `sc_54` centraliza el bombeo (sirve del
  tap) para no robar tramas a la pila.
  F5: TCP (TCB por pid, colas FIFO, demux por tupla) en el nucleo,
  syscall unica 61 op-multiplexada; Ring3 habla por `u_tcp_*`.
- API `red_*` (syscalls 52-63, ver `API-FACILOS32.md`): F1 solo
  52/53/54. Toda espera acotada; ningun bloqueo indefinido.
  No hay pipes: la recepcion es por sondeo con espera maxima.
- Comandos en castellano con `_` (ver `COMANDOS.md`).

Decisiones F1: sondeo en vez de IRQ (determinismo sin rutear
PIRQ aun; IRQ en fase posterior); 1 Tx en vuelo; anillo 16K;
relleno a 60 B en driver (la 8139 no rellena); OWN=1 en reposo
tras reset -> transmision directa y confirmacion por TOK.
