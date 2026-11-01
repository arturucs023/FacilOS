#!/usr/bin/env python3
"""voldisco.py: herramienta de volumen FACILOS2/v1.2 (etapa 3).
Solo inspecciona y formatea en el ANFITRION; el kernel accede por si mismo.
Uso:
  voldisco.py mkfs IMAGEN [SECTORES]    crea imagen vacia valida (por defecto 20480 = 10 MB)
  voldisco.py verifica IMAGEN           comprueba firma/checksum/tabla/tramos
  voldisco.py lista IMAGEN             lista archivos y directorios
No toca inyecta.py/mkdatos.py. Sin dependencias externas.
"""
import pathlib, struct, sys

SECT = 512


def lee_tabla(d):
    return [bytes(d[512 + i * 42:512 + (i + 1) * 42]) for i in range(64)]


def checksum(d):
    tab = bytes(d[512:512 + 64 * 42])
    dat = bytes(d[7 * 512:7 * 512 + 64 * 512])
    return (sum(tab) + sum(dat)) & 0xFFFFFFFF


def cmd_mkfs(img, nsec=20480):
    d = bytearray(nsec * SECT)
    d[512] = 1
    d[512 + 1] = 0
    d[512 + 2:512 + 3] = b"/"
    cab = bytearray(SECT)
    cab[0:8] = b"FACILOS2"
    struct.pack_into("<H", cab, 12, 2)
    d[0:512] = cab
    s = checksum(d)
    struct.pack_into("<H", d, 8, s & 0xFFFF)
    struct.pack_into("<H", d, 10, (s >> 16) & 0xFFFF)
    pathlib.Path(img).write_bytes(bytes(d))
    print(f"OK mkfs {img} {nsec} sectores")


def cmd_verifica(img):
    d = bytearray(pathlib.Path(img).read_bytes())
    if len(d) % SECT:
        print("FALLO tamano no multiplo de 512");
        return 1
    nsec = len(d) // SECT
    mal = 0

    def queja(m):
        nonlocal mal
        mal += 1
        print("FALLO", m)

    if d[0:8] != b"FACILOS2":
        queja("sin firma FACILOS2");
        return 1
    if d[12] != 2:
        queja(f"version={d[12]} (se espera 2)");
    s = checksum(d)
    if (d[8] | (d[9] << 8)) != (s & 0xFFFF) or \
       (d[10] | (d[11] << 8)) != ((s >> 16) & 0xFFFF):
        queja("checksum no cuadra");
    tab = lee_tabla(d)
    if tab[0][0] != 1:
        queja("raiz no es directorio");
    usados = []
    for i, e in enumerate(tab):
        tipo = e[0]
        if tipo not in (0, 1, 2):
            queja(f"entrada {i}: tipo={tipo}");
            continue
        if not tipo:
            continue
        nom = e[2:34].split(b"\x00")[0]
        if not nom or len(nom) > 31 or (b"/" in nom and not (i == 0 and nom == b"/")):
            queja(f"entrada {i}: nombre invalido");
        tam = e[34] | (e[35] << 8)
        seg = e[36] | (e[37] << 8)
        suma = e[38] | (e[39] << 8) | (e[40] << 16) | (e[41] << 24)
        if tam > 65535:
            queja(f"entrada {i}: tam enorme");
        nsec_f = (tam + 511) // 512 if tam else 0
        if not seg:
            if nsec_f > 1:
                queja(f"entrada {i}: heredado con tam>512");
        else:
            if seg < 64:
                queja(f"entrada {i}: seg<{64}");
            if seg + nsec_f > nsec - 7:
                queja(f"entrada {i}: tramo fuera del volumen");
            for (s2, n2, j) in usados:
                if seg < s2 + n2 and s2 < seg + nsec_f:
                    queja(f"entrada {i}: solapa con {j}");
            # suma de contenido
            s = 0
            for k in range(nsec_f):
                off = (7 + seg + k) * SECT
                trozo = min(512, tam - k * 512)
                s += sum(d[off:off + trozo])
            if (s & 0xFFFFFFFF) != suma:
                queja(f"entrada {i}: suma no cuadra");
            usados.append((seg, nsec_f, i))
    if not mal:
        print(f"OK verifica {img}: {nsec} sectores, formato valido");
    return 1 if mal else 0


def cmd_lista(img):
    d = bytearray(pathlib.Path(img).read_bytes())
    if d[0:8] != b"FACILOS2":
        print("sin firma FACILOS2");
        return 1
    for i, e in enumerate(lee_tabla(d)):
        if not e[0]:
            continue
        nom = e[2:34].split(b"\x00")[0].decode("ascii", "replace")
        tam = e[34] | (e[35] << 8)
        seg = e[36] | (e[37] << 8)
        print(f"{'D' if e[0] == 1 else 'F'} {i:2d} padre={e[1]:2d} "
              f"tam={tam:5d} seg={seg:5d} {nom}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__);
        sys.exit(2)
    cmd, img = sys.argv[1], sys.argv[2]
    if cmd == "mkfs":
        nsec = int(sys.argv[3]) if len(sys.argv) > 3 else 20480
        cmd_mkfs(img, nsec)
    elif cmd == "verifica":
        sys.exit(cmd_verifica(img))
    elif cmd == "lista":
        sys.exit(cmd_lista(img))
    else:
        print(__doc__);
        sys.exit(2)
