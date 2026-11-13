# FacilOS (rama v2.4.3)

Sistema operativo de 32 bits desarrollado desde cero (sin Linux ni otro kernel
como base): arranque BIOS, kernel x86 en C freestanding + NASM, modo protegido
(GDT/IDT/TSS), separación Ring0/Ring3, multitarea Round Robin con planificador
sobre PIT, memoria virtual por proceso, sistema de archivos propio en RAM,
formato ejecutable F32X, shell y aplicaciones independientes.

v2.4.3 (esta rama, en desarrollo): evolución importante sobre v2.3-red.

- **VESA por enumeración**: el arranque ya no depende de los modos Bochs;
  enumera los modos VBE y elige resolución+32bpp+LFB. Vale en QEMU,
  VirtualBox y HW real (`v2.0/src/kernel32/boot32.asm`).
- **USB por OHCI con sondeo** (sin IRQ nueva): teclado y ratón HID-boot
  se enumeran y se inyectan en el camino PS/2 (`usb_ohci.c`,
  `teclado_usb_tecla`, `ps2_inyecta_usb`). Probado en QEMU
  (`pci-ohci` + `usb-kbd` + `usb-mouse`).
- **Navegador con CSS básico**: color/fondo por etiqueta, clase e inline,
  negrita/cursiva; formularios GET (sirve un SearXNG propio por HTTP).
  Sin JS; sin HTTPS (ver `docs/network/TLS.md`).
- ISO: `v2.0/build/iso/FacilOS-v2.4.3.iso` (solo la de esta rama).

Batería v2.4 en QEMU: `t1_lib` 258/0, `tutf8` 26/0, `mem32` 15/0,
`t_fb` 129/0. `t_red` llega hasta el mismo punto que F8 (la parte viva
externa depende del NAT/slirp del anfitrión).

## Estructura

| Ruta | Contenido |
|---|---|
| `v2.0/src/kernel32/` | Kernel (memoria, procesos, syscalls, FS, shell, drivers incl. USB) |
| `v2.0/src/legacy16/` | Línea histórica 16-bit (no modificar) |
| `v2.0/user/apps/` | Aplicaciones F32X (shell, fbrowser, t_fb, …) |
| `v2.0/user/lib/` | Biblioteca de usuario (`ulib`: red, HTTP, HTML+CSS, …) |
| `v2.0/tools/` | `build32.py` (build), `qrun.py`/`qwin.ps1` (QEMU), `http_stub.py`, utilidades |
| `v2.0/build/iso/` | ISO (`FacilOS-v2.4.3.iso`) |
| `v2.0/docs/` | Documentación (red, kernel) |

## Requisitos (Windows)

- Python 3.12 (`winget install Python.Python.3.12`).
- GCC MinGW i686 16.2.0 vía MSYS2 (`C:\msys64\mingw32\bin\gcc.exe`, ruta configurable en `v2.0/tools/build32.py`).
- NASM incluido en `v2.0/tools/nasm/`.
- QEMU 11.1.0 i386 (`C:\Program Files\qemu\qemu-system-i386.exe`).

## Construir

```powershell
# Desde la raíz del repo.
python v2.0/tools/build32.py
```

Genera el kernel, los programas y la ISO en `v2.0/build/iso/`.
Cada rama solo lleva las ISOs de su versión.

## Probar en QEMU (arranque manual con red y USB)

```powershell
# Desde la raíz del repo. Red user NAT + RTL8139 + OHCI + HID.
& 'C:\Program Files\qemu\qemu-system-i386.exe' -cdrom v2.0\build\iso\FacilOS-v2.4.3.iso -boot order=d -m 32 -netdev user,id=red0 -device rtl8139,netdev=red0,mac=52:54:00:12:34:56 -device pci-ohci -device usb-kbd -device usb-mouse
```

- Red QEMU user: `10.0.2.0/24`, DNS `10.0.2.3`. Dentro de FacilOS, ejecutar
  `dhcp` en la shell para tener IP; sin esos parámetros arranca igual pero
  sin adaptador (modo sin red).
- Arranque → menú gráfico → `{ESC}` sale a la shell (`facil32:/>`). Para
  volver al escritorio: `ejecutar servidor`. Teclas: F1 = menú.
- Sin USB físico, el PS/2 de siempre sigue funcionando (ambos conviven).

Harness automatizado (serie + monitor, para baterías):

```powershell
$env:QRUN_RED = "1"
$env:QRUN_USB = "1"
python v2.0/tools/qrun.py v2.0/build/iso/FacilOS-v2.4.3.iso - serie.log 5123 "{ESC}" "{SLEEP:3}" "ejecutar t1_lib"
```

## Licencia

MIT — ver `LICENSE`.
