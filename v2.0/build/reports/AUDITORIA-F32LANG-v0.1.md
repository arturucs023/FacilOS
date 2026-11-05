# AUDITORIA-F32LANG-v0.1 — FASE 1

Rama `v2.2-f32lang` (de `v2.2-editor`=`29eb475`, verificado).
SO v2.2 intacto; F32 Lang versiona aparte (v0.1).

## 1. Arquitectura y memoria

- Kernel C+asm 32 bit, Ring0/Ring3, ABI syscalls 0-49 (49 =
  modificadores teclado, v2.2-editor). Sin cambios de kernel
  previstos para v0.1.
- Proceso usuario: codigo+datos+BSS <= 0xC0000, args <= 255 B
  (`u_args`), heap sbrk 1.5 MB (0x300000-0x480000), 768 marcos.
  Interprete cabe de sobra (objetivo < 64K plano).
- `u_malloc` first-fit; `u_malloc_info` para medir. Sin OOM
  killer: `u_malloc` devuelve 0 → el interprete debe
  comprobar cada reserva (error F32-090, no PANIC).

## 2. Syscalls/ABI utiles (sin altas)

- Ficheros 3/4/5/6/15/24 (`u_fopen/u_fread/u_fsize/...`, 8 slots
  `UFILE`). Fuente `.f32` <= 65535 B (formato FS); tope
  interprete 32768 B (igual que el editor).
- Consola 1/8/27: `u_print`, `u_lee_linea` (eco y borrado
  incluidos) para `escribir`/`leer` en hijo modal.
- Procesos 28/31 (`u_crear/u_ejecutar/u_esperar`): el hijo
  modal hereda consola; rc = exit code (<-100 = fallo carga).
- Nombres FS 31 B, rutas 128, UTF-8 opaco. Sin cambio de FS.

## 3. Terminal (integracion FASE 5)

- `op_ejecutar` (`terminal.c:1596`): junta `pos_arg[1..]` en
  `args[256]`, `ug_modal(1)`, `u_ejecutar(pos_arg[0], args)`,
  `ug_modal(0)`, imprime `rc=N`, `perr(33)` si rc<-100.
- Registro `CMDS` (`:338`): `ejecutar`, min 1 max 4 pos, sin
  pars. Ayuda/manual/autocompletado salen del registro.
- `a_absoluta()` (`:685`) convierte a absoluta desde el cwd del
  terminal. `op_ejecutar` hoy NO la usa (el nucleo resuelve
  contra `exec_base_dir`): para `.f32` se convertira aqui
  (rutas cwd-relativas funcionan) sin tocar el nucleo.
- Extension: el despacho actual no mira extensiones; se anade
  rama `.f32` -> lanza `f32` con la ruta (minimo diff).

## 4. Editor (integracion FASE 6)

- `tecla_doc`: Enter inserta `\n` pelado (`:2859`), Tab
  inserta 4 espacios (`:2872`): punto de insercion del
  auto-indent (reglas por palabra de la linea anterior).
- Backspace borra 1 caracter; no hay reduccion por nivel:
  se anade (solo en lineas de indentacion).
- Shift+Tab HOY indistinguible (Tab emite 9 siempre):
  requiere codigo de driver como `0xE120` (Ctrl+Tab) ya
  existente; se propone `0xE121` May+Tab (3 lineas, mismo
  patron). Sin esto no hay Shift+Tab fiable: auditado.
- Sin framework de coloreado (color = formato de usuario por
  byte) ni autocompletado: v0.1 = indentacion; coloreado =
  futuro documentado (no se sacrifica estabilidad).
- `icono_de`/`texto_tipo` (`explorador.c:157/186`): anadir
  `"f32"` a la lista de texto (1 linea justificada).
- Borrado y edicion seguros UTF-8 (`ut_*`); el interprete
  reutiliza `utext` (ya enlazado con `con_ulib`).

## 5. Carga, build e ISO

- `kern_crea_proceso`: F32X v1/2/3, args 255 B, rc propaga.
  Interprete = programa consola Ring3 normal (como `t1_lib`).
- `build32.py`: anadir `("f32", ["f32.c", ...], True)` a
  `_apps3`; hereda `-Os --gc-sections`, tope v3 128K.
  ISO fija `FacilOS-v2.2-terminal.iso`; la fase 7 seguira el
  patron editor (`*-editor.iso` -> copia `*-f32.iso` + boot).
- `--disco` omite binarios >64K con aviso (no afecta: el
  interprete ira por ISO; los `.f32` de usuario viven en FS).

## 6. Pruebas existentes (base de regresion)

- `t1_lib` 258, `tutf8` 26, `mem32` 15, `disco32` 38,
  `ipc32` 111, `equidad32` 3 (verdes en v2.2-editor).
- `t2_gfx` 7 fallos estables aqui e identicos en base limpia
  (entorno, no regresion). `qrun.py` + capturas = metodo.
- Patron para F32: binario `f32` + `.f32` de ejemplo
  (embebidos para limites sin fixtures FS + extremo a extremo
  via editor+`ejecutar` en QEMU).

## 7. Reutilizable vs nuevo

- Reutilizar: syscalls FS/consola/mem/proc, `ulib/utext`,
  `u_lee_linea`, protocolo modal, `a_absoluta`, registro CMDS,
  patron `haz_programa`, metodo qrun, editor/explorador.
- Nuevo: lexer/parser/AST-walker del interprete, `f32.c`,
  mensajes F32-0NN, suite `.f32`, 12 docs en `docs/f32-lang/`,
  ramas `.f32` en `ejecutar`, indent en editor, codigo `0xE121`,
  `"f32"` en explorador.
- Riesgos: tamano plano (vigilar <64K), cuelgues por bucles
  (cuota de iteraciones), recursion (cuota 64), memoria
  (topes + `u_malloc` comprobado), QEMU fusiona teclas
  (pruebas con reintentos).
