#!/usr/bin/env python3
"""Crea FacilOS-v1.3-datos.img (10MB) formato v1.2:
LBA0 cabecera (FACILOS2, sum_lo, sum_hi, version=2),
LBA1-6 tabla (64x42, raiz con fecha 0), LBA7-70 datos ceros. Checksum32.
"""
import pathlib, struct, datetime
BASE = pathlib.Path(__file__).parent.parent
IMG = BASE/"build"/"FacilOS-v1.3-datos.img"
SECT=512; TOTAL=20480
img=bytearray(TOTAL*SECT)
tabla=bytearray(6*SECT)
tabla[0]=1; tabla[1]=0; tabla[2:3]=b"/"; tabla[3]=0
datos=bytearray(64*SECT)
s=(sum(tabla)+sum(datos))&0xFFFFFFFF
cab=bytearray(SECT)
cab[0:8]=b"FACILOS2"
struct.pack_into("<H",cab,8,s&0xFFFF)
struct.pack_into("<H",cab,10,(s>>16)&0xFFFF)
struct.pack_into("<H",cab,12,2)
cab[14]=1
img[0:512]=cab
img[1*512:7*512]=tabla
img[7*512:71*512]=datos
IMG.write_bytes(img)
print(f"OK {IMG} {len(img)} checksum {s:#010x}")
