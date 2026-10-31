#!/usr/bin/env python3
"""Inspecciona imagen v1.1: cabecera FACILOS1, tabla 64x36 LBA1-5, datos LBA6."""
import pathlib, struct, sys
p = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("build/mig-test.img")
d = p.read_bytes()
print("tam:", len(d))
print("magic:", d[0:8])
lo, hi, ver = struct.unpack_from("<HHH", d, 8)
print(f"lo={lo} hi={hi} ver={ver}")
tabla = d[512:512 + 5 * 512][:64 * 36]
datos = d[6 * 512:6 * 512 + 64 * 512]
s = (sum(tabla) + sum(datos)) & 0xFFFFFFFF
print(f"checksum calc={s & 0xFFFF}:{s >> 16} cab={lo}:{hi} ->",
      "OK" if (s & 0xFFFF) == lo and ((s >> 16) & 0xFFFF) == hi else "MAL")
for i in range(64):
    e = tabla[i * 36:(i + 1) * 36]
    if e[0] == 0 and all(b == 0 for b in e[1:]):
        continue
    try:
        nom = e[2:34].split(b"\x00")[0].decode()
    except Exception:
        nom = repr(e[2:10])
    tam = struct.unpack_from("<H", e, 34)[0]
    print(f"idx={i} tipo={e[0]} padre={e[1]} nombre={nom!r} tam={tam}")
    if e[0] == 2 and tam:
        seg = datos[i * 512:(i + 1) * 512][:tam]
        print("   contenido:", seg[:60])
