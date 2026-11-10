#!/usr/bin/env python3
"""Bateria FacilOS en UN solo arranque (ahorra minutos y tokens).

Uso: bateria.py ISO [- SERLOG PUERTO] [t1 [t2 ...]]
  Arranca un QEMU, pasa dhcp una vez y ejecuta cada test con
  {WAIT:retorno:} event-driven (sin sleeps fijos por test).
  Imprime UNA linea por test (veredicto) y sale con nº de fallos.

Con --stub levanta http_stub.py para los viva de t_fb.
"""
import os, re, subprocess, sys, time, pathlib

BASE = pathlib.Path(__file__).parent.parent
ISO = str(BASE / "build" / "iso" / "FacilOS-v2.4.3.iso")
SER = "C:/Temp/ser_bateria.log"
PORT = "5130"
TESTS = []
WITH_STUB = False
for a in sys.argv[1:]:
    if a == "--stub":
        WITH_STUB = True
    elif a.endswith(".iso"):
        ISO = a
    elif a.startswith("--ser="):
        SER = a[6:]
    elif a.startswith("--port="):
        PORT = a[7:]
    else:
        TESTS.append(a)
if not TESTS:
    TESTS = ["t1_lib", "tutf8", "mem32", "t_cpp", "t_fb"]

TIMEOUT = {"t_fb": 120.0, "t_red": 120.0}
DFLT_TO = 40.0

env = dict(os.environ)
env["QRUN_RED"] = "1"
env["QRUN_USB"] = "1"

stub = None
if WITH_STUB:
    stub = subprocess.Popen(
        [sys.executable, str(BASE / "tools" / "http_stub.py"), "18080"])
    time.sleep(2)

cmds = ["{ESC}", "{WAIT:facil32:/>,30}", "dhcp", "{WAIT:retorno:,30}"]
for t in TESTS:
    cmds += ["ejecutar " + t,
             "{WAIT:retorno:,%d}" % int(TIMEOUT.get(t, DFLT_TO))]

try:
    r = subprocess.run(
        [sys.executable, str(BASE / "tools" / "qrun.py"), ISO, "-", SER,
         PORT] + cmds,
        capture_output=True, text=True, timeout=900, env=env,
        cwd=str(BASE.parent))
    if r.returncode != 0:
        print("QEMU fallo:\n" + (r.stderr or "")[-500:])
        sys.exit(2)
finally:
    if stub:
        stub.terminate()

txt = pathlib.Path(SER).read_bytes().decode(errors="ignore")
# trocea por cada "ejecutar TEST"
partes = re.split(r"facil32:/> ejecutar (\S+)\r?\n", txt)
# partes[0] = boot, luego (test, salida, test, salida...)
fallos_tot = 0
visto = 0
for i in range(1, len(partes) - 1, 2):
    nom, sal = partes[i], partes[i + 1]
    m = re.findall(r"(\d+) pruebas (\d+) fallos", sal)
    mr = re.findall(r"retorno: (-?\d+)", sal)
    if m:
        pr, fa = int(m[-1][0]), int(m[-1][1])
        print("%-10s %3d pruebas %d fallos" % (nom, pr, fa), flush=True)
        fallos_tot += fa
    elif mr:
        rc = int(mr[-1])
        print("%-10s retorno %d" % (nom, rc), flush=True)
        fallos_tot += (rc != 0)
    else:
        print("%-10s SIN VEREDICTO" % nom, flush=True)
        fallos_tot += 1
    visto += 1
if not visto:
    print("sin tests localizados; cola:")
    print(txt[-400:])
    sys.exit(2)
print("TOTAL fallos: %d" % fallos_tot, flush=True)
sys.exit(1 if fallos_tot else 0)
