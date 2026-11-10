#!/usr/bin/env python3
"""Arranca FacilOS en QEMU, envia comandos por monitor y vuelca el serial.
Uso: qrun.py ISO HDD SERLOG PUERTO cmd1 [cmd2 ...]
  HDD = "-" para arrancar solo con la ISO (v1.8 no necesita disco).
  {UP} {DOWN} {LEFT} {RIGHT} = flechas ; {RET} = enter ; resto = texto + enter.
  Sufijo ~ = sin el Enter final. {SHOT:ruta} = volcado de pantalla.
  {SLEEP:seg} = espera (las apps tardan ~1-2 s en redibujar 800x600).
  {K:0x1b} = tecla cruda de QEMU (codigo en hexadecimal). Ojo: en la
  tabla ES de FacilOS '+' es el scancode 0x1B y '*' es shift-0x0D, que en
  el harness es el mismo shift-equal que produce '+' en un teclado US.
  Ojo tambien con PowerShell: los argumentos con espacios hay que pasarlos
  por tools/qwin.ps1 (Start-Process los parte si se llaman tal cual).
"""
import os, socket, subprocess, sys, time, pathlib
ISO = str(pathlib.Path(sys.argv[1]).resolve())
HDD = sys.argv[2] if sys.argv[2] == "-" else str(pathlib.Path(sys.argv[2]).resolve())
SER, PORT = sys.argv[3], int(sys.argv[4])
CMDS = sys.argv[5:]
ser = pathlib.Path(SER)
if ser.exists():
    ser.unlink()
args = [r"C:\Program Files\qemu\qemu-system-i386.exe", "-cdrom", ISO]
if HDD != "-":
    # format=raw explicito: si QEMU sondea el formato, bloquea las
    # escrituras al sector 0 (justo la cabecera FACILOS2) y deja la imagen
    # con suma que no cuadra al siguiente arranque.
    args += ["-drive", f"file={HDD},format=raw,if=ide,index=0,media=disk"]
args += ["-boot", "order=d", "-m", "32", "-display", "none",
         "-serial", f"file:{ser}", "-monitor", f"tcp:127.0.0.1:{PORT},server,nowait"]
if os.environ.get("QRUN_RED") == "1":
    # v2.3-red F1: red virtual opcional (user-net + RTL8139). Por
    # defecto NO hay red: el arranque sin adaptador debe seguir
    # funcionando. MAC fija para pruebas reproducibles.
    # v2.3-red F5: QRUN_FWD="HP:GP[,HP2:GP2]" reenvia puertos del
    # anfitrion al invitado (pruebas de escucha pasiva).
    red = "user,id=red0"
    if os.environ.get("QRUN_FWD"):
        for rr in os.environ["QRUN_FWD"].split(","):
            hp, gp = rr.split(":")
            red += ",hostfwd=tcp::%s-:%s" % (hp.strip(), gp.strip())
    args += ["-netdev", red,
             "-device", "rtl8139,netdev=red0,mac=52:54:00:12:34:56"]
    if os.environ.get("QRUN_PCAP"):
        args += ["-object",
                 "filter-dump,id=pcap,netdev=red0,file=%s" %
                 os.environ["QRUN_PCAP"]]
if os.environ.get("QRUN_USB") == "1":
    # v2.4-USB: OHCI + teclado y raton USB (qemu los expone; el
    # invitado los enumera por sondeo sin IRQ nueva).
    args += ["-device", "pci-ohci",
             "-device", "usb-kbd",
             "-device", "usb-mouse"]
if os.environ.get("QRUN_DEBUG"):
    # Traza de interrupciones y reinicios de QEMU (depuracion del kernel).
    args += ["-d", "int,cpu_reset,guest_errors", "-D",
             str(ser.with_suffix(".qemu"))]
q = subprocess.Popen(args)
time.sleep(2)
s = socket.create_connection(("127.0.0.1", PORT), timeout=10)
f = s.makefile("rwb", buffering=0)
time.sleep(0.3)
try:
    s.recv(5000)
except Exception:
    pass
for _ in range(40):
    time.sleep(0.5)
    if ser.exists() and "facil:" in ser.read_bytes().decode(errors="ignore"):
        break
MP = {"\n": "ret", " ": "0x39", "_": "shift-minus", "-": "minus",
      ".": "dot", "/": "slash", ":": "shift-dot", '"': "shift-2",
      # tabla CP437 del BIOS: 0x0B '0'/shift'=', 0x1B '+'/shift'*',
      # 0x0D '='/shift'+', 0x0B..0x0C fila de numeros.
      "=": "shift-0", "*": "shift-equal", "+": "shift-equal",
      ")": "shift-0", "(": "shift-9", "!": "shift-1", "?": "shift-slash"}
MP.update({c: f"shift-{c.lower()}" for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZÑ"})
_ultima = [""]
def key(k):
    # Hold explicito de 20 ms: sin el, QEMU mantiene la tecla 100 ms y al
    # enviar la siguiente (gap de 90 ms) las pulsaciones se solapan y el
    # invitado ve scancodes raros (p. ej. el espacio como Enter).
    # v2.2: en hosts cargados QEMU fusiona pulsaciones rapidas (el 8042
    # virtual pierde bytes); gaps amplios para inyeccion fiable.
    f.write(f"sendkey {k}\n".encode()); f.flush()
    if k == _ultima[0]:
        time.sleep(0.6)
    else:
        time.sleep(0.35)
    _ultima[0] = k
def send(t):
    i = 0
    while i < len(t):
        if t[i] == "{":
            j = t.index("}", i)
            n = t[i+1:j]
            if n.startswith("K:"):
                key(n[2:])          # nombre de tecla crudo de QEMU
            else:
                key({"UP": "up", "DOWN": "down", "LEFT": "left",
                     "RIGHT": "right", "RET": "ret", "ESC": "esc"}[n])
            i = j + 1
        else:
            c = t[i]
            key(MP.get(c, c))
            i += 1
    time.sleep(1.0)
for c in CMDS:
    if c.startswith("{SHOT:"):
        f.write(f"screendump {c[6:-1]}\n".encode()); f.flush(); time.sleep(1.5)
        continue
    if c.startswith("{MOU:"):
        # {MOU:x:y} = mover puntero (requiere raton PS/2 en el invitado)
        xy = c[5:-1]
        f.write(f"mouse_move {xy.replace(':', ' ')}\n".encode()); f.flush(); time.sleep(0.5)
        continue
    if c.startswith("{BTN:"):
        f.write(f"mouse_button {c[5:-1]}\n".encode()); f.flush(); time.sleep(0.5)
        continue
    if c.startswith("{SLEEP:"):
        time.sleep(float(c[7:-1])); continue
    if c.startswith("{WAIT:"):
        # {WAIT:texto,seg} = espera event-driven (ahorra sleeps fijos).
        interior = c[6:-1]
        txt, _, to = interior.partition(",")
        to = float(to) if to else 60.0
        t0 = time.time()
        while time.time() - t0 < to:
            try:
                if txt in ser.read_bytes().decode(errors="ignore"):
                    break
            except Exception:
                pass
            time.sleep(0.5)
        continue
    crudo = c.endswith("~")
    if crudo:
        c = c[:-1]
    # v2.0-etapa5: tecla especial aislada ({DOWN}, {K:f1}...): se envia
    # SIN Enter final. Antes caia al send() con "\n" y cada tecla metia
    # un Enter fantasma (explica falsos lanzamientos del menu y ecos
    # vacios en la terminal). Las cadenas mixtas conservan su Enter.
    if c.startswith("{") and c.endswith("}") and c.count("{") == 1:
        send(c)
        continue
    send(c + ("" if crudo else "\n"))
time.sleep(1)
txt = ser.read_bytes().decode(errors="ignore")
print(txt[-3000:].encode("ascii", "replace").decode("ascii"))
f.write(b"quit\n"); f.flush(); time.sleep(0.5)
q.terminate()
