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
GXX = r"C:\Users\artur\gcc32\mingw32\bin\g++.exe"
NASM = str(BASE / "tools" / "nasm" / "nasm-3.01" / "nasm.exe")
C32FLAGS = ["-ffreestanding", "-nostdlib", "-m32", "-march=i386",
            "-O1", "-Wall", "-fno-pic", "-fno-pie", "-fno-stack-protector",
            "-fzero-initialized-in-bss"]
PROGFLAGS = C32FLAGS + ["-Os", "-ffunction-sections", "-fdata-sections"]
# F32 C++ (v2.4.2): mismo modelo que C, sin excepciones/RTTI/libstdc++.
# -fno-asynchronous-unwind-tables evita .eh_frame (prog.ld lo descarta).
# -fuse-cxa-atexit: destructores via __cxa_atexit del runtime (MinGW
# por defecto usaria atexit de su CRT, que aqui no existe).
CXXFLAGS = PROGFLAGS + ["-fno-exceptions", "-fno-rtti",
                        "-fno-asynchronous-unwind-tables",
                        "-fuse-cxa-atexit"]

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("FALLO:", " ".join(cmd)); print(r.stdout); print(r.stderr)
        sys.exit(1)

run([NASM, "-f", "bin", str(K32 / "boot32.asm"), "-o", str(BLD / "boot32.bin")])
assert (BLD / "boot32.bin").stat().st_size == 1024, "boot32 != 1024 (F8)"
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

# F8: TODO dentro de la ISO, sin limite de 1,44 MB. Arranque por
# no-emulacion El Torito + EDD (INT 13h AH=42h): el BIOS carga 2
# sectores de 512 (boot32 de 1024 B) en 0x7C00 y el cargador lee
# por LBA absoluta del CD. Ahi va (sectores logicos de 512):
#   LBA 0 = arranque (sector 1) | 1..224 = nucleo | 225 = catalogo
#   F32C (4 sect) | 229.. = datos. En el CD van empaquetados de 4
#   en 4 en sectores de 2048 desde PAYLOAD_LBA. La RAM destino no
#   cambia (0x30000 baja / 0x1000000 alta), asi que cd.c sigue igual.
BOOT_LBA = 21        # sector CD (2048) con los 1024 B de arranque
PAYLOAD_LBA = 22    # sector CD del logico 0
iso = None          # se dimensiona al final (tamano = contenido)
CAT_LBA = 0
DATOS_LBA = 0
def both16(v): return struct.pack("<H", v) + struct.pack(">H", v)
def both32(v): return struct.pack("<I", v) + struct.pack(">I", v)
iso_sec = {}   # F8: sectores CD (2048) dispersos; la ISO final se ensambla al final
def wsec(lba, data):
    assert len(data) == 2048, len(data)
    iso_sec[lba] = bytes(data)
pvd = bytearray(2048)
pvd[0] = 1; pvd[1:6] = b"CD001"; pvd[6] = 1
pvd[8:40] = b"FACILOS".ljust(32, b" ")
pvd[40:72] = b"FACILOS_V20_32".ljust(32, b" ")
# F8: tamano real se parchea al ensamblar (depende del contenido).
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
cat[33] = 0             # F8: sin emulacion (antes 2 = disquete 1.44 MB)
cat[34] = 0x00          # el BIOS carga `count` sectores de 512 en 0x7C00
cat[35] = 0x00
cat[36] = 0x00          # tipo de sistema (0 = BIOS)
cat[37] = 0x00
cat[38:40] = struct.pack("<H", 2)     # boot32 mide 1024 B (2 sectores)
cat[40:44] = struct.pack("<I", BOOT_LBA)
wsec(19, cat)
rd = bytearray(2048)
r1 = bytearray(34); r1[0] = 34; r1[2:10] = both32(20); r1[10:18] = both32(2048)
r1[18:25] = db; r1[25] = 2; r1[28] = 1; r1[29] = 0
rd[0:34] = r1
r2 = bytearray(34); r2[0] = 34; r2[2:10] = both32(20); r2[10:18] = both32(2048)
r2[18:25] = db; r2[25] = 2; r2[28] = 1; r2[29] = 1
rd[34:68] = r2
wsec(20, rd)
# F8: payload logico de 512 B (sin tope de 1,44 MB): el arranque
# (boot32 de 1024 B) ocupa su propio sector CD; aqui solo van los
# logicos [0..fin): 0 = arranque (sector 1), 1..224 = nucleo,
# 225 = catalogo F32C (4 sect), 229.. = datos (ver cdcat_build).
# El tamano final lo dicta el contenido (10/50/100 MB posibles).
KERN_SECS = 256          # sectores logicos de nucleo (v2.4-USB: era 240)
img = bytearray(225 * 512)
CAT_LBA = KERN_SECS + 1  # = 225 (0 = arranque, 1.. = nucleo)
# v1.9: el catalogo ocupa 4 sectores (2048 B) para que quepan 32 programas
# (12 B de cabecera + 32 entradas de 32 B); con 512 B solo cabian 15.
CAT_SECS = 4
DATOS_LBA = CAT_LBA + CAT_SECS
# El cargador lee LBAs 1..KERN_SECS (el nucleo) y despues la copia
# baja [cat, 993) a 0x30000 y la alta [993, fin) a 0x1000000 (EDD).
img[512:512 + kb] = (BLD / "kernel32.bin").read_bytes()
assert kb <= KERN_SECS * 512, "nucleo > sectores del cargador"
# NOTA F8: copyalta.asm se conserva como historia (ISOs f7x); el nuevo
# arranque carga la cola alta directo por EDD y ya no la usa/inyecta.
# El arranque: pay_secs en 496, cat_lba en 498, dat_lba en 500;
# iso_base (u32, LBA CD del logico 0) tras el marcador 'BFSX'
# del sector 2 (busqueda, nunca offset fijo: antes pisaba codigo).
_bs = bytearray((BLD / "boot32.bin").read_bytes())
assert len(_bs) == 1024, "boot32 debe medir 1024 B (F8, 2 sectores)"
struct.pack_into("<HHH", _bs, 496, 0, CAT_LBA, DATOS_LBA)  # pay luego
_mag = bytes(_bs).find(b'BFSX')
assert _mag >= 512 and _mag + 8 <= len(_bs), "sin marcador BFSX"
struct.pack_into("<I", _bs, _mag + 4, PAYLOAD_LBA)
assert _bs[510] == 0x55 and _bs[511] == 0xAA, "firma del arranque"
img[0:512] = bytes(_bs[0:512])
# catalogo de programas (sector CAT_LBA) + datos alineados a 2048
def cdcat_build(nombres):
    """Rellena el indice y los datos dentro de la imagen; devuelve LBA final."""
    assert CAT_LBA > 2, "catalogo solapado con el nucleo"
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
        # F8: la imagen crece con el contenido (sin tope de 2880).
        need = lba * 512 + len(data)
        if len(img) < need:
            img.extend(bytes(need - len(img)))
        img[lba * 512:lba * 512 + len(data)] = data
        lba += (len(data) + 511) // 512
    img[CAT_LBA * 512:(CAT_LBA + CAT_SECS) * 512] = cab[:CAT_SECS * 512]
    return lba


# programas ejemplo F32X (todos con ulib + start)
run([NASM, "-f", "win32", str(LIB / "start.asm"),
     "-o", str(BLD / "start.o")])
# F32 C++ (v2.4.2): entrada con init/fini + runtime (new/delete,
# ctores globales, guardas, atexit). start.asm de C queda intacto.
run([NASM, "-f", "win32", str(LIB / "startxx.asm"),
     "-o", str(BLD / "startxx.o")])
run([GXX] + CXXFLAGS + ["-I", str(LIB), "-I", str(APPS), "-c",
     str(LIB / "f32cxx.cpp"), "-o", str(BLD / "f32cxx.o")])
run([GCC] + PROGFLAGS + ["-I", str(LIB), "-I", str(APPS), "-c",
     str(LIB / "ulib.c"), "-o", str(BLD / "ulib.o")])
run([GCC] + PROGFLAGS + ["-I", str(LIB), "-I", str(APPS), "-c",
     str(LIB / "ugfx.c"), "-o", str(BLD / "ugfx.o")])
run([GCC] + PROGFLAGS + ["-I", str(LIB), "-I", str(APPS), "-c",
     str(LIB / "utext.c"), "-o", str(BLD / "utext.o")])
run([GCC] + PROGFLAGS + ["-I", str(LIB), "-I", str(APPS), "-c",
     str(LIB / "ulog.c"), "-o", str(BLD / "ulog.o")])
import struct as _st

_datos_img = IMG_DIR / "FacilOS-v1.8-32-datos.img"


def haz_programa(nom, primero, fuentes=None, ver=2, con_ulib=False,
                 al_disco=True, con_ugfx=False):
    if nom == "editor32":
        run([NASM, "-f", "win32", str(APPS / "editor32.asm"),
             "-o", str(BLD / "editor32.o")])
        run([GCC, "-m32", "-nostdlib", "-Wl,--image-base,0x10000",
             "-Wl,-e,_start",
             "-T", str(LD / "prog.ld"),
             "-o", str(BLD / "editor32.pe"),
             str(BLD / "editor32.o")])
    else:
        objs = []
        es_cpp = any(str(fs_).endswith(".cpp") for fs_ in (fuentes or [nom + ".c"]))
        # F32 C++: entrada con init/fini + runtime; las apps C++ no
        # definen __main (lo aporta f32cxx). Flujo C intacto.
        objs.append(str(BLD / ("startxx.o" if es_cpp else "start.o")))
        CC = GXX if es_cpp else GCC
        CF = CXXFLAGS if es_cpp else PROGFLAGS
        for fs_ in (fuentes or [nom + ".c"]):
            obj = str(BLD / (pathlib.Path(fs_).stem + ".o"))
            run([CC] + CF + ["-I", str(LIB), "-I", str(APPS), "-c",
                 str(APPS / fs_), "-o", obj])
            objs.append(obj)
        if es_cpp:
            objs.append(str(BLD / "f32cxx.o"))
        if con_ulib or con_ugfx or es_cpp:
            objs.append(str(BLD / "ulib.o"))
            objs.append(str(BLD / "utext.o"))
            objs.append(str(BLD / "ulog.o"))
        if con_ugfx:
            objs.append(str(BLD / "ugfx.o"))
        run([CC, "-m32", "-nostdlib", "-Wl,--image-base,0x10000",
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
        # v2.2-editor: el editor avanzado (multidoc, undo, buscar, iconos)
        # supera los 64K clasicos; el nucleo admite plano+bss<=768K
        # (ver kern_crea_proceso). F7-C: fbrowser roza los 140K
        # (ulib HTTP+DNS+HTML+UGFX); tope 192K plano.
        assert len(plano) + 20 <= 196607, nom + " no cabe en 192K"
        assert bss_tam <= 0x40000, nom + " bss enorme"
        assert len(plano) + bss_tam <= 0xC0000, nom + " +bss > 768K"
    cab = b"F32X" + _st.pack("<IIII", ver, 0, len(plano) + 20, bss_tam)
    (BLD / nom).write_bytes(cab + plano)
    print(f"OK programa {nom} v{ver} ({len(plano) + 20} bytes, bss {bss_tam})")
    if al_disco:
        # v2.2-editor: los binarios >64K (editor avanzado) no caben como
        # archivo del FS (tam uint16). Siguen disponibles en el catalogo
        # de la ISO; aqui se omiten con aviso en vez de abortar todo.
        if len(cab + plano) > 65535:
            print(f"AVISO {nom}: {len(cab + plano)} B, no cabe en datos.img "
                  f"(sigue en la ISO)")
        else:
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
    ("quema32", ["quema32.c"], True),
    ("equidad32", ["equidad32.c"], True),
    ("sucio32", ["sucio32.c"], True),
    ("ipc_hijo", ["ipc_hijo.c"], True),
    ("ipc32", ["ipc32.c"], True),
    ("estres_mem", ["estres_mem.c"], True),
    ("estres_ipc", ["estres_ipc.c"], True),
    ("estres_mix", ["estres_mix.c"], True),
    ("disco32", ["disco32.c"], True),
    ("servidor", ["servidor.c"], True),
    ("wdemo", ["wdemo.c"], True),
    ("ugfx32", ["ugfx32.c"], True, True),
    ("terminal", ["terminal.c"], True, True),
    ("explorador", ["explorador.c"], True, True),
    ("sysinfo", ["sysinfo.c"], True, True),
    ("editor", ["editor.c"], True, True),
    ("f32", ["f32.c"], True),
    ("configura", ["configura.c"], True, True),
    ("gbye", ["gbye.c"], True),
    ("planificador32", ["planificador32.c"], True),
    ("contador32", ["contador32.c"], True),
    ("dormir32", ["dormir32.c"], True),
    ("aislamiento32", ["aislamiento32.c"], True),
    ("aisla32", ["aisla32.c"], True),
    ("t1_lib", ["t1_lib.c"], True),
    ("t_red", ["t_red.c"], True),
    ("dhcp", ["dhcp.c"], True),
    ("dns", ["dns.c"], True),
    ("http", ["http.c"], True),
    ("tutf8", ["tutf8.c"], True),
    ("t2_gfx", ["t2_gfx.c"], True),
    ("editor16", ["editor16.c"], True),
    ("demo16", ["d_main.c", "d_graficos.c", "d_ficheros.c", "d_memoria.c",
                "d_banco.c"], True),
    ("menu", ["menu.c"], True),
    ("calculadora", ["calculadora.c"], True),
    ("reloj", ["reloj.c"], True),
    ("adivina", ["adivina.c"], True),
    ("fbrowser", ["fbrowser.c"], True, True),
    ("t_fb", ["t_fb.c"], True),
    # F32 C++ (v2.4.2): demo + bateria (compilan con g++, enlazan
    # startxx + f32cxx + ulib; mismo F32X v3 y limites que C).
    ("cppdemo", ["cppdemo.cpp"], True),
    ("t_cpp", ["t_cpp.cpp"], True),
]
for _nom, _fs, _u, *_g in _apps3:
    haz_programa(_nom, False, _fs, 3, _u, al_disco=WANT_DISK,
                 con_ugfx=bool(_g and _g[0]))

# ---- escribir la ISO con el catalogo (todo dentro del CD) ----
_todos = _programas + [a[0] for a in _apps3]
_fin_img = cdcat_build(_todos)
# F8: cola alta [993, fin) en 0x1000000 (8 MB: pag.c mapea
# 0x1000000-0x1800000). El tamano lo dicta el contenido.
_cola = (_fin_img - 993) * 512
assert _cola <= 0x800000 - 8192, "cola del catalogo > 8M-8K"
pay = _fin_img - CAT_LBA
assert pay <= 0xFFFF, "payload >32M: pasar params a u32 (ver F8)"
# El arranque copia desde CAT_LBA hasta el final de los datos: eso son
# (_fin_img - CAT_LBA) sectores de 512. Se parchea en el payload
# (logico 0) y en el sector de arranque del CD (BOOT_LBA).
struct.pack_into("<H", img, 496, pay)
struct.pack_into("<H", _bs, 496, pay)
_bs2 = bytearray(img[0:512])
img[0:512] = bytes(_bs2)
# Arranque (1024 B) en BOOT_LBA y payload empaquetado de 4 en 4
# (4 logicos de 512 por sector CD de 2048) desde PAYLOAD_LBA.
_ab = bytes(_bs)
assert len(_ab) == 1024
_bootsec = bytearray(2048)
_bootsec[0:1024] = _ab
wsec(BOOT_LBA, _bootsec)
_ncd = (len(img) + 2047) // 2048
for _k in range(_ncd):
    _trozo = img[_k * 2048:(_k + 1) * 2048]
    _sec = bytearray(2048)
    _sec[0:len(_trozo)] = _trozo
    wsec(PAYLOAD_LBA + _k, _sec)
_fin = PAYLOAD_LBA + _ncd
# el PVD declara el tamano real (si no, el BIOS lee mas alla del EOF)
struct.pack_into("<I", pvd, 80, _fin)
struct.pack_into(">I", pvd, 84, _fin)
wsec(16, pvd)
out_iso = ISO_DIR / "FacilOS-v2.4.2.iso"   # v2.4.2 (rama propia; v2.4 intacta)
_iso = bytearray(_fin * 2048)
for _k in range(_fin):
    _iso[_k * 2048:(_k + 1) * 2048] = iso_sec.get(_k, bytes(2048))
_iso = bytes(_iso)
out_iso.write_bytes(_iso)
print(f"OK {out_iso} {len(_iso)} bytes ({_fin} sectores, "
      f"{len(_todos)} programas dentro; util {kb + _fin_img * 512} B, "
      f"cat LBA {CAT_LBA}, datos LBA {DATOS_LBA})")
if WANT_DISK:
    print(f"OK {_datos_img} {_datos_img.stat().st_size} bytes (disco opcional)")
