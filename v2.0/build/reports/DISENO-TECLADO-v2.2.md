# DISENO-TECLADO-v2.2

## Arquitectura
Driver (scancodes+modificadores) + tablas por distribucion. Ring3 no
duplica: recibe codepoints/TEC_* por el pipeline int existente.

## Distribuciones (tablas en `teclado.c`, por scancode set-1)
- US (0): compatible v1.x (sin enie; 0x27=;).
- FacilOS-ES (1, defecto): letras/numeros ES + ñ/º/ª/·/¡/¿; muertas
  0x1A tumba/circunflejo, 0x28 agudo/dieresis; AltGr: | @ # ~ € \.
- FacilOS-CA (2): ç directa (0x27); agudas en 0x1A, graves en 0x28;
  mismo AltGr. l·l = l+·+l (documentado, sin tecla propia).

Muertas: rango privado 0xE000-4 (nunca texto); componen en el driver
(tabla espejo de ut_compone); con espacio dan el literal (´ ` ¨ ^ ~);
con base incompatible dan literal+base; ESC/retroceso/flechas las
cancelan. ~ siempre muerta (asi hay ñ en CA).

## Teclas nuevas
TAB 0x0F->9 (antes muerta: el ciclo de foco era inalcanzable), Supr
0x106, Inicio 0x107, Fin 0x108, RePag 0x109, AvPag 0x10A, E0 Inicio/Fin,
numpad con NumLock (defecto on). Sin Ctrl (no hay codigos).

## Seleccion y persistencia
`distribucion_{actual,pon}` + syscall 46 (leer/establecer, 0-2).
Persiste en cabecera byte 15 (lee al montar, escribe al guardar;
mismo camino que teclado es/us). Cab 14 legacy respetada. Shell
`teclado es|ca|us`; terminal `teclado ...` (misma syscall).

## Coste
Tablas ~1 KB + compose ~0.5 KB en nucleo (motivo subir KERN 128->160
junto a UTF-8; ver informe final). Sin ISR nuevas (IRQ1).
