#!/usr/bin/env python3
"""Construye la linea 32-bit de FacilOS (v1.8):
nasm boot32 + nasm entry32 + gcc C + link + objcopy + ISO.
La ISO lleva el arranque, el nucleo y TODOS los programas (catalogo
F32C, solo lectura). Con --disco ademas genera la imagen de datos
(opcional, para poder guardar).
"""
import pathlib, struct, subprocess, sys, datetime
WANT_DISK = "--disco" in sys.argv
BASE = pathlib.Path(__file__).parent.parent
K32 = BASE / "src" / "kernel32"
USR = BASE / "user"
APPS = USR / "apps"
LIB = USR / "lib"
LD = USR / "ld"
BLD = BASE / "build" / "bin"
ISO_DIR = BASE / "build" / "iso"
IMG_DIR = BASE / "build" / "images"
for _d in (BLD, ISO_DIR, IMG_DIR):
    _d.mkdir(parents=True, exist_ok=True)
GCC = r"C:\Users\artur\gcc32\mingw32\bin\gcc.exe"
NASM = str(BASE / "tools" / "nasm" / "nasm-3.01" / "nasm.exe")
C32FLAGS = ["-ffreestanding", "-nostdlib", "-m32", "-march=i386",
            "-O1", "-Wall", "-fno-pic", "-fno-pie", "-fno-stack-protector",
            "-fzero-initialized-in-bss"]
PROGFLAGS = C32FLAGS + ["-Os", "-ffunction-sections", "-fdata-sections"]

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("FALLO:", " ".join(cmd)); print(r.stdout); print(r.stderr)
        sys.exit(1)

run([NASM, "-f", "bin", str(K32 / "boot32.asm"), "-o", str(BLD / "boot32.bin")])
assert (BLD / "boot32.bin").stat().st_size == 512, "boot32 != 512"
run([NASM, "-f", "win32", str(K32 / "entry32.asm"), "-o", str(BLD / "entry32.o")])
run([NASM, "-f", "win32", str(K32 / "idt32.asm"), "-o", str(BLD / "idt32.o")])
for c in sorted(K32.glob("*.c")):
    run([GCC] + C32FLAGS + ["-c", str(c), "-o", str(BLD / (c.stem + ".o"))])
objs = [str(BLD / "entry32.o"), str(BLD / "idt32.o")] + [str(BLD / (c.stem + ".o")) for c in sorted(K32.glob("*.c"))]
run([GCC, "-m32", "-nostdlib", "-Wl,--image-base,0x10000",
     "-Wl,-e,_kmain_entry", "-T", str(K32 / "link32.ld"),
     "-o", str(BLD / "kernel32.pe")] + objs + ["-lgcc"])
run([str(pathlib.Path(GCC).parent / "objcopy.exe"), "-O", "binary",
     str(BLD / "kernel32.pe"), str(BLD / "kernel32.bin")])
kb = (BLD / "kernel32.bin").stat().st_size
print(f"kernel32.bin: {kb} bytes")
assert kb <= 131072, "nucleo32 > 256 sectores"

floppy = bytearray(1474560)
floppy[0:512] = (BLD / "boot32.bin").read_bytes()
floppy[512:512 + kb] = (BLD / "kernel32.bin").read_bytes()
(IMG_DIR / "floppy32.img").write_bytes(bytes(floppy))

# v1.8: TODO dentro de la ISO. El BIOS entrega al arranque la disquete
# virtual de El Torito, que es la imagen que metemos en el LBA 21. Ahi va:
#   LBA 0 = arranque | 1 = cabecera F32H | 2.. = nucleo | cat | datos
# El cargador y el nucleo leen de ahi con int 0x13 AH=42 (solo lectura).
IMG_LBA = 21
IMG_F32H = 1
IMG_KERN = 2
iso = bytearray(2048 * 320)
CAT_LBA = 0
DATOS_LBA = 0
def both16(v): return struct.pack("<H", v) + struct.pack(">H", v)
def both32(v): return struct.pack("<I", v) + struct.pack(">I", v)
def wsec(lba, data):
    assert len(data) == 2048, len(data)
    iso[lba * 2048:(lba + 1) * 2048] = data
pvd = bytearray(2048)
pvd[0] = 1; pvd[1:6] = b"CD001"; pvd[6] = 1
pvd[8:40] = b"FACILOS".ljust(32, b" ")
pvd[40:72] = b"FACILOS_V20_32".ljust(32, b" ")
struct.pack_into("<I", pvd, 80, len(iso) // 2048)
struct.pack_into(">I", pvd, 84, len(iso) // 2048)
now = datetime.datetime.now()
db = bytes([now.year - 1900, now.month, now.day, now.hour, now.minute, now.second, 0])
rr = bytearray(34); rr[0] = 34; rr[2:10] = both32(20); rr[10:18] = both32(2048)
rr[18:25] = db; rr[25] = 2; rr[28] = 1; rr[29] = 0
pvd[156:190] = rr
wsec(16, pvd)
br = bytearray(2048); br[0] = 0; br[1:6] = b"CD001"; br[6] = 1
# Layout validado en QEMU/SeaBIOS: la plataforma empieza en 7 y el byte 0
# vale 0. (La forma "correcta" segun el EL TORITO spec, con 0x01 y偏移 8,
# hace que SeaBIOS rechace el CD: comprobado.)
br[7:39] = b"EL TORITO SPECIFICATION".ljust(32, b"\x00")
br[71:75] = struct.pack("<I", 19)
wsec(17, br)
tm = bytearray(2048); tm[0] = 255; tm[1:6] = b"CD001"; tm[6] = 1
wsec(18, tm)
cat = bytearray(2048)
cat[0] = 1; cat[4:28] = b"FACILOS1832".ljust(24, b"\x00"); cat[30] = 0x55; cat[31] = 0xAA
s = sum(struct.unpack("<16H", cat[0:32])) & 0xFFFF
cat[28:30] = struct.pack("<H", (0x10000 - s) & 0xFFFF)
cat[32] = 0x88          # arrancable
cat[33] = 2             # medio: emulacion de disquete (lo unico que el
cat[34] = 0x00          # BIOS es capa de leer la ISO; ver INFORME)
cat[35] = 0x00
cat[36] = 0x00          # tipo de sistema (0 = BIOS)
cat[37] = 0x00
cat[38:40] = struct.pack("<H", 1)     # 1 sector al BIOS (solo el arranque);
cat[40:44] = struct.pack("<I", IMG_LBA)   # con 0 intenta cargar 1.44 MB y
                                         # cuelga (verificado en SeaBIOS)
wsec(19, cat)
rd = bytearray(2048)
r1 = bytearray(34); r1[0] = 34; r1[2:10] = both32(20); r1[10:18] = both32(2048)
r1[18:25] = db; r1[25] = 2; r1[28] = 1; r1[29] = 0
rd[0:34] = r1
r2 = bytearray(34); r2[0] = 34; r2[2:10] = both32(20); r2[10:18] = both32(2048)
r2[18:25] = db; r2[25] = 2; r2[28] = 1; r2[29] = 1
rd[34:68] = r2
wsec(20, rd)
# sector 21: arranque. El nucleo va en 23.. y la cabecera F32H en 22.
# Imagen de arranque de 1.44 MB: lo que SeaBIOS exige para emulacion de
# disquete (v1.8: probados sin-emulacion, imagen corta y sector-count 0;
# todos fallan en este BIOS; ver INFORME-v1.8-32.txt). Contenido util
# ~130 KB; el resto es relleno que el BIOS exige.
KERN_SECS = 128          # sectores de 512 que lee el cargador
IMG_SECS = 2880
img = bytearray(IMG_SECS * 512)
CAT_LBA = KERN_SECS + 1  # LBA del catalogo (0 = arranque, 1.. = nucleo)
# v1.9: el catalogo ocupa 4 sectores (2048 B) para que quepan 32 programas
# (12 B de cabecera + 32 entradas de 32 B); con 512 B solo cabian 15.
CAT_SECS = 4
DATOS_LBA = CAT_LBA + CAT_SECS
# El cargador lee LBAs 1..128 (el nucleo) y despues copia el catalogo de
# programas + sus datos a 0x20000 en RAM (ver boot32.asm).
img[512:512 + kb] = (BLD / "kernel32.bin").read_bytes()
assert kb <= 128 * 512, "nucleo > 128 sectores de 512"
# El arranque: pay_secs en 496, cat_lba en 498, dat_lba en 500.
_bs = bytearray((BLD / "boot32.bin").read_bytes())
struct.pack_into("<HHH", _bs, 496, 0, CAT_LBA, DATOS_LBA)  # pay luego
assert _bs[510] == 0x55 and _bs[511] == 0xAA, "firma del arranque"
img[0:512] = bytes(_bs)
# catalogo de programas (sector CAT_LBA) + datos alineados a 2048
def cdcat_build(nombres):
    """Rellena el indice y los datos dentro de la imagen; devuelve LBA final."""
    assert CAT_LBA > IMG_KERN, "catalogo solapado con el nucleo"
    cab = bytearray(2048)
    cab[0:4] = b"F32C"
    cab[4] = 1
    cab[5] = len(nombres)
    struct.pack_into("<I", cab, 8, DATOS_LBA)
    lba = DATOS_LBA
    for i, nom in enumerate(nombres):
        data = (BLD / nom).read_bytes()
        off = 12 + i * 32
        cab[off:off + 15] = nom.encode()[:15].ljust(15, b"\x00")
        cab[off + 15] = 2
        struct.pack_into("<I", cab, off + 16, lba)
        struct.pack_into("<I", cab, off + 20, len(data))
        img[lba * 512:lba * 512 + len(data)] = data
        lba += (len(data) + 511) // 512
    img[CAT_LBA * 512:(CAT_LBA + CAT_SECS) * 512] = cab[:CAT_SECS * 512]
    return lba


# programas ejemplo F32X (todos con ulib + start)
run([NASM, "-f", "win32", str(LIB / "start.asm"),
     "-o", str(BLD / "start.o")])
run([GCC] + PROGFLAGS + ["-I", str(LIB), "-I", str(APPS), "-c",
     str(LIB / "ulib.c"), "-o", str(BLD / "ulib.o")])
import struct as _st

_datos_img = IMG_DIR / "FacilOS-v1.8-32-datos.img"


def haz_programa(nom, primero, fuentes=None, ver=2, con_ulib=False,
                 al_disco=True):
    if nom == "editor32":
        run([NASM, "-f", "win32", str(APPS / "editor32.asm"),
             "-o", str(BLD / "editor32.o")])
        run([GCC, "-m32", "-nostdlib", "-Wl,--image-base,0x10000",
             "-Wl,-e,_start",
             "-T", str(LD / "prog.ld"),
             "-o", str(BLD / "editor32.pe"),
             str(BLD / "editor32.o")])
    else:
        objs = [str(BLD / "start.o")]
        for fs_ in (fuentes or [nom + ".c"]):
            obj = str(BLD / (pathlib.Path(fs_).stem + ".o"))
            run([GCC] + PROGFLAGS + ["-I", str(LIB), "-I", str(APPS), "-c",
                 str(APPS / fs_), "-o", obj])
            objs.append(obj)
        if con_ulib:
            objs.append(str(BLD / "ulib.o"))
        run([GCC, "-m32", "-nostdlib", "-Wl,--image-base,0x10000",
             "-Wl,-e,_start", "-Wl,--gc-sections",
             "-T", str(LD / "prog.ld"),
             "-o", str(BLD / (nom + ".pe"))] + objs)
    run([str(pathlib.Path(GCC).parent / "objcopy.exe"), "-O", "binary",
         str(BLD / (nom + ".pe")), str(BLD / (nom + ".plano"))])
    plano = (BLD / (nom + ".plano")).read_bytes()
    r = subprocess.run(
        [str(pathlib.Path(GCC).parent / "objdump.exe"), "-h",
         str(BLD / (nom + ".pe"))],
        capture_output=True, text=True)
    bss_tam = 0
    for ln in r.stdout.splitlines():
        p = ln.split()
        if len(p) >= 4 and p[1] == ".bss":
            bss_tam = int(p[2], 16)
            break
    if ver == 2:
        assert len(plano) + 20 <= 512, nom + " no cabe en 512"
        assert bss_tam <= 0x10000, nom + " bss enorme"
    else:
        assert len(plano) + 20 <= 65535, nom + " no cabe en 64K"
        assert bss_tam <= 0x40000, nom + " bss enorme"
        assert len(plano) + bss_tam <= 0xC0000, nom + " +bss > 768K"
    cab = b"F32X" + _st.pack("<IIII", ver, 0, len(plano) + 20, bss_tam)
    (BLD / nom).write_bytes(cab + plano)
    print(f"OK programa {nom} v{ver} ({len(plano) + 20} bytes, bss {bss_tam})")
    if al_disco:
        base = (str(BASE / "build" / "FacilOS-v1.3-datos.img") if primero
                else str(_datos_img))
        run([sys.executable, str(BASE / "tools" / "inyecta.py"),
             base, str(_datos_img), nom, str(BLD / nom)])


_programas = ["hola", "editor32", "visor32", "paint32", "benchmark32"]
for _i, _nom in enumerate(_programas):
    haz_programa(_nom, _i == 0, al_disco=(WANT_DISK))
_apps3 = [
    ("procesos32", ["procesos32.c"], True),
    ("mem32", ["mem32.c"], True),
    ("planificador32", ["planificador32.c"], True),
    ("contador32", ["contador32.c"], True),
    ("dormir32", ["dormir32.c"], True),
    ("aislamiento32", ["aislamiento32.c"], True),
    ("aisla32", ["aisla32.c"], True),
    ("t1_lib", ["t1_lib.c"], True),
    ("t2_gfx", ["t2_gfx.c"], True),
    ("editor16", ["editor16.c"], True),
    ("demo16", ["d_main.c", "d_graficos.c", "d_ficheros.c", "d_memoria.c",
                "d_banco.c"], True),
    ("menu", ["menu.c"], True),
    ("calculadora", ["calculadora.c"], True),
    ("reloj", ["reloj.c"], True),
    ("adivina", ["adivina.c"], True),
]
for _nom, _fs, _u in _apps3:
    haz_programa(_nom, False, _fs, 3, _u, al_disco=WANT_DISK)

# ---- escribir la ISO con el catalogo (todo dentro del CD) ----
_todos = _programas + [a[0] for a in _apps3]
_fin_img = cdcat_build(_todos)
# El arranque copia desde CAT_LBA hasta el final de los datos: eso son
# (_fin_img - CAT_LBA) sectores de 512. Se lo parcheamos ahora.
struct.pack_into("<H", img, 496, _fin_img - CAT_LBA)
_bs2 = bytearray(img[0:512])
img[0:512] = bytes(_bs2)
# meter la imagen completa (1.44 MB) en el LBA IMG_LBA
_nimg = IMG_SECS
iso[IMG_LBA * 2048:IMG_LBA * 2048 + _nimg * 512] = bytes(img)
_fin = IMG_LBA + (_nimg * 512 + 2047) // 2048
# el PVD declara el tamano real (si no, el BIOS lee mas alla del EOF)
struct.pack_into("<I", pvd, 80, _fin)
struct.pack_into(">I", pvd, 84, _fin)
wsec(16, pvd)
out_iso = ISO_DIR / "FacilOS-v2.0-32.iso"
_iso = bytes(iso[:_fin * 2048])
out_iso.write_bytes(_iso)
print(f"OK {out_iso} {len(_iso)} bytes ({_fin} sectores, "
      f"{len(_todos)} programas dentro; util {kb + _fin_img * 512} B, "
      f"cat LBA {CAT_LBA}, datos LBA {DATOS_LBA})")
if WANT_DISK:
    print(f"OK {_datos_img} {_datos_img.stat().st_size} bytes (disco opcional)")
