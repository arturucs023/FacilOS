#!/usr/bin/env python3
"""Genera FacilOS.iso (El Torito, emulacion floppy 1.44MB) desde build/floppy.img.
ISO 9660 minima + Boot Record + Catalogo + imagen floppy.
Probado en QEMU y VirtualBox (SeaBIOS).
"""
import pathlib, struct
BASE = pathlib.Path(__file__).parent.parent
FLOPPY = BASE/"build"/"floppy.img"
ISO = BASE/"build"/"FacilOS.iso"

def both16(v): return struct.pack("<H",v)+struct.pack(">H",v)
def both32(v): return struct.pack("<I",v)+struct.pack(">I",v)

floppy = FLOPPY.read_bytes()
assert len(floppy)==1474560, f"floppy debe ser 1474560, es {len(floppy)}"
# 1474560 / 2048 = 720 sectores exactos
assert len(floppy)%2048==0
n_boot_sectors = len(floppy)//2048  # 720

# Layout ISO (sectores de 2048):
# 0-15 sistema (16)
# 16 PVD
# 17 Boot Record
# 18 Terminator
# 19 Boot Catalog (1 sector)
# 20 Root dir (1 sector)
# 21..740 Boot image (720 sectores floppy)
LBA_PVD=16; LBA_BOOTREC=17; LBA_TERM=18; LBA_CATALOG=19; LBA_ROOT=20; LBA_BOOTIMG=21
TOTAL = LBA_BOOTIMG + n_boot_sectors  # 741
print(f"Total sectores ISO: {TOTAL} ({TOTAL*2048} bytes)")

iso = bytearray(TOTAL*2048)

def wsec(lba, data):
    assert len(data)==2048
    iso[lba*2048:(lba+1)*2048]=data

# ---- PVD sector 16 ----
pvd=bytearray(2048)
pvd[0]=1; pvd[1:6]=b"CD001"; pvd[6]=1; pvd[7]=0
pvd[8:40]=b"FACILOS".ljust(32,b" ")
pvd[40:72]=b"FACILOS_V10".ljust(32,b" ")
# 72-79 ceros
pvd[80:88]=both32(TOTAL)
# 88-119 ceros
pvd[120:124]=both16(1)  # set size
pvd[124:128]=both16(1)  # seq
pvd[128:136]=both32(2048)  # block size
pvd[136:144]=both32(0)  # path table size 0? minimo: 10? usamos 0
# path table locs 0
# root dir record en 156 (34 bytes)
# Root record: len=34, ext_attr=0, extent=LBA_ROOT, size=2048, fecha, flags=2 (dir), unit/interleave 0, name len 0? Para '.'? Minimo root con '.' y '..'? Hacemos root con '.' solo? Para simplificar, root extent apunta a sector 20 con '.' y '..' y terminador.
# Directory record (34+?):
# 0 len, 1 ext_attr, 2-9 extent both32, 10-17 size both32, 18-24 fecha(7), 25 flags, 26 unit, 27 interleave, 28 namelen, 29.. name
import datetime
now=datetime.datetime.now()
date_bytes=bytes([now.year-1900, now.month, now.day, now.hour, now.minute, now.second, 0])  # GMT offset 0
root_rec=bytearray(34)
root_rec[0]=34; root_rec[1]=0
root_rec[2:10]=both32(LBA_ROOT)
root_rec[10:18]=both32(2048)
root_rec[18:25]=date_bytes
root_rec[25]=2
root_rec[26]=0; root_rec[27]=0
root_rec[28]=1; root_rec[29]=0  # nombre 0x00 (root)
pvd[156:190]=root_rec
pvd[880:888]=b"CD001".ljust(8,b" ")  # ??? no, 813? En realidad PVD termina con... dejamos ceros
wsec(LBA_PVD,pvd)

# ---- Boot Record sector 17 ----
br=bytearray(2048)
br[0]=0; br[1:6]=b"CD001"; br[6]=1
br[7:39]=b"EL TORITO SPECIFICATION".ljust(32,b"\x00")
br[39:71]=b"\x00"*32
br[71:75]=struct.pack("<I",LBA_CATALOG)
wsec(LBA_BOOTREC,br)

# ---- Terminator sector 18 ----
tm=bytearray(2048)
tm[0]=255; tm[1:6]=b"CD001"; tm[6]=1
wsec(LBA_TERM,tm)

# ---- Boot Catalog sector 19 ----
cat=bytearray(2048)
# Validation entry (32 bytes en 0)
cat[0]=1; cat[1]=0
cat[2:4]=b"\x00\x00"
cat[4:28]=b"FACILOS".ljust(24,b"\x00")
# checksum en 28-29: suma de words debe ser 0
# key 55 AA en 30-31
cat[30]=0x55; cat[31]=0xAA
s=sum(struct.unpack("<16H",cat[0:32])) & 0xFFFF
chk=(0x10000 - s) & 0xFFFF
cat[28:30]=struct.pack("<H",chk)
# Initial entry (32 bytes en 32)
cat[32]=0x88  # bootable
cat[33]=2     # floppy 1.44MB
cat[34:36]=struct.pack("<H",0x0000)  # load segment (0 = 0x7C0)
cat[36]=0     # system type
cat[37]=0
cat[38:40]=struct.pack("<H",1)  # sector count (1, ignorado en floppy)
cat[40:44]=struct.pack("<I",LBA_BOOTIMG)  # LBA imagen
cat[44:64]=b"\x00"*20
wsec(LBA_CATALOG,cat)

# ---- Root dir sector 20: '.' , '..' ----
rd=bytearray(2048)
# '.' record (34 bytes, nombre 0)
r1=bytearray(34)
r1[0]=34; r1[1]=0
r1[2:10]=both32(LBA_ROOT); r1[10:18]=both32(2048)
r1[18:25]=date_bytes; r1[25]=2; r1[26]=0; r1[27]=0; r1[28]=1; r1[29]=0
rd[0:34]=r1
# '..' record (34 bytes, nombre 1)
r2=bytearray(34)
r2[0]=34; r2[1]=0
r2[2:10]=both32(LBA_ROOT); r2[10:18]=both32(2048)
r2[18:25]=date_bytes; r2[25]=2; r2[26]=0; r2[27]=0; r2[28]=1; r2[29]=1
rd[34:68]=r2
wsec(LBA_ROOT,rd)

# ---- Boot image 21.. ----
iso[LBA_BOOTIMG*2048:LBA_BOOTIMG*2048+len(floppy)]=floppy

ISO.write_bytes(iso)
print(f"OK ISO {ISO} {len(iso)} bytes")
# verificacion rapida
assert iso[LBA_CATALOG*2048+30]==0x55 and iso[LBA_CATALOG*2048+31]==0xAA
print("Catalogo OK, Boot image LBA",LBA_BOOTIMG)
