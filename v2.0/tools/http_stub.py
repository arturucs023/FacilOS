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
  /chunked              : Transfer-Encoding: chunked ("hola0", v2.4.3 OK).
  /cablarga             : cabecera desconocida de 600 B (se ignora).
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
  F7-C (FacilBrowser): paginas HTML de prueba (todas text/html).
  /pagina               : portada con titulo, parrafos, enlace.
  /enlaces              : enlaces absolutos y relativos.
  /lista                : listas ul/ol, anidada.
  /entidades            : entidades HTML y UTF-8.
  /malformada           : tags sin cerrar, desconocidos, script.
  /img-pagina           : imagenes con/sin alt.
  /busca[?q=...]        : resultados de busqueda de ejemplo.
  /grande-html          : 70 KB con Content-Length (cabe en 256K).
  /grande-chunked       : ~200 KB HTML en trozos chunked de 8K.
  v2.4.4 (cookies + POST):
  /galleta             : Set-Cookie sesion=abc123 + tema=oscuro.
  /eco-cookie          : devuelve el Cookie recibido ("visto: ...").
  /form-post           : formulario method=post action=/eco-post.
  /eco-post            : devuelve el cuerpo POST ("visto: ...").
  redirecciones (sin version: el codigo 301/302 nunca se probo):
  /redir               : 302 a /redir2 (cadena de 2 saltos).
  /redir2              : 302 a /pagina + Set-Cookie ruta=redir.
  imagenes (BMP/GIF):
  /foto.bmp            : BMP 24-bit 48x32 (mosaico 8 colores).
  /foto.gif            : GIF 48x32 (mismo mosaico).
  /img-bmp             : pagina con <img> bmp + gif.
  /foto.png            : PNG 48x32 (mosaico 8 colores).
  /foto.jpg            : JPEG 48x32 4:2:2 (mosaico).
  /img-todo            : pagina con <img> bmp+gif+png+jpg.
  /estilo.css          : hoja externa (h1 azul + .x verde).
  /con-css             : pagina con <link rel=stylesheet>.
"""
import socket
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 18080

CUERPO8K = bytes(((i * 131 + 7) & 0xFF) for i in range(8192))

# GIF 48x32 real (8 colores; mismo mosaico que /foto.bmp).
GIF_FOTO = bytes.fromhex(
    "47494638376130002000820000e61e1e1ec81e1e50e6f0c81ec828b41"
    "ec8c8f0f0f01414142c00000000300020004008ff0001081c18a0a041"
    "0108130e58c89080c38705224a3440b1e2818b18132a64b8f02144891"
    "12b5ac47871204183053d3a041952a40192254d0240595023428e0b5d"
    "be842973264d9b02700e50498065019a01800a256a5427cc033d97aa"
    "6ceaf2694fa440a98ab42a13ab4da91eb99af4aa11ec47903aaffefc8"
    "a93294ba730c96ee4e8166d559e3295b69dfaf62e49b528cdaeecbb1"
    "5efd8b51aeb4ef48b11f041b61cb58efcdb15f1dc862ae1523e8c522f"
    "5dbe20c59eec0c19b347c9141dd72cdd11f4e2c29b473f2ebbf734e1c"
    "918e5deac7df635ee989c675f6e6ddb2eecc679590f75ddf23870d9ab"
    "697f2eee9ba26881480517bd6ddd30f4a4ca153751ff0db532e9c4cc0"
    "b68461e3cfaf0e5d4c7778f8dddf2eec8dc77d2f779febd78f58c3d57"
    "5f7ff799f6d07a02f2275c81c4f5269f7eec7de799818385e6dd800b0"
    "6c55b85d54198a06e1a4ee72080220504003b")


# v2.5: PNG 48x32 (mismo mosaico) y JPEG 48x32 4:2:2 (PIL q75).
PNG_FOTO = bytes.fromhex(
    "89504e470d0a1a0a0000000d4948445200000030000000200802000000dbb66c"
    "4b000000c549444154789ccdd7b10dc2301404d0fb57a676edea171e21635051"
    "330623300135353543b8a66282d41e819aef0214c5e45e799215ebebe4d89652"
    "c2a7e7348564be8700b8d4b8ea3c87e475ba86e478ebbe554a4808318418428c"
    "b5d642e4ee3bb68a10438821c458aef5ebd9e07f6c95dc8408318418abe5b1e2"
    "8fe3c35a253721420c21c65acd9bdc637ca356c94d88104388b17c5806dd8e7d"
    "55abe426448821c458eecea1716f2effa155721322c410626cc979c797bc77ad"
    "929b1021867b6f207a0361304ae73a0c582e0000000049454e44ae426082"
    )
JPG_FOTO = bytes.fromhex(
    "ffd8ffe000104a46494600010100000100010000ffdb00430008060607060508"
    "0707070909080a0c140d0c0b0b0c1912130f141d1a1f1e1d1a1c1c20242e2720"
    "222c231c1c2837292c30313434341f27393d38323c2e333432ffdb0043010909"
    "090c0b0c180d0d1832211c213232323232323232323232323232323232323232"
    "323232323232323232323232323232323232323232323232323232323232ffc0"
    "0011080020003003012200021101031101ffc4001f0000010501010101010100"
    "000000000000000102030405060708090a0bffc400b510000201030302040305"
    "0504040000017d01020300041105122131410613516107227114328191a10823"
    "42b1c11552d1f02433627282090a161718191a25262728292a3435363738393a"
    "434445464748494a535455565758595a636465666768696a737475767778797a"
    "838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7"
    "b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8e9eaf1"
    "f2f3f4f5f6f7f8f9faffc4001f01000301010101010101010100000000000001"
    "02030405060708090a0bffc400b5110002010204040304070504040001027700"
    "0102031104052131061241510761711322328108144291a1b1c109233352f015"
    "6272d10a162434e125f11718191a262728292a35363738393a43444546474849"
    "4a535455565758595a636465666768696a737475767778797a82838485868788"
    "898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4"
    "c5c6c7c8c9cad2d3d4d5d6d7d8d9dae2e3e4e5e6e7e8e9eaf2f3f4f5f6f7f8f9"
    "faffda000c03010002110311003f00cef107c26ff841f43b8f11ff006dfdb7ec"
    "7b7fd1fecbe5efdec13ef6f38c6ecf4ed585e1ef10fda3ed3fe8bb76edff0096"
    "99f5f6aefb42f117d9b59b79becbbb6eee3ccc7f091e95a5e27f0c7fc2d5fb2f"
    "fa67f65ff66eff00f965e7f99e66df75c63cbf7ce7da9e372dbaf7beff009fa9"
    "54f1d3a5ff0009d98d3b29ebc97f89747cd15a6b1dafd3cce21b4ffed13f6bf3"
    "7cbf33f876e718e3ae7daba18bc4bff093c8347fb27d9bed1ff2d7ccdfb76fcd"
    "d3033f771d7bd79ef8817fe107d72e3c39ff001fbf63dbfe91feaf7ef50ff779"
    "c63763af6ae8daeffb447d936797e67f16738c73d3f0ae29d66f929e35737269"
    "4e5b7b3dbdee55f1dad17696f6b7560b075f077c565f2e7a6bde9c6c97b38ad5"
    "2bcaee5a5d5d2be9e66eded97fc235e5fef3ed3f68cff0ecdbb71f5cfdeaea34"
    "2f187d9b46b787ec3bb6eee7cec7f113fddae1f4cd4ffe113f37f73f6afb4e3f"
    "8b66ddb9f639fbdfa56c45a3ff00c24f18d63cff00b37da3fe596cdfb76fcbd7"
    "233f773d3bd774b17ec68aa98c7cd45e8aaedcd2edc8b58db5577a3e5bf544c6"
    "385cd65ede8cfd9d67f14ace574b4b59d976d56ba79b218bc4bff093c8347fb2"
    "7d9bed1ff2d7ccdfb76fcdd3033f771d7bd36f6cbfe11af2ff0079f69fb467f8"
    "766ddb8fae7ef56135dff688fb26cf2fccfe2ce718e7a7e15774cd4ffe113f37"
    "f73f6afb4e3f8b66ddb9f639fbdfa57360a704f972f8f23ff9f37bf3bb6b2f69"
    "2f86cba7f77fbc3a98aab47fe13f39a7a4f5b5d6dd35879c7bfe0771a178c3ec"
    "da35bc3f61ddb7773e763f889feed79e7883e137fc20fa1dc788ff00b6fedbf6"
    "3dbfe8ff0065f2f7ef609f7b79c63767a76ae8e2d1ff00e1278c6b1e7fd9bed1"
    "ff002cb66fdbb7e5eb919fbb9e9de9ba1788becdacdbcdf65ddb7771e663f848"
    "f4aed83a38ce784349c349c75d1eb757d13b34d5d682786c460ad89c2be7a6bd"
    "ee8bd9c56ab7bb9e9f3f77cce07c3de21fb47da7fd176eddbff2d33ebed5b2da"
    "7ff689fb5f9be5f99fc3b738c71d73ed5dbf89fc31ff000b57ecbfe99fd97fd9"
    "bbff00e5979fe6799b7dd718f2fdf39f6af27f102ffc20fae5c7873fe3f7ec7b"
    "7fd23fd5efdea1feef38c6ec75ed5c31a789c0d6757092e593d1e89e9f3bae88"
    "b947059c47dbca7ecea4b5752ce5cd6d3979344ba6bfddf33fffd9"
    )


def resp(code, motivo, headers, body):
    h = "HTTP/1.0 %d %s\r\n" % (code, motivo)
    for k, v in headers:
        h += "%s: %s\r\n" % (k, v)
    h += "Connection: close\r\n\r\n"
    return h.encode("latin1") + body


def cab(txt, nombre):
    # v2.4.4: valor de cabecera de la peticion (insensible a Mayus).
    nb = nombre.lower()
    for lin in txt.split("\r\n")[1:]:
        if not lin:
            break
        p = lin.find(":")
        if p > 0 and lin[:p].strip().lower() == nb:
            return lin[p + 1:].strip()
    return ""


def cuerpo_post(cn, d, txt):
    # v2.4.4: cuerpo segun Content-Length (lo ya leido + resto).
    n = 0
    for lin in txt.split("\r\n")[1:]:
        if not lin:
            break
        p = lin.find(":")
        if p > 0 and lin[:p].strip().lower() == "content-length":
            try:
                n = int(lin[p + 1:].strip())
            except ValueError:
                n = 0
            break
    if n < 0 or n > 16384:
        return b""
    partes = d.split(b"\r\n\r\n", 1)
    cuerpo = partes[1] if len(partes) > 1 else b""
    while len(cuerpo) < n:
        try:
            t = cn.recv(min(4096, n - len(cuerpo)))
        except OSError:
            break
        if not t:
            break
        cuerpo += t
    return cuerpo[:n]


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
        metodo = partes[0] if partes else ""
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
        elif path == "/cablarga":
            # v2.4.3+hubbub: cabecera desconocida de 600 B (tipo CSP
            # real): debe ignorarse, no tumbar la respuesta.
            h = ("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n"
                 "X-Relleno: " + "abcd1234" * 75 + "\r\n"
                 "Content-Length: 15\r\nConnection: close\r\n\r\n")
            cn.sendall(h.encode("latin1"))
            cn.sendall(b"<p>cablarga</p>")
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
        elif path == "/pagina" or path == "/":
            body = ("<!DOCTYPE html><html><head>"
                    "<title>Inicio Facil</title></head><body>"
                    "<h1>FacilBrowser</h1>"
                    "<p>Pagina de <b>prueba</b> con "
                    "<a href=\"/enlaces\">enlaces</a>.</p>"
                    "<p>Segunda linea con entidad &amp; y e\xc3\xb1e.</p>"
                    "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/css":
            body = ("<html><head><title>CSS</title>"
                    "<style>p{color:red} .v{color:#00ff00} "
                    "h1{background:#000080;color:white}</style>"
                    "</head><body>"
                    "<h1>Cabecera</h1>"
                    "<p>Rojo por etiqueta.</p>"
                    "<p class=\"v\">Verde por clase.</p>"
                    "<p style=\"color:#ffff00\">Amarillo inline.</p>"
                    "<p><b>Negrita</b></p>"
                    "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/enlaces":
            body = ("<html><head><title>Enlaces</title></head><body>"
                    "<h2>Enlaces</h2><ul>"
                    "<li><a href=\"http://10.0.2.2:18080/pagina\">"
                    "absoluto</a></li>"
                    "<li><a href=\"/pagina\">raiz</a></li>"
                    "<li><a href=\"pagina\">relativo</a></li>"
                    "<li><a href=\"../pagina\">padre</a></li>"
                    "<li><a href=\"/lista\">lista</a></li>"
                    "<li><a href=\"https://example.com/\">seguro</a></li>"
                    "</ul></body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/lista":
            body = ("<html><head><title>Listas</title></head><body>"
                    "<h2>Compra</h2><ul><li>pan</li><li>leche<ul>"
                    "<li>entera</li></ul></li></ul>"
                    "<h2>Pasos</h2><ol><li>primero</li>"
                    "<li>segundo</li></ol></body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/entidades":
            body = ("<html><head><title>Entidades</title></head><body>"
                    "<p>&amp;&lt;&gt;&quot;&apos;&#65;&#x41;&nbsp;fin</p>"
                    "<p>ca\xc3\xb1a \xe6\x97\xa5</p></body></html>"
                    ).encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/malformada":
            body = ("<html><head><title>Rota</title></head><body>"
                    "<p>uno<p>dos<foo>misterio</foo>"
                    "<!-- comentario -->"
                    "<script>if(a<b){mala();}</script>"
                    "<b><i>cruce</b></i>"
                    "<p>fin sin cerrar").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/img-pagina":
            body = ("<html><head><title>Fotos</title></head><body>"
                    "<h2>Fotos</h2>"
                    "<img src=\"/img1.png\" alt=\"amanecer\">"
                    "<p>Texto entre fotos.</p>"
                    "<img src=\"foto.png\">"
                    "<img src=\"/noexiste.png\" alt=\"rota\">"
                    "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/busca" or path.startswith("/busca?"):
            q = ""
            if "?" in path:
                q = path.split("?", 1)[1]
                if q.startswith("q="):
                    q = q[2:]
            body = ("<html><head><title>Busca</title></head><body>"
                    "<h2>Resultados de " + q + "</h2><ul>"
                    "<li><a href=\"/pagina\">Inicio Facil</a> "
                    "portada de ejemplo</li>"
                    "<li><a href=\"/lista\">Listas</a> "
                    "listas de ejemplo</li>"
                    "</ul></body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/grande-html":
            trozo = "<p>relleno de prueba para superar el tope</p>"
            body = ("<html><head><title>Grande</title></head><body>" +
                    trozo * 1600 + "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/html"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/grande-chunked":
            # v2.4.3: 200 KB HTML en trozos de 8K + trailers.
            trozo = "<p>trozo chunked para red util v2.4.3</p>"
            body = ("<html><head><title>GrandeC</title></head><body>" +
                    trozo * 4600 + "</body></html>").encode("latin1")
            h = ("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n"
                 "Transfer-Encoding: chunked\r\n"
                 "Connection: close\r\n\r\n")
            cn.sendall(h.encode("latin1"))
            for i in range(0, len(body), 8192):
                t = body[i:i + 8192]
                cn.sendall(("%X\r\n" % len(t)).encode("latin1"))
                cn.sendall(t)
                cn.sendall(b"\r\n")
                time.sleep(0.02)
            cn.sendall(("0\r\nX-Partes: %d\r\n\r\n" %
                        (len(body) // 8192 + 1)).encode("latin1"))
        elif path == "/galleta":
            # v2.4.4: dos Set-Cookie (prueba jar multiple + attrs).
            body = ("<html><head><title>Galleta</title></head><body>"
                    "<h1>Sesion</h1><p><a href=\"/eco-cookie\">"
                    "ver cookie</a></p></body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "text/html"),
                          ("Set-Cookie", "sesion=abc123; Path=/"),
                          ("Set-Cookie", "tema=oscuro; Path=/"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/eco-cookie":
            ck = cab(txt, "cookie")
            print("stub: Cookie: %r" % ck, flush=True)
            body = ("visto: " + (ck if ck else "vacio")).encode("latin1")
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "text/plain"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/form-post":
            body = ("<html><head><title>FormaPost</title></head><body>"
                    "<h1>Entra</h1>"
                    "<form method=\"post\" action=\"/eco-post\">"
                    "<input name=\"q\"></form>"
                    "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "text/html"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/eco-post":
            if metodo != "POST":
                cn.sendall(resp(405, "Method",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", "10")],
                             b"solo POST\n"))
            else:
                cpo = cuerpo_post(cn, d, txt)
                print("stub: POST %d B" % len(cpo), flush=True)
                body = b"visto: " + cpo
                cn.sendall(resp(200, "OK",
                             [("Content-Type", "text/plain"),
                              ("Content-Length", str(len(body)))],
                             body))
        elif path == "/redir":
            # Cadena de 2 saltos: /redir -> /redir2 -> /pagina.
            cn.sendall(resp(302, "Found",
                         [("Location", "/redir2"),
                          ("Content-Length", "0")],
                         b""))
        elif path == "/redir2":
            # El 302 tambien trae Set-Cookie (el login tipico):
            # fbrowser lo cosecha antes de seguir.
            cn.sendall(resp(302, "Found",
                         [("Location", "/pagina"),
                          ("Set-Cookie", "ruta=redir; Path=/"),
                          ("Content-Length", "0")],
                         b""))
        elif path == "/foto.bmp":
            # BMP 24-bit 48x32: 8 bloques de color (4px/6px).
            import struct as _st
            pal = [(230, 30, 30), (30, 200, 30), (30, 80, 230),
                   (240, 200, 30), (200, 40, 180), (30, 200, 200),
                   (240, 240, 240), (20, 20, 20)]
            px = b""
            for yy in range(32):
                for xx in range(48):
                    c = pal[(xx // 6 + (31 - yy) // 4) % 8]
                    px += bytes((c[2], c[1], c[0]))
            hd = _st.pack("<2sIHHI", b"BM", 54 + len(px), 0, 0, 54)
            hd += _st.pack("<IIIHHIIIIII", 40, 48, 32, 1, 24, 0,
                           len(px), 0, 0, 0, 0)
            body = hd + px
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "image/bmp"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/foto.gif":
            body = GIF_FOTO
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "image/gif"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/foto.png":
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "image/png"),
                          ("Content-Length", str(len(PNG_FOTO)))],
                         PNG_FOTO))
        elif path == "/foto.jpg":
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "image/jpeg"),
                          ("Content-Length", str(len(JPG_FOTO)))],
                         JPG_FOTO))
        elif path == "/img-todo":
            body = ("<html><head><title>Fotos4</title></head><body>"
                    "<h2>Fotos BMP+GIF+PNG+JPG</h2>"
                    "<img src=\"/foto.bmp\" alt=\"mosaico\">"
                    "<p>Entre medias.</p>"
                    "<img src=\"/foto.gif\" alt=\"trama\">"
                    "<img src=\"/foto.png\" alt=\"mosa\">"
                    "<img src=\"/foto.jpg\" alt=\"foto\">"
                    "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "text/html"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/estilo.css":
            body = ("h1{color:#0000ee}\n.x{color:#00aa00}\n"
                    "p{color:#333333}").encode("latin1")
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "text/css"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/con-css":
            body = ("<html><head><title>ConCSS</title>"
                    "<link rel=\"stylesheet\" href=\"/estilo.css\">"
                    "</head><body>"
                    "<h1>Titulo externo</h1>"
                    "<p class=\"x\">Verde externo</p>"
                    "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "text/html"),
                          ("Content-Length", str(len(body)))],
                         body))
        elif path == "/img-bmp":
            body = ("<html><head><title>Fotos2</title></head><body>"
                    "<h2>Fotos BMP+GIF</h2>"
                    "<img src=\"/foto.bmp\" alt=\"mosaico\">"
                    "<p>Entre medias.</p>"
                    "<img src=\"/foto.gif\" alt=\"trama\">"
                    "</body></html>").encode("latin1")
            cn.sendall(resp(200, "OK",
                         [("Content-Type", "text/html"),
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
