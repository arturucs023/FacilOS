# Hito 7 NetSurf — Núcleo a 3 MB por programa (mapa nuevo)

Rama `v2.4.3`, sin tocar `main`. Solo FacilOS nativo en el binario
final; el cambio de mapa es la excepción autorizada.

## 1. Mapa nuevo (desplazamiento +0x180000 de pila/heap/bloque kernel)

- `0x200000-0x4FFFFF` usuario: código+BSS (`plano+bss<=0x300000`)
- `0x500000` página de guardia (no presente)
- `0x510000-0x52FFFF` pila de usuario, 32 págs; args/esp en `0x520000`
- `0x530000-0x5FFFFF` heap usuario (sbrk, 832K, base sin cambios
  de tamaño)
- `0x600000-0x7BFFFF` supervisor: pilas anillo 0 + pool FS
  (`POOL_DIR` 0x600000 → 0x780000, `pila_anon` 0x580000/0x480000 →
  0x700000/0x600000)
- Topes: `exec.c`/`cdcat.c` `plano+bss<=0x300000` (+20 cabecera en
  catálogo), `build32.py` assert `<=0x300000`, `prog.ld` sin cambios
  (plano sigue enlazado en 0x200000).

## 2. Bug cazado (sistemático, -103 en todo)

- Primer intento puso args/esp en `0x530000` = final EXCLUSIVO de
  la región de pila mapeada (`0x510000+128K`): `pag_escribe_usr`
  fallaba siempre y ningún programa cargaba (ni el servidor;
  la shell arrancaba porque ella misma ya era proceso).
- Corrección: args/esp en el punto medio `0x520000`, como antes
  `0x3A0000` era el punto medio de `0x390000-0x3AFFFF`.
  Heap (`UBRK_INI`, `p->brk`) sí en `0x530000` (fin de pila).
- Lado usuario actualizado: `ulib.c` (`u_args`), `visor32.c`,
  `editor32.asm` (5 ocurrencias) → `0x520000`.

## 3. Resultado

- Build limpia: `kernel32.bin` 129016 B, ISO 5085184 B,
  `navega` 1270676 B + bss 20404 (< 3 MB).
- FULL: 736 pruebas (t1_lib 258 + tutf8 26 + mem32 15 + t_cpp 18
  + t_fb 286 + t_tls 8 + t_f32 59 + t_ns 24 + t_fetch 10
  + navega 32), 0 fallos. `t_red` excluido como siempre
  (slirp sin DNS/ruta en este entorno; no regresión).
