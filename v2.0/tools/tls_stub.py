"""Stub TLS para el hito 1 de https (FacilOS).
Servidor TLS 1.2 con el certificado de test (10.0.2.2); tras el
handshake sirve HTTP/1.0 simple (GET). Sin autenticacion de cliente.
Uso: python3 tls_stub.py [puerto]  (defecto 18443).
"""
import socket
import ssl
import sys
import threading

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 18443
BASE = "C:/Temp"


def cuerpo_pagina(path):
    if path == "/pagina" or path.startswith("/pagina?"):
        body = ("<html><head><title>Segura</title></head><body>"
                "<h1>TLS ok</h1><p>Pagina por HTTPS.</p></body></html>"
                ).encode("latin1")
        return (200, "OK", [("Content-Type", "text/html"),
                            ("Content-Length", str(len(body)))], body)
    body = b"eco tls: " + path.encode("latin1")
    return (200, "OK", [("Content-Type", "text/plain"),
                        ("Content-Length", str(len(body)))], body)


def resp(code, motivo, headers, body):
    h = "HTTP/1.0 %d %s\r\n" % (code, motivo)
    for k, v in headers:
        h += "%s: %s\r\n" % (k, v)
    h += "Connection: close\r\n\r\n"
    return h.encode("latin1") + body


def atiende(cn):
    try:
        cn.settimeout(60)
        d = b""
        while b"\r\n\r\n" not in d and len(d) < 8192:
            t = cn.recv(8192 - len(d))
            if not t:
                print("tls-stub: cliente cerro sin pedir", flush=True)
                return
            d = d + t
        try:
            txt = d.decode("latin1")
        except Exception:
            return
        lin = txt.split("\r\n", 1)[0]
        partes = lin.split(" ")
        metodo = partes[0] if partes else ""
        path = partes[1] if len(partes) > 1 else "/"
        print("tls-stub: %r -> %s" % (lin[:80], path), flush=True)
        if metodo != "GET":
            cn.sendall(resp(405, "Method",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", "10")],
                             b"solo GET\n"))
            return
        code, motivo, headers, body = cuerpo_pagina(path.split("?", 1)[0])
        cn.sendall(resp(code, motivo, headers, body))
        print("tls-stub: fin %s (%d B pedidos)" % (path, len(d)),
              flush=True)
    except ssl.SSLError as e:
        print("tls-stub: fallo TLS %r" % (e,), flush=True)
    except OSError as e:
        print("tls-stub: fallo red %r" % (e,), flush=True)
    finally:
        try:
            cn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            cn.close()
        except OSError:
            pass


def main():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(BASE + "/tls_srv_cert.pem",
                        BASE + "/tls_srv_key.pem")
    # El stub acepta lo que ofrezca el cliente (ECDHE+RSA+AESGCM).
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", PORT))
    s.listen(8)
    print("tls-stub: escuchando en 127.0.0.1:%d" % PORT, flush=True)
    while True:
        try:
            cn, _ = s.accept()
        except OSError:
            continue
        try:
            tc = ctx.wrap_socket(cn, server_side=True)
        except ssl.SSLError as e:
            print("tls-stub: handshake fallo %r" % (e,), flush=True)
            try:
                cn.close()
            except OSError:
                pass
            continue
        print("tls-stub: handshake ok %s" % (tc.version(),), flush=True)
        threading.Thread(target=atiende, args=(tc,),
                         daemon=True).start()


if __name__ == "__main__":
    main()
