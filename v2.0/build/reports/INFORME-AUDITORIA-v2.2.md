# INFORME-AUDITORIA-v2.2 (base real v2.1-p1-tiempo @8ba820e)

## A. Terminal (`user/apps/terminal.c`, 342 lin)
Line editor ASCII (32-126), cursor pos-por-byte, historial 10 sin
editar-recuperado (lo pisa), sin comillas/parametros (`cmd`+`resto`
planos), builtins: ayuda/version/mem/procs/fecha/limpiar/salir/ver/
ejecutar. Sin TAB (nunca llega), sin Home/Fin/Supr, sin `manual`,
sin sugerencias. Prompt `f32>`. Todo Ring3/UGFX.

## B. Teclado (`src/kernel32/teclado.c`, 144 lin)
Tabla CP437 base/mayus (ES parcial: ñ 0xA4, ºª, · en shift-3).
Shift izq/der, Caps solo a-z, sin AltGr (E0 0x38 ignorado), sin teclas
muertas, sin NumLock, sin releases (salvo shifts). E0: flechas+Menu.
SIN mapear: 0x0F (TAB: no llega a nadie, el ciclo de foco del servidor
es codigo muerto hoy), 0x1A/0x1B/0x28/0x2B/0x56, F2-F12, Ctrl/Alt.
Cola 64 ints; pipeline int (32 bits: caben codepoints). `teclado_es`
persiste en cabecera disco byte 14 (`fs_guardar`, shell `teclado es|us`
+ `autoguardar`). Distribucion y driver mezclados (una tabla).

## C. Graficos/texto
`g_texto` dibuja cada BYTE como CP437 (`FONT_BIN+c*16`, 0x6000, copiada
por loader). Consola texto = VGA 0xB8000 (fuente HW); consola grafica
`glifo()` opaco igual. Sin UTF-8 en ningun nivel. Fuente BIOS cubre
casi todo ES/CA (ñ á é í ó ú ü ï ç à è ò · ¿ ¡ º ª âêîôû) SALVO €.
Eventos: servidor come TAB (ciclo) y ESC (salir); flechas/ENTER si
llegan. IPC 64 B texto.

## D. Almacenamiento
Formato intacto (64 ent, nombres NUL 31 B + 1, `nombre_ok` rechaza
`' '` y `/`). Comparacion por bytes (`fs_hijo`). Sin cambios: UTF-8
pasa como bytes; espacios requieren abrir `nombre_ok` (superset
compatible). `fs_borrar` exige carpeta vacia (-2). `fs_mover` existe
en kernel pero SIN exponer (sc_43 solo misma carpeta). `dir_actual`
global (shell); Ring3 sin get/set: terminal llevara su propio cwd
(indice + pila de ruta). `u_dir_hijo(dir<0=actual)`.

## E. Apps
Terminal/editor/explorador/sysinfo/configura sobre UGFX; editor borra
por byte (romperia multibyte); explorer trunca por bytes. Servidor:
EDIT inexistente (hay que añadir opt-in TAB/ESC).

## F. Registros
Kernel: kprintf cortos (algunos en castellano ya). Tests dependen del
formato serial (NO tocar). Servidor/apps: `u_print` ad-hoc. Sin
niveles/codigos. Decision: lib Ring3 `ulog` + terminal; kernel intacto
(documentado).

## G. Compatibilidad
ABI +1 justificada: syscall 46 `teclado_distribucion` (driver+persis-
tencia no son alcanzables desde Ring3). Todo lo demas Ring3 o interno
kernel sin ABI. FS sin cambios de formato (solo `nombre_ok` abre
espacio). Nombres viejos CP437: renderer tolerante (bytes sueltos ->
CP437), validacion estricta en utext. VFS byte-exacto (precedente
Linux, documentado); NFC-compose en utext + driver emite NFC.
