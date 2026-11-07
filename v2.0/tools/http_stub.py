#!/usr/bin/env python3
"""Servidor HTTP/1.0 de pruebas para FacilOS32 F7-B (solo anfitrion).

Escucha en 127.0.0.1:18080 (el invitado QEMU llega via
10.0.2.2:18080 con la red user-mode por defecto, sin
hostfwd: la conexion la inicia el invitado).

Uso:  python tools/http_stub.py [puerto]

Rutas:
  /normal, /conlongitud : 200 + Content-Length + cuerpo fijo.
  /ocho-kb              : 200 + 8192 bytes deterministicos.
  /fragmentado          : 200 + cuerpo enviado byte a byte.
  /por-cierre           : 200 sin Content-Length, cierra al final.
  /chunked              : Transfer-Encoding: chunked (debe fallar).
  /mala-cabecera        : cabecera sin dos puntos (debe fallar).
  /cl-mal               : Content-Length no numerico (debe fallar).
  /cl-corto             : CL:100 con 10 bytes y cierre (truncada).
  /cl-exceso            : CL:10 con 20 bytes (excedente).
  /vacio                : 200 + Content-Length: 0 (vacio ok).
  /vacio204             : 204 sin cuerpo, mantiene conexion 25 s.
  /noencontrado         : 404 + cuerpo.
  /error                : 500 + cuerpo.
  /redir301, /redir302  : redireccion con Location (no seguir).
  /lento                : acepta y calla 30 s (timeout cliente).
  /corte                : cabeceras parciales y cierre (malformada).
  /corte-cuerpo         : cabeceras CL:100, 10 bytes, cierre.
  /eco/<resto>          : 200 con cuerpo "visto: <resto>".
"""
import socket
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 18080

CUERPO8K = bytes(((i * 131 + 7) & 0xFF) for i in range(8192))


def resp(code, motivo, headers, body):
    h = "HTTP/1.0 %d %s\r\n" % (code, motivo)
    for k, v in headers:
        h += "%s: %s\r\n" % (k, v)
    h += "Connection: close\r\n\r\n"
    return h.encode("latin1") + body


def atiende(cn):
    try:
        cn.settimeout(10)
        d = b""
        while b"\r\n\r\n" not in d and len(d) < 4096:
            t = cn.recv(4096 - len(d))
            if not t:
                print("stub: cliente cerro sin pedir (got %d B)" % len(d),
                      flush=True)
                return
            d = d + t
        try:
            txt = d.decode("latin1")
        except Exception:
            return
        lin = txt.split("\r\n", 1)[0]
        partes = lin.split(" ")
        path = partes[1] if len(partes) > 1 else "/"
        print("stub: %r -> %s" % (lin[:80], path), flush=True)
        if path == "/normal" or path == "/conlongitud":
            body = b"Hello, world!" if path == "/normal" else \
                b"0123456789ABCDEF"
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/ocho-kb":
            h = ("HTTP/1.0 200 OK\r\nContent-Type: "
                 "application/octet-stream\r\nContent-Length: 8192\r\n"
                 "Connection: close\r\n\r\n")
            cn.sendall(h.encode("latin1"))
            for i in range(0, len(CUERPO8K), 128):
                try:
                    cn.sendall(CUERPO8K[i:i + 128])
                except OSError as e:
                    print("stub: 8k envio fallo en %d (%r)" % (i, e),
                          flush=True)
                    return
                time.sleep(0.025)
            print("stub: 8k enviado completo", flush=True)
            time.sleep(2.0)
        elif path == "/fragmentado":
            h = ("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n"
                 "Content-Length: 14\r\nConnection: close\r\n\r\n")
            msg = h.encode("latin1") + b"fragmentado-ok"
            for i in range(len(msg)):
                try:
                    cn.sendall(msg[i:i + 1])
                except OSError:
                    return
                time.sleep(0.02)
        elif path == "/por-cierre":
            h = ("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n"
                 "Connection: close\r\n\r\n")
            cn.sendall(h.encode("latin1"))
            cn.sendall(b"hasta el cierre")
        elif path == "/chunked":
            h = ("HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n"
                 "Connection: close\r\n\r\n")
            cn.sendall(h.encode("latin1"))
            cn.sendall(b"5\r\nhola0\r\n0\r\n\r\n")
        elif path == "/mala-cabecera":
            cn.sendall(b"HTTP/1.0 200 OK\r\nX-Mala sin dos puntos\r\n"
                        b"\r\n")
        elif path == "/cl-mal":
            cn.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: abc\r\n"
                        b"\r\nxy")
        elif path == "/cl-corto":
            cn.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 100\r\n"
                        b"\r\n0123456789")
        elif path == "/cl-exceso":
            cn.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 10\r\n"
                        b"\r\n0123456789ABCDEFGHIJ")
        elif path == "/vacio":
            cn.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 0\r\n"
                        b"\r\n")
        elif path == "/vacio204":
            cn.sendall(b"HTTP/1.0 204 Sin Contenido\r\n"
                        b"Connection: close\r\n\r\n")
            time.sleep(25)
        elif path == "/noencontrado":
            cn.sendall(resp(404, "Not Found",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", "9")],
                             b"404 nada\n"))
        elif path == "/error":
            cn.sendall(resp(500, "Internal Server Error",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", "6")],
                             b"falla\n"))
        elif path == "/redir301":
            cn.sendall(resp(301, "Moved Permanently",
                             [("Location", "/normal"),
                              ("Content-Length", "5")],
                             b"muda\n"))
        elif path == "/redir302":
            cn.sendall(resp(302, "Found",
                             [("Location", "/normal"),
                              ("Content-Length", "5")],
                             b"muda\n"))
        elif path == "/lento":
            time.sleep(30)
        elif path == "/corte":
            cn.sendall(b"HTTP/1.0 200 OK\r\nContent-Ty")
        elif path == "/corte-cuerpo":
            cn.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 100\r\n"
                        b"\r\n0123456789")
        elif path.startswith("/eco/"):
            body = ("visto: " + path[5:]).encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", str(len(body)))],
                             body))
        else:
            cn.sendall(resp(404, "Not Found",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", "9")],
                             b"404 no?\n"))
    except OSError:
        pass
    finally:
        try:
            print("stub: fin %s (%d B pedidos)" % (path, len(d)),
                  flush=True)
        except Exception:
            pass
        try:
            cn.close()
        except OSError:
            pass


def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", PORT))
    s.listen(8)
    print("http-stub: escuchando en 127.0.0.1:%d" % PORT, flush=True)
    while True:
        try:
            cn, _ = s.accept()
        except OSError:
            continue
        threading.Thread(target=atiende, args=(cn,),
                         daemon=True).start()


if __name__ == "__main__":
    main()
