#!/usr/bin/env python3
"""Par TCP del invitado para F5 (vive en el ANFITRION).

Uso:  python tools/tcp_par.py [puerto_eco]
  5001: eco (hilos; varias conexiones a la vez).
  5002: medio-cierre (acepta, espera 1 s, shutdown(SHUT_WR), cierra a los 6 s).
  5003: silencio (acepta, lee una vez y calla 25 s: provoca RTO en el invitado).
  Escucha del invitado: el invitado escucha en 6101; este script NO la
  inicia: el test F de t_red la levanta y aqui se conecta con reintentos
  (ver funcion prueba_escucha, invocada con argumento 'escucha').

Uso:  python tools/tcp_par.py escucha
  Conecta a 127.0.0.1:6101 (reintentos 60 s), envia 'hola invitado',
  espera 'hola anfitrion', cierra. Imprime PASS/FAIL.
"""
import socket
import sys
import threading
import time

ECO = 5001
FIN = 5002
SILENCIO = 5003
ESCUCHA = 6101
LISTO = 6102


def eco_uno(cn, n):
    try:
        while True:
            d = cn.recv(536)
            if not d:
                break
            cn.sendall(d)
    except OSError:
        pass
    finally:
        try:
            cn.close()
        except OSError:
            pass


def sirve_eco():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", ECO))
    s.listen(8)
    print("par: eco en %d" % ECO, flush=True)
    while True:
        cn, _ = s.accept()
        threading.Thread(target=eco_uno, args=(cn, 0), daemon=True).start()


def sirve_fin():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", FIN))
    s.listen(4)
    print("par: medio-cierre en %d" % FIN, flush=True)
    while True:
        cn, _ = s.accept()
        try:
            time.sleep(1)
            cn.shutdown(socket.SHUT_WR)
            time.sleep(5)
        except OSError:
            pass
        finally:
            try:
                cn.close()
            except OSError:
                pass


def sirve_silencio():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", SILENCIO))
    s.listen(4)
    print("par: silencio en %d" % SILENCIO, flush=True)
    while True:
        cn, _ = s.accept()
        try:
            cn.settimeout(30)
            cn.recv(536)
            time.sleep(25)
        except OSError:
            pass
        finally:
            try:
                cn.close()
            except OSError:
                pass


def prueba_escucha():
    intento = 0
    while True:
        intento += 1
        try:
            c = socket.create_connection(("127.0.0.1", ESCUCHA), timeout=5)
        except OSError:
            time.sleep(1)
            continue
        try:
            c.sendall("hola invitado".encode())
            d = b""
            c.settimeout(10)
            while len(d) < 14:
                t = c.recv(14 - len(d))
                if not t:
                    break
                d += t
            if d == "hola anfitrion".encode():
                print("par escucha: PASS (intento %d)" % intento)
                c.close()
                sys.exit(0)
            print("par escucha: reintento (recibido %r)" % (d,))
        except OSError as e:
            print("par escucha: reintento (%s)" % (e,))
        finally:
            try:
                c.close()
            except Exception:
                pass
        time.sleep(1)
    print("par escucha: FAIL (sin conexion util)")
    sys.exit(1)


def sirve_listo():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", LISTO))
    s.listen(4)
    print("par: listo-en %d (dispara la prueba pasiva)" % LISTO, flush=True)
    while True:
        try:
            listo, _ = s.accept()
        except OSError:
            continue
        try:
            # El invitado ya escucha: un intento basta (reintentos
            # acotados por si llego una rafaga temprana).
            fin = time.time() + 60
            ok = False
            while time.time() < fin and not ok:
                try:
                    c = socket.create_connection(("127.0.0.1", ESCUCHA),
                                                 timeout=5)
                except OSError:
                    time.sleep(1)
                    continue
                try:
                    c.sendall("hola invitado".encode())
                    d = b""
                    c.settimeout(10)
                    while len(d) < 14:
                        t = c.recv(14 - len(d))
                        if not t:
                            break
                        d += t
                    if d == "hola anfitrion".encode():
                        print("par escucha: PASS", flush=True)
                        ok = True
                    else:
                        print("par escucha: reintento (%r)" % (d,),
                              flush=True)
                except OSError as e:
                    print("par escucha: reintento (%s)" % (e,), flush=True)
                finally:
                    try:
                        c.close()
                    except Exception:
                        pass
                if not ok:
                    time.sleep(1)
            if not ok:
                print("par escucha: FAIL (sin conexion util)", flush=True)
        finally:
            try:
                listo.close()
            except Exception:
                pass


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "escucha":
        demora = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        if demora > 0:
            time.sleep(demora)
        prueba_escucha()
    else:
        for f in (sirve_eco, sirve_fin, sirve_silencio, sirve_listo):
            threading.Thread(target=f, daemon=True).start()
        print("par: listo (eco=%d fin=%d silencio=%d)" % (ECO, FIN, SILENCIO),
              flush=True)
        while True:
            time.sleep(3600)
