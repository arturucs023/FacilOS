"""Par DNS controlado para F6-D (anfitrion).

Escucha UDP+TCP en 127.0.0.1:PUERTO (defecto 5353). Zonas
deterministas para probar CNAME, TC->TCP, errores y TTL.
Uso:  python tools/dns_stub.py [puerto] [--alt]
--alt: directo.test responde 192.0.2.99 (prueba de servidor).
"""
import socket
import struct
import sys
import threading

PUERTO = int(sys.argv[1]) if len(sys.argv) > 1 else 5353
ALT = "--alt" in sys.argv


def ip(*b):
    return bytes(b)


def nombre_wire(dom):
    out = b""
    for et in dom.split("."):
        out += bytes([len(et)]) + et.encode()
    return out + b"\x00"


def cab(xid, band, qd=1, an=0, ns=0, ar=0):
    return struct.pack(">HHHHHH", xid, band, qd, an, ns, ar)


def rr_nombre_ptr(qoff=12):
    return struct.pack(">H", 0xC000 | qoff)


def rr_a(ttl, ipa):
    return struct.pack(">HHIH", 1, 1, ttl, 4) + bytes(ipa)


def rr_cname(ttl, wire_tgt):
    return struct.pack(">HHIH", 5, 1, ttl, len(wire_tgt)) + wire_tgt


def resp(xid, q, an_rds, band=0x8180):
    """q: pregunta wire completa (QNAME+QTYPE+QCLASS). an_rds: lista
    de (nombre_ptr_off_o_wire, rdata_sin_cabecera)."""
    msg = cab(xid, band, 1, len(an_rds)) + q
    for nm, rd in an_rds:
        if isinstance(nm, int):
            msg += rr_nombre_ptr(nm)
        else:
            msg += nm
        msg += rd
    return msg


def pregunta_dom(m):
    # extrae QNAME en texto desde offset 12 (sin seguir punteros)
    o, partes = 12, []
    while True:
        ln = m[o]
        if ln == 0:
            break
        if ln & 0xC0:
            return None
        o += 1
        partes.append(m[o:o + ln].decode("ascii", "replace"))
        o += ln
        if len(partes) > 32 or o >= len(m):
            return None
    return ".".join(partes).lower()


def zona(dom, es_tcp=False):
    """Devuelve (an_rds, band_extra) o None si NXDOMAIN."""
    A = (192, 0, 2, 10) if not ALT else (192, 0, 2, 99)
    if dom == "directo.test":
        return ([(12, rr_a(300, A))], 0x8180)
    if dom == "alias-full.test":
        # cadena completa en una respuesta
        return ([(12, rr_cname(200, nombre_wire("medio-full.test"))),
                 (12, rr_a(300, (192, 0, 2, 20)))], 0x8180)
    if dom == "medio-full.test":
        return ([(12, rr_a(300, (192, 0, 2, 20)))], 0x8180)
    if dom == "alias1.test":
        return ([(12, rr_cname(200, nombre_wire("alias2.test")))],
                0x8180)
    if dom == "alias2.test":
        return ([(12, rr_cname(200, nombre_wire("alias3.test")))],
                0x8180)
    if dom == "alias3.test":
        return ([(12, rr_a(300, (192, 0, 2, 30)))], 0x8180)
    if dom and dom[0] == "c" and dom[1:-5].isdigit() and \
            dom.endswith(".test"):
        k = int(dom[1:-5])
        if 1 <= k <= 8:
            nxt = "c%d.test" % (k + 1)
            return ([(12, rr_cname(200, nombre_wire(nxt)))], 0x8180)
        if k == 9:
            return ([(12, rr_a(300, (192, 0, 2, 40)))], 0x8180)
    if dom and dom[0] == "d" and dom[1:-5].isdigit() and \
            dom.endswith(".test"):
        k = int(dom[1:-5])
        if 1 <= k <= 10:
            nxt = "d%d.test" % (k + 1)
            return ([(12, rr_cname(200, nombre_wire(nxt)))], 0x8180)
        if k == 11:
            return ([(12, rr_a(300, (192, 0, 2, 41)))], 0x8180)
    if dom == "loop.test":
        return ([(12, rr_cname(200, nombre_wire("loop2.test")))],
                0x8180)
    if dom == "loop2.test":
        return ([(12, rr_cname(200, nombre_wire("loop.test")))],
                0x8180)
    if dom == "nx.test":
        return ([], 0x8183)
    if dom == "nodata.test":
        return ([], 0x8180)
    if dom == "srvfail.test":
        return ([], 0x8182)
    if dom == "refused.test":
        return ([], 0x8185)
    if dom == "tc.test":
        if es_tcp:
            return ([(12, rr_a(300, (192, 0, 2, 60)))], 0x8180)
        return (None, 0x8380)  # TC=1, sin respuestas
    if dom == "multi.test":
        return ([(12, rr_a(300, (192, 0, 2, 70))),
                 (12, rr_a(300, (192, 0, 2, 71))),
                 (12, rr_a(300, (192, 0, 2, 72)))], 0x8180)
    if dom == "ttl0.test":
        return ([(12, rr_a(0, (192, 0, 2, 80)))], 0x8180)
    if dom == "ttlbig.test":
        return ([(12, rr_a(999999999, (192, 0, 2, 81)))], 0x8180)
    if dom.startswith("gen") and dom.endswith(".test"):
        try:
            k = int(dom[3:-5])
        except ValueError:
            return None
        if 1 <= k <= 60:
            return ([(12, rr_a(300, (192, 0, 2, 100 + k % 150)))],
                    0x8180)
    return None


def atiende(m, es_tcp=False):
    if len(m) < 17:
        return None
    xid = struct.unpack(">H", m[:2])[0]
    dom = pregunta_dom(m)
    if dom is None:
        return None
    q = m[12:]
    z = zona(dom, es_tcp)
    if z is None:
        return cab(xid, 0x8183, 1, 0) + q
    an_rds, band = z
    if an_rds is None:  # TC sin respuestas
        return cab(xid, band, 1, 0) + q
    return resp(xid, q, an_rds, band)


def sirve_udp():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", PUERTO))
    print("stub: UDP en %d (alt=%s)" % (PUERTO, ALT), flush=True)
    while True:
        try:
            d, a = s.recvfrom(4096)
        except OSError:
            continue
        try:
            r = atiende(d)
        except Exception:
            continue
        if r:
            try:
                s.sendto(r, a)
            except OSError:
                pass


def uno_tcp(cn):
    try:
        pre = b""
        while len(pre) < 2:
            t = cn.recv(2 - len(pre))
            if not t:
                return
            pre += t
        ln = struct.unpack(">H", pre)[0]
        if ln < 12 or ln > 4096:
            return
        q = b""
        while len(q) < ln:
            t = cn.recv(ln - len(q))
            if not t:
                return
            q += t
        r = atiende(q, True)
        if r:
            cn.sendall(struct.pack(">H", len(r)) + r)
    except OSError:
        pass
    finally:
        try:
            cn.close()
        except OSError:
            pass


def sirve_tcp():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", PUERTO))
    s.listen(8)
    print("stub: TCP en %d (alt=%s)" % (PUERTO, ALT), flush=True)
    while True:
        try:
            cn, _ = s.accept()
        except OSError:
            continue
        threading.Thread(target=uno_tcp, args=(cn,),
                         daemon=True).start()


if __name__ == "__main__":
    for f in (sirve_udp, sirve_tcp):
        threading.Thread(target=f, daemon=True).start()
        print("stub: listo (directo=%s)" % (
            "192.0.2.99" if ALT else "192.0.2.10"), flush=True)
    import time
    while True:
        time.sleep(3600)
