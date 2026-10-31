# FacilOS

Sistema operativo de 32 bits desarrollado desde cero (sin Linux ni otro kernel
como base): arranque BIOS, kernel x86 en C freestanding + NASM, modo protegido
(GDT/IDT/TSS), separación Ring0/Ring3, multitarea Round Robin con planificador
sobre PIT, memoria virtual por proceso, sistema de archivos propio en RAM,
formato ejecutable F32X, shell y aplicaciones independientes.

- **v1.9 (estable)**: ver [release v1.9](../../releases/tag/v1.9) y
  `build/reports/INFORME-ESTABILIZACION-INTEGRAL-v1.9.md`.
  Validada en QEMU: batería de aislamiento/procesos 8/8, `t1_lib` 258 pruebas
  0 fallos, 40+20 ciclos crear/esperar, balance exacto de marcos, sin PANIC.
- **v2.0-etapa1 (en desarrollo)**: rama `v2.0-etapa1` (memoria, IPC, syscalls,
  drivers, VFS, seguridad). Sin entorno gráfico todavía.

## Estructura

| Ruta | Contenido |
|---|---|
| `src/kernel32/` | Kernel v1.9→v2.0 (memoria, procesos, syscalls, FS, shell, drivers) |
| `src/legacy16/` | Línea histórica 16-bit (no modificar) |
| `user/apps/` | Aplicaciones F32X (shell, menú, reloj, batería `aislamiento32`, …) |
| `user/lib/` | Biblioteca de usuario (`ulib`, `sys.h`) y arranque (`start.asm`) |
| `user/ld/` | Enlazado de programas (`prog.ld`) |
| `tools/` | `build32.py` (build), `qrun.py`/`qwin.ps1` (QEMU), utilidades |
| `build/iso/` | ISOs (`FacilOS-v1.9-32.iso` estable) |
| `build/reports/` | Informes de estabilización y de etapa |
| `build/evidence/` | Evidencias de QEMU/serie de versiones anteriores |

## Requisitos (Windows)

- Python 3 (`python tools/build32.py`).
- GCC MinGW i686 (`C:\Users\artur\gcc32\mingw32\bin\gcc.exe`).
- NASM incluido en `tools/nasm/`.
- QEMU i386 (`C:\Program Files\qemu\qemu-system-i386.exe`).

## Construir

```powershell
python tools/build32.py
```

Genera el kernel (`build/bin/kernel32.bin`), los programas y la ISO en
`build/iso/`. La ISO de la rama correspondiente lleva su nombre
(`FacilOS-v1.9-32.iso`, `FacilOS-v2.0-32.iso`); no reutilizar nombres de
versiones estables.

## Probar en QEMU

```powershell
# ISO, "-" = sin disco, log serie, puerto monitor, comandos...
python tools/qrun.py build/iso/FacilOS-v1.9-32.iso - build/logs/hola.log 4720 "{ESC}" "{SLEEP:3}" "ejecutar hola"
```

Arranque → menú gráfico → `{ESC}` sale a la shell (`facil32:/>`). Comandos
útiles: `version`, `ejecutar hola`, `ejecutar t1_lib`,
`ejecutar aislamiento32` (batería A–H), `ejecutar planificador32`.

## Ramas

- `main`: v1.9 estable (congelada como referencia; tag `v1.9`).
- `v2.0-etapa1`: evolución del kernel por entregas (ver `build/reports/`).

## Licencia

MIT — ver `LICENSE`.
