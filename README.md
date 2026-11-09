# FacilOS

Sistema operativo de 32 bits desarrollado desde cero (sin Linux ni otro kernel
como base): arranque BIOS, kernel x86 en C freestanding + NASM, modo protegido
(GDT/IDT/TSS), separación Ring0/Ring3, multitarea Round Robin con planificador
sobre PIT, memoria virtual por proceso, sistema de archivos propio en RAM,
formato ejecutable F32X, shell y aplicaciones independientes.

- **v1.9 (estable)**: esta rama (`main`, tag `v1.9`) y
  `build/reports/INFORME-ESTABILIZACION-INTEGRAL-v1.9.md`.
  Validada en QEMU: batería de aislamiento/procesos 8/8, `t1_lib` 258 pruebas
  0 fallos, 40+20 ciclos crear/esperar, balance exacto de marcos, sin PANIC.
- **Desarrollo actual**: rama `v2.3-red` (F8: kernel reformado con arranque
  El Torito + EDD, red con TCP propio, compositor con región dañada y clip por
  arrendamiento, navegador básico solo-HTTP). El árbol v2 vive en `v2.0/`.

## Estructura

| Ruta | Contenido |
|---|---|
| `src/kernel32/` | Kernel v1.9 (memoria, procesos, syscalls, FS, shell, drivers) |
| `src/legacy16/` | Línea histórica 16-bit (no modificar) |
| `user/apps/` | Aplicaciones F32X v1.9 (shell, menú, reloj, batería `aislamiento32`, …) |
| `user/lib/` | Biblioteca de usuario v1.9 (`ulib`, `sys.h`) y arranque (`start.asm`) |
| `user/ld/` | Enlazado de programas (`prog.ld`) |
| `tools/` | `build32.py` (build v1), `qrun.py`/`qwin.ps1` (QEMU), utilidades |
| `build/iso/` | ISOs v1 (`FacilOS-v1.9-32.iso` estable) |
| `build/reports/` | Informes de estabilización v1 |
| `v2.0/` | Árbol v2 (kernel32, apps, lib, `tools/build32.py`, `docs/`, ISOs en `v2.0/build/iso/`) |

## Requisitos (Windows)

- Python 3.
- GCC MinGW i686 (ruta configurable en `tools/build32.py` / `v2.0/tools/build32.py`).
- NASM incluido en `tools/nasm/` (v1) y `v2.0/tools/nasm/`.
- QEMU i386 (`C:\Program Files\qemu\qemu-system-i386.exe`).

## Construir

```powershell
# v1 (esta rama)
python tools/build32.py
# v2 (ramas v2.x, desde la raíz del repo)
python v2.0/tools/build32.py
```

Genera el kernel, los programas y la ISO en `build/iso/` (v1) o en
`v2.0/build/iso/` (v2). Cada rama solo lleva las ISOs de su versión (ver tabla);
no reutilizar nombres de versiones estables.

## Probar en QEMU (arranque manual con red)

```powershell
# Desde la raíz del repo. Red user NAT + RTL8139 (la que usa FacilOS).
& 'C:\Program Files\qemu\qemu-system-i386.exe' -cdrom v2.0\build\iso\FacilOS-v2.3-f8.iso -boot order=d -m 32 -netdev user,id=red0 -device rtl8139,netdev=red0,mac=52:54:00:12:34:56
```

- Red QEMU user: `10.0.2.0/24`, DNS `10.0.2.3`. Dentro de FacilOS, ejecutar
  `dhcp` en la shell para configurar la IP; sin los dos parámetros de red el
  sistema arranca igual pero sin adaptador (modo sin red).
- En esta rama (`main`) la ISO es `build/iso/FacilOS-v1.9-32.iso` (sin red).
- Arranque → menú gráfico → `{ESC}` sale a la shell (`facil32:/>`). Comandos
  útiles: `version`, `ejecutar hola`, `ejecutar t1_lib`.

Harness automatizado (serie + monitor, para baterías):

```powershell
$env:QRUN_RED = 1
python v2.0/tools/qrun.py v2.0/build/iso/FacilOS-v2.3-f8.iso - serie.log 5123 "{ESC}" "{SLEEP:3}" "ejecutar t1_lib"
```

## Ramas e ISOs

Cada rama contiene solo sus propias ISOs (el historial conserva las demás).

| Rama | ISO(s) en la rama |
|---|---|
| `main` | `build/iso/FacilOS-v1.*.iso` (v1.1–v1.9, estable) |
| `v2.0-etapa1` | `build/iso/FacilOS-v2.0-32.iso` |
| `v2.0-etapa2` | `build/iso/FacilOS-v2.0-etapa2.iso` |
| `v2.0-etapa3` | `v2.0/build/iso/FacilOS-v2.0-etapa3.iso` |
| `v2.0-etapa4` | (sin ISO propia) |
| `v2.0-etapa4-desktop` | `v2.0/build/iso/FacilOS-v2.0-etapa4.iso` |
| `v2.1-p0-visual` | `v2.0/build/iso/FacilOS-v2.1-p0.iso` |
| `v2.1-p1-tiempo` | (sin ISO propia) |
| `v2.2-editor` | `v2.0/build/iso/FacilOS-v2.2-editor.iso` |
| `v2.2-explorador` | (sin ISO propia) |
| `v2.2-f32lang` | `v2.0/build/iso/FacilOS-v2.2-f32.iso` |
| `v2.2-terminal-castellano` | `v2.0/build/iso/FacilOS-v2.2-terminal.iso` |
| `v2.3-red` | `v2.0/build/iso/FacilOS-v2.3-*.iso` (f8 vigente + historial f1…f7c) |

## Licencia

MIT — ver `LICENSE`.
