#!/usr/bin/env python3
"""Crea FacilOS-datos.img (10MB) preformateado: cabecera + tabla vacia + datos cero + checksum.
Layout: LBA0 cabecera (magic FACILOS1, lo, hi, version), LBA1-5 tabla (64*36), LBA6-69 datos (64*512).
Checksum = suma32 de tabla+datos (bytes).
"""
import pathlib, struct
BASE = pathlib.Path(__file__).parent.parent
IMG = BASE/"build"/"FacilOS-datos.img"
SECT=512; TOTAL_SECT=20480  # 10MB
img=bytearray(TOTAL_SECT*SECT)
# tabla vacia con raiz
tabla=bytearray(5*SECT)
tabla[0]=1; tabla[1]=0; tabla[2:3]=b"/"; tabla[3]=0
# tam raiz en offset 34-35 (ya 0)
datos=bytearray(64*SECT)
s=sum(tabla)+sum(datos)
s&=0xFFFFFFFF
lo=s&0xFFFF; hi=(s>>16)&0xFFFF
cab=bytearray(SECT)
cab[0:8]=b"FACILOS1"
struct.pack_into("<H",cab,8,lo)
struct.pack_into("<H",cab,10,hi)
struct.pack_into("<H",cab,12,1)
img[0:512]=cab
img[1*512:6*512]=tabla
img[6*512:70*512]=datos
IMG.write_bytes(img)
print(f"OK {IMG} {len(img)} bytes, checksum {s:#010x} (lo={lo} hi={hi})")
