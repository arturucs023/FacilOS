# F8: reforma integral del kernel (v2.3-red)

Informe final de la F8. Arquitectura propia; Ring 3 y ABI/syscalls intactos
salvo correcciones de errores documentadas abajo. Cierra en un unico commit
F7/F8 + push verificado.

## 1. Objetivo y alcance

- Reforma integral del kernel en `v2.3-red` sin tocar `main`.
- Eliminar el tope de 1,44 MB de la ISO: el tamano lo dicta el contenido.
- Sin regresiones en la bateria F7; sin copiar codigo de otros kernels.
- Cierre en un unico commit descriptivo + push normal; verificar
  HEAD==origin, arbol limpio, `main` y stash intactos, ISOs historicas presentes.

## 2. Auditoria previa (5 vias)

Antes de codificar se auditaron: memoria/paginacion, procesos/IPC,
IRQ/excepciones, drivers/red/NIM, arranque/FS/logging. De ahi salieron
los 5 bloques: `klog`, `hal`, IRQ centralizada, fusion diferida y NIM
minimo, mas cotas estrictas en `cd`/`fs`/`cdcat`.

## 3. `klog` (`klog.{h,c}`)

Diagnostico centralizado con buffer circular, serie con tope y niveles.
La IRQ ya no hace `kprintf` con EOI pendiente (colgaba): `proc_fusiona`
solo avisa por `klog`. Ficheros nuevos: `v2.0/src/kernel32/klog.{h,c}`.

## 4. HAL (`hal.{h,c}`)

Capa propia minima: registro IRQ con nombre, EOI PIC/IOAPIC segun ruta,
ticks, foto de arranque `0x7000` (`hal_arranque`/`hal_cat_lba`/`hal_dat_lba`),
framebuffer. Los subsistemas usan la HAL; QEMU/SeaBIOS queda aislado ahi.
Ficheros nuevos: `v2.0/src/kernel32/hal.{h,c}`.

## 5. IRQ centralizada

Despacho por HAL (`irq_escucha` delega en `hal_irq_registra`).
EOI con espurias (7/15 solo si ISR real). `panic` endurecido: sin `kprintf`
(usa `uart_puts` + hex), con rangos de pila/nucleo y `klog FATAL`.
`idt.c`: mascara maestra `0xFC`->`0xF8` (cascada IRQ2 libre para el esclavo).

## 6. Orden de arranque (`k32main.c`)

PIT primero (`pit_drv_init`), deteccion de disco, registro con recursos,
`drv_init_todos`, `irq_init` (PIT/teclado a la HAL) y `sti` al final.
Antes la IRQ0 corria a frecuencia heredada y el teclado encolaba sin
driver listo. NMI inhibida al tocar CMOS (`0x80|0x40`).

## 7. Procesos: fusion diferida y cargas transaccionales

- La fusion ya no corre en IRQ: la IRQ marca pendiente
  (`proc_fusion_pendiente`) y las tareas la ejecutan
  (`proc_fusion_si_toca` en `bloquea`/`cede`/tarea 0).
- `proc_anota_marco_w` (fallible) para rutas transaccionales.
- `sc_07` (sbrk) atomico: ver apartado 16.
- `exec.c`: `kern_crea_proceso` transaccional (anotacion+VMA con rollback);
  se conserva el guarda redundante documentado. `pid` sin envoltura a <=0.

## 8. Cotas (`cd.c`, `fs.c`, `cdcat.c`)

- `cd_sector`: cota real de la copia baja (`CORTE-base`), guarda
  anti-envoltorio en la alta.
- `fs.c`: valida tipo/padre en `fs_crear`; fin en 64 bit en
  `fs_escribir_parcial` (el `off+n` en u32 eludia el tope).
- `cdcat.c`: rechaza `tam` corrupto (<16 o >0xC0014) antes de copiar.

## 9. Drivers y PCI (`drv.{h,c}`, `pci.{h,c}`)

- `drv_registra_ex` con recursos (PIO/IRQ/MMIO): detecta duplicados y
  conflictos al registrar. Teclado declara `0x60/1`; raton no declara
  (comparte KBC); `pit` declara `0x40/4`.
- `pci_busca_n` (n-esima coincidencia, documentada como limite bus 0) y
  `pci_bar_tam` (sizing con restauracion). Sin enumeracion general.

## 10. NIM (`nim.{h,c}`)

Gestor de interfaces minimo sobre la pila intacta (no es segunda pila).
2 ranuras; la 0 es la RTL8139; la 1 solo se detecta (sin gestion HW).
Puerta por software (`up`/`down` bloquea Tx/Rx), MTU 68..1500, ficha Ring3
con estado NIM. L3 migra a `nim_tx(0,...)` (ARP/IP/ICMP) y `red_bombea`
a `nim_rx(0,...)`; `red_ruta_oif` expone la interfaz (hoy siempre 0).
`drv_red.c` llama a `nim_init` en todas las salidas del probe.

## 11. Arranque EDD (El Torito sin emulacion)

El BIOS carga 2 sectores (boot32 de 1024 B) en `0x7C00`; el cargador lee
por LBA absoluta del CD (INT 13h AH=42h) con rebote `0x8000` y cache del
ultimo sector CD. Mapa de RAM intacto: `[cat,993)` a `0x30000`,
`[993,fin)` directo a `0x1000000`, nucleo a `0x10000` y luego a `0x100000`.
Detalles fijados al depurar (todos verificados en QEMU/SeaBIOS 1.17.0):

- `iso_base` por marcador magico `BFSX` en el sector 2 (prohibido parchear
  por offset fijo: antes pisaba codigo).
- `p_lba` en memoria (DI son los 16 bit bajos de EDI: `inc di` corrompia
  el destino de `leer_alta`).
- GDT irreal de 3 entradas (codigo+dato 32 bit 4 GB).
- `KERN_SECS=240` en arranque y constructor (el nucleo mide ~116 KB;
  con 224 sectores se truncaba y `vfs_actual` quedaba a 0).
- El chequeo `AH=41h` se cuelga con DL=0xE0 en este BIOS: se omite por
  diseno (la lectura `AH=42h` lo demuestra o falla con `4` por serie).
- La lectura EDD exige `sti` durante el INT 13h (SeaBIOS/ATAPI usa IRQ);
  sin `sti` el `AH=42h` no vuelve.
- `copyalta.asm` se conserva como historia (ISOs f7x); el arranque F8 ya
  no la usa/inyecta.

## 12. ISO sin tope de 1,44 MB (`build32.py`)

Arranque no-emulado (`medio=0`, `count=2`), payload logico de 512 B
empaquetado 4:1 en sectores CD de 2048 desde `PAYLOAD_LBA`.
La imagen crece con el contenido (`img.extend`); el PVD declara el tamano
real. Salida `FacilOS-v2.3-f8.iso` (f7c intacta).

## 13. Ventana alta y limites restantes documentados

- Cola alta `[993,fin)` en `0x1000000-0x1800000` (8 MB): `cd.c`
  (`CD_ALTO_MAX 0x800000`), `pag.c` (reserva + identidad) y constructor
  (assert `cola <= 8M-8K`) alineados. Antes el nucleo solo mapeaba 1 MB.
- Limites reales restantes: `pay <= 0xFFFF` sectores (32 MB, params u16
  del sector de arranque); pasar a 50/100 MB exige params u32 + ventana
  alta mayor + RAM E820 suficiente (con `-m 32` el E820 util ronda
  22-23 MB). El tamano final lo dicta el contenido, no el historial.

## 14. Prueba de crecimiento (artefacto temporal, descartado)

Relleno temporal de 4 MB (`relleno4`) como entrada 47 del catalogo:
ISO de 5416960 B (2645 sectores, util 5487240 B), arranca, cataloga
`relleno4 lba=2298 tam=4194304` (region alta, prueba la ventana de 8 MB)
y `t1_lib` 258/0. Tras verificar se revertio el hunk del constructor,
se borro el relleno y se reconstruyo la entrega (1,2 MB, 46 programas).

## 15. Regresion de teclado (causa raiz + fix)

Sintoma: con el nucleo F8 el ESC llegaba al escritorio pero el shell no
veia nada; con f7c el mismo harness pasaba. Contadores silenciosos
temporales mostraron que tras salir al escritorio solo llegaba la
primera IRQ (las siguientes se perdian).
Causa: `kbd_tecla`/`raton_lee` migraron a secciones anidables
(`cpu_if_restaura`). La espera con `hlt` exige IF=1; si se entra con
IF=0 (tarea 0 tras `bloquea`), restaurar solo deja IF=0 y el `hlt`
duerme para siempre. El codigo viejo hacia `sti` incondicional.
Fix: `sti` explicito antes de `hlt` (+`cli` tras el) en `kbd_tecla`
y `raton_lee`, como ya hacen `bloquea`/`ktimer`. Instrumentacion
temporal revertida.

## 16. Regresion `sbrk` (causa raiz + fix)

Sintoma tras arreglar el teclado: `t1_lib` daba `FALLO malloc200` y
page fault en NULL.
Causa doble en el `sc_07` reescrito: (a) la "validacion previa" usaba
`pag_valida_usuario` (exige paginas presentes) antes de mapear, lo que
es circular y falla siempre que hay que crecer; (b) el rollback con
`pag_desmapa(ini, v-ini)` desmapeaba paginas preexistentes y no recortaba
`nmarcos` (doble-free pendiente).
Fix: validacion por rango (`n > UHEAP_FIN-*b`); lista previa de paginas
nuevas (max 384) y rollback solo sobre ellas + `nmarcos` truncado al
valor de entrada; VMA con union (ya coalescea heap).

## 17. Regresion de argumentos (causa raiz + fix)

El cambio F8 apuntaba `esp_ini` al final de args (+k+1 alineado) porque
"el primer push pisaba args". Falso: `entrar_usuario` usa la pila de
nucleo; el que pisa es el `call _main` de `start.asm`, que apila hacia
abajo. Con esp al final, el retorno caia sobre `0x2F0000` y `dhcp` leia
basura (`Uso: dhcp [renovar|liberar]` con args vacios).
Fix: revertir a `esp_ini = 0x2F0000` (el `call` apila en `0x2EFFFFC` y
los args quedan intactos).

## 18. Bateria de pruebas (todo verificado en QEMU, una instancia cada vez)

- `t1_lib`: 258 pruebas 0 fallos.
- `t_red`: 97 pruebas 0 fallos (sin adaptador; con `QRUN_RED=1` el probe
  ve RTL8139 `io=c000 irq=11 mac=52:54:00:12:34:56` y `dhcp` obtiene
  `10.0.2.15/24 via 10.0.2.2 dns 10.0.2.3`).
- `tutf8`: 26 pruebas 0 fallos.
- `mem32`: 15 pruebas 0 fallos.
- `t_fb`: 94 pruebas 0 fallos sin red; 105 pruebas 0 fallos viva contra
  `tools/http_stub.py` (guest `dhcp`, luego `t_fb`; el stub sirve
  `/pagina`, `/malformada`, `/grande-html` con tope y `/busca?q=hola`).
- Crecimiento: apartado 14.

## 19. Compatibilidad

Ring 3 y ABI/syscalls intactos (sin cambios de contrato; `sc_07`/`esp`
fueron correcciones de errores, no cambios de API). FacilBrowser, pila
TCP/IP y apps Ring 3 sin tocar salvo `t_fb`/`t_red` heredados de F7.
ISOs historicas presentes (`f7b`/`f7c` entre otras); `FacilOS-v2.3-f8.iso`
nueva sin sobrescribir ninguna.

## 20. Cierre

Un unico commit `F8` + push normal (sin `reset --hard`, sin `clean -fd`,
sin force). Verificar: `git status` limpio salvo la entrega, HEAD==origin,
`main` en su sitio, stash del compositor intacto. Este informe es el
apartado 20 de 20.
