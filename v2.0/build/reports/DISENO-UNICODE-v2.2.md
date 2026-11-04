# DISENO-UNICODE-v2.2

## Codificacion
UTF-8 en todas partes (teclado, IPC, FS, pantalla, serie). int pipeline
ya de 32 bits (sin cambios de ABI).

## Capas
- Entrada (driver): emite codepoints NFC directos (muertas compuestas
  en el driver). utext (Ring3) + gfx.c/consola (nucleo) decodifican.
- utext (`user/lib/utext.{h,c}`): dec/cod estrictos, valido, largo,
  anterior/siguiente, compone (tabla latina acotada), nfc, ancho.
- Nucleo `utf8_dec` (gfx.c): misma semantica; bytes sueltos 0x80-0xFF
  -> CP437 directo (compat volumenes v1.x, documentado).
- Consola: decodifica con estado (serie conserva bytes crudos; texto
  VGA traduce por tabla o '?').

## Glifos
Fuente BIOS CP437 verificada glifo a glifo contra el video real
(QEMU std): `build/font_slots*.png`, `font_all.png`, `font_rowe.png`
(evidencia local). Tabla `l1_glifo` SOLO con slots verificados
(es/ca/dieresis/puntuacion/©«»ªº·¢£¥₧ƒßð etc.). Resto -> '?' (0x3F).
U+20AC (€) NO existe en la fuente: dibujo propio 8x16 (`glifo_euro`).
Faltan (documentado): Ò Ù Ý ý Þ þ Ø ø ¬ y astrales; VGA texto muestra
'?' (sin celda libre).

## Normalizacion
Politica: NFC en el borde (el driver emite precompuestos). `ut_nfc`
compone pares latinos base+U+0300-0308/0327 (tabla explicita).
VFS byte-exacto (precedente Linux): no renombra ni fusiona; dos formas
distintas son dos ficheros (documentado). Sin tablas de descomposicion
completas (coste injustificado para latin).

## Memoria
utext ~1.5 KB codigo; tablas nucleo ~200 B; euro 16 B; sin buffers
nuevos (lineas existentes). Limites previos intactos.

## Version Unicode
Tablas disenadas contra Unicode 15 (rango latin + simbolos); sin
dependencia de version (tablas propias embebidas).
