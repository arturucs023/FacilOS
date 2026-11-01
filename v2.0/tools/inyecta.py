#!/usr/bin/env python3
"""Mete un binario como archivo en raiz de una imagen v1.2 (copia). Uso:
inyecta.py ENTRADA.IMG SALIDA.IMG NOMBRE RUTA_BIN
v1.6: <=512 hereda sector idx (seg=0, legible en linea 16-bit); >512 tramo
>=64 first-fit con seg [36..37] y suma [38..41]. Checksum global igual.
"""
import pathlib, struct, sys
ent, sal, nom, binario = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
assert len(nom) <= 31 and '/' not in nom and ' ' not in nom
d = bytearray(pathlib.Path(ent).read_bytes())
data = pathlib.Path(binario).read_bytes()
assert len(data) <= 65535, "binario > 65535"
assert d[0:8] == b"FACILOS2"
tabla = d[512:512 + 6 * 512][:64 * 42]
usados = {i for i in range(64) if tabla[i * 42] != 0}
libre = next(i for i in range(1, 64) if i not in usados)
for i in range(64):
    e = tabla[i * 42:(i + 1) * 42]
    if e[0] and e[1] == 0 and e[2:34].split(b"\x00")[0].decode() == nom:
        raise SystemExit("ya existe")
nsec_total = len(d) // 512
e = bytearray(42)
e[0] = 2
e[2:2 + len(nom)] = nom.encode()
struct.pack_into("<H", e, 34, len(data))
if len(data) <= 512:
    sec = bytearray(512)
    sec[:len(data)] = data
    d[(7 + libre) * 512:(8 + libre) * 512] = sec
    detalle = f"idx={libre} heredado"
else:
    nsec = (len(data) + 511) // 512
    runs = []
    for i in range(64):
        ee = tabla[i * 42:(i + 1) * 42]
        if ee[0] != 2:
            continue
        sg = struct.unpack_from("<H", ee, 36)[0]
        if not sg:
            continue
        t2 = struct.unpack_from("<H", ee, 34)[0]
        runs.append((sg, (t2 + 511) // 512))
    base = None
    for b in range(64, nsec_total - 7 - nsec + 1):
        if all(b + nsec <= s or s + n <= b for s, n in runs):
            base = b
            break
    assert base is not None, "sin tramo libre"
    for k in range(nsec):
        trozo = data[k * 512:(k + 1) * 512]
        sec = bytearray(512)
        sec[:len(trozo)] = trozo
        d[(7 + base + k) * 512:(8 + base + k) * 512] = sec
    struct.pack_into("<H", e, 36, base)
    struct.pack_into("<I", e, 38, sum(data) & 0xFFFFFFFF)
    detalle = f"idx={libre} seg={base} nsec={nsec}"
d[512 + libre * 42:512 + (libre + 1) * 42] = e
tab = bytes(d[512:512 + 64 * 42])
dat = bytes(d[7 * 512:7 * 512 + 64 * 512])
s = (sum(tab) + sum(dat)) & 0xFFFFFFFF
struct.pack_into("<H", d, 8, s & 0xFFFF)
struct.pack_into("<H", d, 10, (s >> 16) & 0xFFFF)
pathlib.Path(sal).write_bytes(bytes(d))
print(f"OK {nom} tam={len(data)} {detalle} -> {sal}")
