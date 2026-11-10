# F32 C++ — C++ nativo en FacilOS (v2.4.2)

Segundo lenguaje nativo de F32 OS junto a C y ASM. Programas C++ reales,
x86 32-bit, Ring 3, formato F32X **sin cambios**, sobre las APIs C existentes.

## 1. Auditoría previa (resumen)

| Pieza | Estado | Reutilización |
|---|---|---|
| Toolchain | `gcc32/mingw32`: g++ 16.2 i686, ld 2.47, objcopy/objdump | Directa con flags propios |
| Entrada usuario | `start.asm`: `call _main`, salida syscall 0 | Modelo copiado en `startxx.asm` |
| Formato F32X v3 | plano ≤192K, bss ≤256K, plano+bss ≤768K, base 0x200000, entry=offset | Sin cambios |
| Cargador (`exec.c`) | copia plano a 0x200000, pila en 0x2E0000, `proc_crea(eip=base+ent)` | Sin cambios |
| Heap usuario | `u_malloc/u_free/u_realloc/u_calloc` (first-fit sobre sbrk, syscall 7) | `new/delete` delegan |
| Syscalls | `int 0x80` vía `sys.h` (`static inline`, sin linkage externo) | Sin cambios, sin tocar ABI |
| `ulib/ugfx/utext/ulog` | C con `-ffreestanding -nostdlib` | Guards `extern "C"` (solo C++, ABI igual) |
| Build (`build32.py`) | compila C, enlaza `-nostdlib -e _start -T prog.ld`, objcopy, cabecera F32X | Extendido con rama C++ |
| Tests (`qrun.py`) | QEMU + serie + comandos | Reusado tal cual |

Hallazgo clave: MinGW decora símbolos C con `_` extra (`__init_array_start`
→ `___init_array_start`) y emite `call __main` + `atexit` aunque se pida
`-fuse-cxa-atexit`. El runtime lo asume (etiquetas asm literales + `atexit`
propio). No se presupuso nada: se comprobó enlazando.

## 2. Toolchain

`C:\Users\artur\gcc32\mingw32\bin\g++.exe` (misma 16.2 i686 que C).

```text
CXXFLAGS = -ffreestanding -nostdlib -m32 -march=i386 -O1 -Wall
           -fno-pic -fno-pie -fno-stack-protector -fzero-initialized-in-bss
           -Os -ffunction-sections -fdata-sections
           -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables
           -fuse-cxa-atexit
```

Enlazado con el propio g++ (`-m32 -nostdlib -e _start --gc-sections
-T prog.ld`): **no entra libstdc++ ni libc del anfitrión**.
Sin `-lgcc`: el código de 64-bit (p. ej. división `long long`) falla en el
enlace con mensaje claro en vez de colar objetos del host.

## 3. Lenguaje soportado / desactivado

Sí: funciones y sobrecarga, clases, ctor/dtor, herencia simple y múltiple
básica, `virtual` (tablas en `.rdata`, direcciones absolutas 0x200000,
sin reubicación), encapsulación, referencias/punteros, `namespace`,
templates (función y clase), operadores y funciones miembro, `extern "C"`,
estáticos locales (guardas), objetos globales con orden de construcción.

No (por diseño): excepciones (`-fno-exceptions`), RTTI (`-fno-rtti`,
sin `dynamic_cast`/`typeid`), `libstdc++` (sin STL/contenedores),
`new` con throw (devuelve 0 si no hay memoria), GC, hilos, 64-bit en
aritmética (sin libgcc), `dynamic_cast`, locales C++.

## 4. Runtime (`v2.0/user/lib/f32cxx.cpp`, ~150 líneas)

- `operator new/new[]/delete/delete[]` (+ `sized delete` C++14) sobre
  `u_malloc/u_free`.
- `memcpy/memmove/memcmp/memset/strlen` globales (`extern "C"`) sobre
  `u_*`: los emite el codegen aunque se use `-fno-builtin`-style.
- `__cxa_pure_virtual`: mensaje + salida con código 99.
- `__cxa_atexit` + `atexit` (tablas fijas de 64): los dtores globales
  corren en inverso tras `main`.
- Guardas Itanium monohilo (`__cxa_guard_acquire/release/abort`).
- `__dso_handle = 0`, `__f32_cxx_init` (recorre `.init_array`),
  `__main` (la init real e idempotente que g++ invoca), `__f32_cxx_fini`.

## 5. Entrada y linker

- `startxx.asm` (nuevo; `start.asm` de C intacto): `init → main →
  fini → exit(rc)`. El `rc` de `main` se preserva en la pila durante `fini`.
- `prog.ld`: añade `*(.rodata*) *(.text.*) *(.data.*)` y
  `KEEP(*(.init_array*) *(.ctors*))` con símbolos `__init_array_start/end`.
  Los programas C no traen esas secciones: su binario no cambia de forma.
- Las apps C++ **no definen** `__main` (las C conservan su stub vacío).

## 6. ABI e interoperabilidad

F32X, syscalls y layout de proceso idénticos. Cabeceras C con guards
`extern "C"` (`ulib.h`, `ugfx.h`, `utext.h`, `ulog.h`; `sys.h` no lo
necesita: todo `static inline`). Separación: `f32cxx.*` es runtime de
lenguaje; las APIs del SO siguen en `ulib`/`sys.h`.

## 7. Compilar y ejecutar

Automático: añadir `("prog", ["prog.cpp"], True)` a `_apps3` en
`build32.py` (detecta `.cpp` por extensión). Manual:

```text
g++.exe -ffreestanding -nostdlib -m32 -march=i386 -Os ... -fno-exceptions
  -fno-rtti -I v2.0/user/lib -I v2.0/user/apps -c prog.cpp -o prog.o
g++.exe -m32 -nostdlib -Wl,--image-base,0x10000 -Wl,-e,_start
  -Wl,--gc-sections -T v2.0/user/ld/prog.ld -o prog.pe
  startxx.o prog.o f32cxx.o ulib.o utext.o ulog.o
```

Ejecutar (ISO `v2.0/build/images/FacilOS-v2.4.2.iso`, también en `build/iso`):

```text
python v2.0/tools/qrun.py v2.0/build/iso/FacilOS-v2.4.2.iso - serie.log 5126 "{ESC}" "{SLEEP:3}" "ejecutar cppdemo"
```

## 8. Programas

- `v2.0/user/apps/cppdemo.cpp`: testigo global, Ring 3, `Punto+operador`,
  herencia + `virtual`, `new/delete/new[]`, template función/clase,
  interop (`u_malloc/u_strcpy/sys_ticks`), `retorno: 0`.
- `v2.0/user/apps/t_cpp.cpp`: 18 comprobaciones, `t_cpp: 18 pruebas
  0 fallos`, `dtor-global` en serie, salida = nº de fallos.

## 9. Pruebas y resultados (2026-11-10, QEMU `-m 32`, ISO v2.4.2)

| Prueba | Resultado |
|---|---|
| `t_cpp` | 18 pruebas 0 fallos, `dtor-global` presente, `retorno: 0` |
| `cppdemo` | salida completa correcta (`punto=4,6 lados=3 v3=30 doble=42 caja=40`), `retorno: 0` |
| `t1_lib` (regresión C) | 258 pruebas 0 fallos |
| `tutf8` (regresión C) | 26 pruebas 0 fallos |
| `mem32` (regresión C) | 15 pruebas 0 fallos |
| `t_fb` + stub (regresión, valida `prog.ld` en todos) | 129 pruebas 0 fallos |

`t_red` no se corre: cuelga en `eco-seq-ok` también en ISO F8 (entorno
slirp, no regresión; ver estado v2.4).

## 10. Limitaciones conocidas

- Sin STL: hay que programar contra `ulib`/`sys.h` (a propósito).
- `new` sin memoria devuelve 0 (no `std::bad_alloc`: no hay excepciones).
- Tablas de dtores/guardas fijas (64) y monohilo.
- Binarios C++ pequeños medidos: `cppdemo` 2652 B, `t_cpp` 3036 B
  (muy lejos de los topes F32X).
- No se tocó el kernel ni el ABI: a propósito, cero riesgo para C.

## 11. Informe de la integración

Cambios: `user/lib/startxx.asm` + `f32cxx.cpp` (nuevos),
`user/ld/prog.ld` (init_array/ctors/rodata), guards C++ en 4 cabeceras,
`tools/build32.py` (GXX/CXXFLAGS/rama C++/2 programas/ISO v2.4.2),
`user/apps/cppdemo.cpp` + `t_cpp.cpp` (nuevos), este documento.
Decisión clave: runtime propio mínimo en vez de libstdc++; `atexit`
propio porque MinGW lo emite igual con `-fuse-cxa-atexit`; `.init_array`
por etiqueta asm por la decoración `_` de MinGW.
No se modificó `main` (repo), ni `start.asm`, ni kernel, ni syscalls.
Rama: `v2.4.2`. Criterios 1–7 del encargo: cumplidos (ver tabla §9).
