#!/usr/bin/env python3
"""Genera FacilOS-v1.1.iso (El Torito floppy 1.44MB) desde build/floppy.img."""
import pathlib, struct, datetime
BASE = pathlib.Path(__file__).parent.parent
FLOPPY = BASE/"build"/"floppy.img"
ISO = BASE/"build"/"FacilOS-v1.1.iso"
def both16(v): return struct.pack("<H",v)+struct.pack(">H",v)
def both32(v): return struct.pack("<I",v)+struct.pack(">I",v)
floppy=FLOPPY.read_bytes()
assert len(floppy)==1474560
n=len(floppy)//2048
LBA_PVD=16; LBA_BOOTREC=17; LBA_TERM=18; LBA_CATALOG=19; LBA_ROOT=20; LBA_BOOTIMG=21
TOTAL=LBA_BOOTIMG+n
iso=bytearray(TOTAL*2048)
def wsec(lba,data):
    assert len(data)==2048
    iso[lba*2048:(lba+1)*2048]=data
pvd=bytearray(2048)
pvd[0]=1; pvd[1:6]=b"CD001"; pvd[6]=1
pvd[8:40]=b"FACILOS".ljust(32,b" ")
pvd[40:72]=b"FACILOS_V11".ljust(32,b" ")
pvd[80:88]=both32(TOTAL)
pvd[120:124]=both16(1); pvd[124:128]=both16(1); pvd[128:136]=both32(2048); pvd[136:144]=both32(0)
now=datetime.datetime.now()
db=bytes([now.year-1900,now.month,now.day,now.hour,now.minute,now.second,0])
rr=bytearray(34); rr[0]=34; rr[2:10]=both32(LBA_ROOT); rr[10:18]=both32(2048); rr[18:25]=db; rr[25]=2; rr[28]=1; rr[29]=0
pvd[156:190]=rr
wsec(LBA_PVD,pvd)
br=bytearray(2048); br[0]=0; br[1:6]=b"CD001"; br[6]=1
br[7:39]=b"EL TORITO SPECIFICATION".ljust(32,b"\x00")
br[71:75]=struct.pack("<I",LBA_CATALOG)
wsec(LBA_BOOTREC,br)
tm=bytearray(2048); tm[0]=255; tm[1:6]=b"CD001"; tm[6]=1
wsec(LBA_TERM,tm)
cat=bytearray(2048)
cat[0]=1; cat[4:28]=b"FACILOS11".ljust(24,b"\x00"); cat[30]=0x55; cat[31]=0xAA
s=sum(struct.unpack("<16H",cat[0:32]))&0xFFFF
cat[28:30]=struct.pack("<H",(0x10000-s)&0xFFFF)
cat[32]=0x88; cat[33]=2; cat[34:36]=struct.pack("<H",0); cat[38:40]=struct.pack("<H",1); cat[40:44]=struct.pack("<I",LBA_BOOTIMG)
wsec(LBA_CATALOG,cat)
rd=bytearray(2048)
r1=bytearray(34); r1[0]=34; r1[2:10]=both32(LBA_ROOT); r1[10:18]=both32(2048); r1[18:25]=db; r1[25]=2; r1[28]=1; r1[29]=0
rd[0:34]=r1
r2=bytearray(34); r2[0]=34; r2[2:10]=both32(LBA_ROOT); r2[10:18]=both32(2048); r2[18:25]=db; r2[25]=2; r2[28]=1; r2[29]=1
rd[34:68]=r2
wsec(LBA_ROOT,rd)
iso[LBA_BOOTIMG*2048:LBA_BOOTIMG*2048+len(floppy)]=floppy
ISO.write_bytes(iso)
print(f"OK {ISO} {len(iso)} bytes")
