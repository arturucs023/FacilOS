#!/usr/bin/env python3
"""Inspecciona imagen v1.2/v1.6: cabecera, checksum, tabla, tramos y sumas."""
import pathlib, struct, sys
p = pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else pathlib.Path("build/FacilOS-v1.2-datos.img")
d = p.read_bytes()
print("tam:", len(d))
print("magic:", d[0:8])
lo, hi, ver = struct.unpack_from("<HHH", d, 8)
lay = d[14]
print(f"lo={lo} hi={hi} ver={ver} layout={lay}")
tabla = d[512:512+6*512][:64*42]
datos = d[7*512:7*512+64*512]
s = (sum(tabla)+sum(datos)) & 0xFFFFFFFF
print(f"checksum calc={(s&0xFFFF)}:{(s>>16)&0xFFFF} cab={lo}:{hi} ->", "OK" if (s&0xFFFF)==lo and ((s>>16)&0xFFFF)==hi else "MAL")
for i in range(64):
    e = tabla[i*42:(i+1)*42]
    if e[0]==0 and all(b==0 for b in e[1:]): continue
    try: nom = e[2:34].split(b"\x00")[0].decode()
    except: nom = repr(e[2:10])
    tam = struct.unpack_from("<H", e, 34)[0]
    seg = struct.unpack_from("<H", e, 36)[0]
    suma = struct.unpack_from("<I", e, 38)[0]
    extra = ""
    if e[0] == 2 and tam:
        if not seg:
            extra = " heredado"
        else:
            nsec = (tam + 511)//512
            if seg + nsec > (len(d)//512) - 7:
                extra = " SEG-FUERA"
            else:
                raw = b""
                for k in range(nsec):
                    raw += d[(7+seg+k)*512:(8+seg+k)*512]
                calc = sum(raw[:tam]) & 0xFFFFFFFF
                extra = f" seg={seg} nsec={nsec} suma={'OK' if calc==suma else f'MAL({calc:#x}!={suma:#x})'}"
    print(f"idx={i} tipo={e[0]} padre={e[1]} nombre={nom!r} tam={tam}{extra}")
