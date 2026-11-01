import subprocess, threading, time, socket

GDB = r"C:\Users\artur\gcc32\mingw32\bin\gdb.exe"
with open("build/gdbcmd3.txt", "w") as f:
    f.write("target remote 127.0.0.1:4674\n")
    f.write("set architecture i386\n")
    f.write("file build/kernel32.pe\n")
    f.write("break *0x200005\n")
    f.write("continue\n")
    for _ in range(12):
        f.write("stepi\n")
    f.write("info registers eax ebx ecx edx esi edi ebp esp eip\n")
    f.write("quit\n")

def gdb_hilo():
    r = subprocess.run(
        [GDB, "-batch", "-x", "build/gdbcmd3.txt"],
        capture_output=True, text=True, timeout=300)
    open("build/gdbout3.txt", "w").write(r.stdout + "\nSTDERR:\n" + r.stderr)

t = threading.Thread(target=gdb_hilo)
t.start()
time.sleep(20)
s = socket.create_connection(("127.0.0.1", 4675), timeout=10)
f = s.makefile("rwb", buffering=0)
time.sleep(0.3)
try:
    s.recv(5000)
except Exception:
    pass

def cmd(txt):
    mp = {"\n": "ret", " ": "spc"}
    for ch in txt:
        f.write(("sendkey %s\n" % mp.get(ch, ch)).encode())
        f.flush()
        time.sleep(0.15)
    time.sleep(1.5)

cmd("ejecutar hola\n")
t.join(timeout=120)
print("gdb terminado")
try:
    f.write(b"quit\n")
    f.flush()
except Exception:
    pass
