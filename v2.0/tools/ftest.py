#!/usr/bin/env python3
"""F32 Test Orchestrator: niveles FAST / INTEGRATION / FULL.

- FAST: compila los ficheros cambiados (sin QEMU), chequeos estaticos.
- INTEGRATION: reconstruye la ISO solo si los inputs cambiaron
  (manifiesto sha1) y ejecuta SOLO los tests afectados en UN arranque.
- FULL: bateria completa (t1_lib, tutf8, mem32, t_cpp, t_fb+stub)
  en un arranque. t_red se excluye siempre (cuelga en este entorno:
  slirp sin DNS/ruta, ver estado v2.3; no es regresion).

Uso:
  python3 v2.0/tools/ftest.py [--level FAST|INTEGRATION|FULL]
                               [--tests t1,t2] [--iso RUTA] [--stub]
                               [--report RUTA] [--compare-baseline TEST]
Seguridad: no toca main (rama actual), no resetea nada, no borra ISOs.
"""
import hashlib
import fnmatch
import json
import os
import pathlib
import re
import subprocess
import sys
import time

BASE = pathlib.Path(__file__).parent.parent   # v2.0 (igual que build32.py)
ROOT = BASE.parent                             # raiz del repo
TOOLS = BASE / "tools"
STATE = BASE / "build" / ".ftest_state.json"
LOGS = BASE / "build" / "logs"
FULL_SUITE = ["t1_lib", "tutf8", "mem32", "t_cpp", "t_fb", "t_tls"]
TIMEOUT = {"t_fb": 150.0, "t_red": 150.0}
DFLT_TO = 60.0
# fichero -> patron de tests afectados ("*" = todos). Lo no listado
# con impacto en binario exige FULL (conservador).
IMPACTO = [
    ("v2.0/src/kernel32/*", "*"),
    ("v2.0/user/lib/*", "*"),
    ("v2.0/user/ld/*", "*"),
    ("v2.0/tools/build32.py", "*"),
    ("v2.0/tools/qrun.py", "*"),
    ("v2.0/tools/bateria.py", "*"),
    ("v2.0/tools/ftest.py", "*"),
    ("v2.0/tools/http_stub.py", "t_fb"),
    ("v2.0/user/apps/t1_lib.c", "t1_lib"),
    ("v2.0/user/apps/tutf8.c", "tutf8"),
    ("v2.0/user/apps/mem32.c", "mem32"),
    ("v2.0/user/apps/t_cpp.cpp", "t_cpp"),
    ("v2.0/user/apps/cppdemo.cpp", "t_cpp"),
    ("v2.0/user/apps/t_fb.c", "t_fb"),
    ("v2.0/user/apps/fbrowser.c", "t_fb"),
    ("v2.0/user/apps/t_tls.c", "t_tls"),
    ("v2.0/user/apps/https.c", "*"),
]
IGNORAR = ["v2.0/thirdparty/", "v2.0/build/", "C:/Temp/", "/tmp/"]
ISO_INPUTS = (".c", ".h", ".cpp", ".asm", ".ld", ".py")


def rel(p):
    try:
        return pathlib.Path(p).resolve().relative_to(
            ROOT.resolve()).as_posix()
    except ValueError:
        return str(p)


def ignorado(r):
    return r.startswith("v2.0/thirdparty/") or r.startswith("v2.0/build/")


def cambios_git():
    """Ficheros modificados/nuevos segun git (solo informan motivos)."""
    r = subprocess.run(["git", "status", "--short"], capture_output=True,
                       text=True, cwd=str(ROOT))
    out = []
    for ln in r.stdout.splitlines():
        if len(ln) > 3:
            p = rel(ln[3:].strip().strip('"'))
            if not ignorado(p):
                out.append(p)
    return sorted(set(out))


def manifiesto_iso():
    """sha1 estable de todos los inputs que condicionan la ISO."""
    h = hashlib.sha1()
    files = []
    for base in [BASE / "src", BASE / "user", BASE / "tools"]:
        for p in sorted(base.rglob("*")):
            if not p.is_file():
                continue
            r = p.relative_to(ROOT).as_posix()
            if ignorado(r):
                continue
            if r.startswith("v2.0/tools/") and p.name != "build32.py":
                continue
            if p.suffix not in ISO_INPUTS:
                continue
            files.append((r, p))
    for r, p in files:
        h.update(r.encode())
        h.update(b"\0")
        h.update(p.read_bytes())
        h.update(b"\0")
    for exe in ["gcc.exe", "g++.exe"]:
        try:
            v = subprocess.run(
                [r"C:\Users\artur\gcc32\mingw32\bin\%s" % exe,
                 "--version"], capture_output=True, text=True,
                timeout=30)
            h.update(v.stdout.splitlines()[0].encode())
        except Exception:
            h.update(b"?")
    return h.hexdigest(), [r for r, _ in files]


def seleccionar(nivel, tests_pedidos, cambiados):
    """Devuelve (tests, motivos{test: razon}, omitidos[(test, razon)])."""
    if nivel == "FULL":
        sel = list(FULL_SUITE)
        mot = {t: "suite FULL" for t in sel}
        return sel, mot, []
    if tests_pedidos:
        conocidos = FULL_SUITE + ["t_red"]
        sel = [t for t in tests_pedidos if t in conocidos]
        mot = {t: "pedido explicito" + (" (puede colgar en este entorno)"
                                       if t == "t_red" else "")
               for t in sel}
        om = [(t, "no pedido") for t in FULL_SUITE if t not in sel]
        return sel, mot, om
    # INTEGRATION: mapea cambios -> tests (transitivo y conservador).
    sel, mot = [], {}
    for f in cambiados:
        for pat, tgt in IMPACTO:
            if fnmatch.fnmatch(f, pat):
                if tgt == "*":
                    for t in FULL_SUITE:
                        if t not in sel:
                            sel.append(t)
                            mot[t] = "cambio en %s (nucleo/libreria/harness)" % f
                elif tgt not in sel:
                    sel.append(tgt)
                    mot[tgt] = "cambio en %s" % f
                break
        else:
            # fichero con impacto desconocido en binario -> FULL.
            if f.endswith((".c", ".cpp", ".h", ".asm", ".ld")):
                for t in FULL_SUITE:
                    if t not in sel:
                        sel.append(t)
                        mot[t] = "cambio en %s (impacto desconocido: FULL)" % f
    sel = [t for t in FULL_SUITE if t in sel]
    om = [(t, "sin cambios relacionados") for t in FULL_SUITE
          if t not in sel]
    return sel, mot, om


def compila_fast(ficheros):
    """Compila (solo sintaxis+objeto a temp) los ficheros cambiados."""
    import tempfile
    GCC = r"C:\Users\artur\gcc32\mingw32\bin\gcc.exe"
    GXX = r"C:\Users\artur\gcc32\mingw32\bin\g++.exe"
    NASM = str(TOOLS / "nasm" / "nasm-3.01" / "nasm.exe")
    LIB = BASE / "v2.0" / "user" / "lib"
    APPS = BASE / "v2.0" / "user" / "apps"
    C32 = ["-ffreestanding", "-nostdlib", "-m32", "-march=i386", "-O1",
           "-Wall", "-fno-pic", "-fno-pie", "-fno-stack-protector",
           "-fzero-initialized-in-bss"]
    PROG = C32 + ["-Os", "-ffunction-sections", "-fdata-sections"]
    CXX = PROG + ["-fno-exceptions", "-fno-rtti",
                  "-fno-asynchronous-unwind-tables", "-fuse-cxa-atexit"]
    res = []
    tmp = tempfile.mkdtemp(prefix="ftest_fast_")
    for f in ficheros:
        p = ROOT / f
        if not p.exists():
            res.append((f, "borrado: sin chequeo"))
            continue
        if f.endswith(".py"):
            r = subprocess.run([sys.executable, "-m", "py_compile",
                                str(p)], capture_output=True, text=True)
            res.append((f, "ok" if r.returncode == 0
                        else "FALLO:\n" + r.stderr[-800:]))
        elif f.endswith(".asm"):
            fmt = "bin" if "boot32" in f else "win32"
            r = subprocess.run([NASM, "-f", fmt, str(p), "-o",
                                os.path.join(tmp, "x.o")],
                               capture_output=True, text=True)
            res.append((f, "ok" if r.returncode == 0
                        else "FALLO:\n" + (r.stdout + r.stderr)[-800:]))
        elif f.endswith(".cpp"):
            r = subprocess.run([GXX] + CXX + ["-I", str(LIB), "-I",
                                str(APPS), "-c", str(p), "-o",
                                os.path.join(tmp, "x.o")],
                               capture_output=True, text=True)
            res.append((f, "ok" if r.returncode == 0
                        else "FALLO:\n" + r.stderr[-800:]))
        elif f.endswith(".c"):
            flags = C32 if "/kernel32/" in f.replace("\\", "/") else PROG
            r = subprocess.run([GCC] + flags + ["-I", str(LIB), "-I",
                                str(APPS), "-c", str(p), "-o",
                                os.path.join(tmp, "x.o")],
                               capture_output=True, text=True)
            res.append((f, "ok" if r.returncode == 0
                        else "FALLO:\n" + r.stderr[-800:]))
        elif f.endswith(".ld"):
            res.append((f, "sin chequeo estatico (requiere INTEGRATION)"))
        else:
            res.append((f, "sin chequeo (no compilable)"))
    return res


def arranca_qemu(iso, ser, port, cmds, timeout_total, stub=False):
    """Un arranque, N comandos. Devuelve texto del serial."""
    env = dict(os.environ)
    env["QRUN_RED"] = "1"
    env["QRUN_USB"] = "1"
    st = None
    if stub:
        st = subprocess.Popen(
            [sys.executable, str(TOOLS / "http_stub.py"), "18080"])
        time.sleep(2)
    try:
        # mata restos de sesiones anteriores (harness conocido)
        subprocess.run(["taskkill", "/F", "/IM", "qemu-system-i386.exe"],
                       capture_output=True)
        r = subprocess.run(
            [sys.executable, str(TOOLS / "qrun.py"), iso, "-", ser,
             str(port)] + cmds, capture_output=True, text=True,
            timeout=timeout_total, env=env, cwd=str(BASE))
        return r.stdout or "", None
    except subprocess.TimeoutExpired as e:
        subprocess.run(["taskkill", "/F", "/IM", "qemu-system-i386.exe"],
                       capture_output=True)
        out = ""
        try:
            out = pathlib.Path(ser).read_bytes().decode(
                errors="ignore")
        except Exception:
            pass
        return out, "timeout QEMU (colgado?)"
    finally:
        if st:
            st.terminate()


def veredictos(ser):
    txt = pathlib.Path(ser).read_bytes().decode(errors="ignore")
    partes = re.split(r"facil32:/> ejecutar (\S+)\r?\n", txt)
    res = []
    for i in range(1, len(partes) - 1, 2):
        nom, sal = partes[i], partes[i + 1]
        m = re.findall(r"(\d+) pruebas (\d+) fallos", sal)
        mr = re.findall(r"retorno: (-?\d+)", sal)
        if m:
            res.append((nom, "pruebas", int(m[-1][0]), int(m[-1][1])))
        elif mr:
            res.append((nom, "retorno", int(mr[-1]), None))
        else:
            res.append((nom, "sin-veredicto", None, None))
    return res


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--level", default="INTEGRATION",
                    choices=["FAST", "INTEGRATION", "FULL"])
    ap.add_argument("--tests", default="")
    ap.add_argument("--iso", default=str(BASE / "build" / "iso" /
                                         "FacilOS-v2.4.3.iso"))
    ap.add_argument("--stub", action="store_true")
    ap.add_argument("--report", default="")
    a = ap.parse_args()
    t00 = time.time()
    ped = [t for t in a.tests.split(",") if t] if a.tests else []
    cambiados = cambios_git()
    iso_hash, iso_files = manifiesto_iso()
    try:
        estado = json.loads(STATE.read_text())
    except Exception:
        estado = {}
    lineas = []
    lineas.append("# F32 Test Report - %s" % a.level)
    lineas.append("- Rama: %s" %
                  subprocess.run(["git", "branch", "--show-current"],
                                 capture_output=True, text=True,
                                 cwd=str(ROOT)).stdout.strip())
    lineas.append("- Fecha (UTC): %s" %
                  time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime()))
    lineas.append("- Manifiesto ISO sha1: `%s` (%d ficheros)" %
                  (iso_hash[:16], len(iso_files)))
    lineas.append("- Cambios detectados (%d): %s" %
                  (len(cambiados),
                   ", ".join(cambiados) if cambiados else "ninguno"))

    if a.level == "FAST":
        t0 = time.time()
        comp = [f for f in cambiados
                if f.endswith((".c", ".cpp", ".h", ".asm",
                                ".ld", ".py"))]
        res = compila_fast(comp)
        fallos = 0
        for f, r in res:
            lineas.append("- FAST %s: %s" % (f, r.splitlines()[0]
                                             if "\n" not in r else "FALLO"))
            if r != "ok" and not r.startswith("sin ") and \
                    not r.startswith("borrado"):
                fallos += 1
                lineas.append("```\n%s\n```" % r[-1500:])
        lineas.append("- FAST duracion: %.1fs" % (time.time() - t0))
        lineas.append("- QEMU: no arrancado (FAST no lo requiere)")
        lineas.append("- TOTAL fallos: %d" % fallos)
        informe = "\n".join(lineas) + "\n"
        print(informe)
        if a.report:
            pathlib.Path(a.report).write_text(informe, encoding="utf-8")
        return 1 if fallos else 0

    tests, motivos, omitidos = seleccionar(a.level, ped, cambiados)
    for t in tests:
        lineas.append("- Seleccionado %s: %s" %
                      (t, motivos.get(t, "?")))
    for t, r in omitidos:
        lineas.append("- NO ejecutado %s: %s (no cuenta como pasado)" %
                      (t, r))
    lineas.append("- Excluido siempre t_red: cuelga en este entorno "
                  "(slirp sin DNS/ruta, ver estado v2.3; no regresion)")

    # --- ISO: reutilizar solo con prueba hash ---
    t0 = time.time()
    iso = a.iso
    reused = ""
    if estado.get("iso_manifest") == iso_hash and \
            pathlib.Path(iso).exists():
        reused = iso_hash[:16]
        lineas.append("- ISO reutilizada `%s` (manifiesto identico %s)" %
                      (pathlib.Path(iso).name, reused))
    else:
        r = subprocess.run([sys.executable, str(TOOLS / "build32.py")],
                           capture_output=True, text=True, cwd=str(BASE))
        lineas.append("- Build duracion: %.0fs" % (time.time() - t0))
        if r.returncode != 0:
            lineas.append("- BUILD FALLO:\n```\n%s\n```" %
                          ((r.stdout + r.stderr)[-2000:]))
            informe = "\n".join(lineas) + "\n"
            print(informe)
            if a.report:
                pathlib.Path(a.report).write_text(informe, encoding='utf-8')
            return 2
        STATE.write_text(json.dumps({"iso_manifest": iso_hash,
                                     "iso": iso}))
    if not reused:
        lineas.append("- ISO reconstruida (inputs distintos o sin estado)")
    t_qemu0 = time.time()
    ser = "C:/Temp/ser_ftest.log"
    port = "5135"
    cmds = ["{ESC}", "{WAIT:facil32:/>,30}", "dhcp",
            "{WAIT:retorno:,30}"]
    for t in tests:
        to = int(TIMEOUT.get(t, DFLT_TO))
        cmds += ["ejecutar " + t, "{WAIT:retorno:,%d}" % to]
    total_to = sum(int(TIMEOUT.get(t, DFLT_TO)) for t in tests) + 180
    qout, infra = arranca_qemu(iso, ser, port, cmds, total_to,
                               stub=(a.stub or "t_fb" in tests))
    lineas.append("- QEMU arranque+ejecucion: %.0fs" % (time.time() - t_qemu0))
    for wl in qout.splitlines():
        if wl.startswith("WAIT-"):
            lineas.append("- %s" % wl)
    if infra:
        lineas.append("- INFRAESTRUCTURA FALLO: %s (log: %s)" % (infra, ser))
        informe = "\n".join(lineas) + "\n"
        print(informe)
        if a.report:
            pathlib.Path(a.report).write_text(informe, encoding='utf-8')
        return 3
    fallos = 0
    for nom, kind, v1, v2 in veredictos(ser):
        if kind == "pruebas":
            lineas.append("- %s: %d pruebas %d fallos %s" %
                          (nom, v1, v2,
                           "OK" if v2 == 0 else "FALLO"))
            fallos += v2
        elif kind == "retorno":
            lineas.append("- %s: retorno %d %s" %
                          (nom, v1, "OK" if v1 == 0 else "FALLO"))
            fallos += (v1 != 0)
        else:
            lineas.append("- %s: SIN VEREDICTO (fallo)" % nom)
            fallos += 1
    for t, r in omitidos:
        lineas.append("- %s: no ejecutado (%s)" % (t, r))
    lineas.append("- TOTAL fallos: %d" % fallos)
    lineas.append("- Duracion total: %.0fs" % (time.time() - t00))
    informe = "\n".join(lineas) + "\n"
    print(informe)
    if a.report:
        pathlib.Path(a.report).write_text(informe, encoding='utf-8')
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
