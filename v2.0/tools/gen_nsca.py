#!/usr/bin/env python3
"""Genera v2.0/user/lib/nsca.h con las raices del almacen Windows.

Uso: python v2.0/tools/gen_nsca.py [dir_con_cer]
Lee *.cer (DER) del almacen (exportados con Export-Certificate),
extrae subject + SPKI (RSA/EC P-256/384/521) con parser ASN.1 propio
(solo stdlib) y emite nsca.h con br_x509_trust_anchor[].

El ancla de test (nsta.h, stub 10.0.2.2) se mantiene aparte y se
concatena en tiempo de ejecucion en nsfetch.c.
"""
import pathlib
import sys

BASE = pathlib.Path(__file__).parent.parent
OUT = BASE / "user" / "lib" / "nsca.h"
SRC = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(
    r"C:\Temp\roots")

OID_RSA = (1, 2, 840, 113549, 1, 1, 1)
OID_EC = (1, 2, 840, 10045, 2, 1)
CURVAS = {
    (1, 2, 840, 10045, 3, 1, 7): ("BR_EC_secp256r1", "P-256"),
    (1, 3, 132, 0, 34): ("BR_EC_secp384r1", "P-384"),
    (1, 3, 132, 0, 35): ("BR_EC_secp521r1", "P-521"),
}
OID_CN = (2, 5, 4, 3)


def tlv(b, p):
    tag = b[p]
    ln = b[p + 1]
    q = p + 2
    if ln & 0x80:
        n = ln & 0x7F
        if n == 0 or n > 4:
            raise ValueError("longitud no soportada")
        ln = int.from_bytes(b[q:q + n], "big")
        q += n
    if q + ln > len(b):
        raise ValueError("truncado")
    return tag, q, ln


def hijos(b, p, ln):
    r = []
    fin = p + ln
    while p < fin:
        ini = p
        tag, q, ln2 = tlv(b, p)
        r.append((tag, ini, q, ln2))
        p = q + ln2
    if p != fin:
        raise ValueError("hijos mal alineados")
    return r


def tlv_ini(b, p):
    # devuelve (tag, inicio, contenido, long) con inicio del tag
    tag = b[p]
    ln = b[p + 1]
    q = p + 2
    if ln & 0x80:
        n = ln & 0x7F
        ln = int.from_bytes(b[q:q + n], "big")
        q += n
    return tag, p, q, ln


def oid(b, p, ln):
    nums = []
    v = 0
    first = True
    for i in range(ln):
        c = b[p + i]
        v = (v << 7) | (c & 0x7F)
        if not (c & 0x80):
            if first:
                nums.append(v // 40)
                nums.append(v % 40)
                first = False
            else:
                nums.append(v)
            v = 0
    return tuple(nums)


def entero(b, p, ln):
    v = bytes(b[p:p + ln])
    if v and v[0] == 0:
        v = v[1:]
    return v


def cn_de_subject(b, p, ln):
    # subject = SEQ de RDNSet; busca OID 2.5.4.3
    try:
        for _, _, qp, lnp in hijos(b, p, ln):
            for _, _, qp2, lnp2 in hijos(b, qp, lnp):
                hs = hijos(b, qp2, lnp2)
                if len(hs) == 2 and hs[0][0] == 0x06:
                    if oid(b, hs[0][2], hs[0][3]) == OID_CN:
                        t, _, vp, vl = hs[1]
                        if t in (0x0C, 0x13, 0x16):
                            return bytes(b[vp:vp + vl]).decode(
                                "ascii", "replace")
    except (ValueError, IndexError):
        pass
    return "?"


def procesa(path):
    b = path.read_bytes()
    tag, _, q0, l0 = tlv_ini(b, 0)
    if tag != 0x30:
        raise ValueError("no es SEQ")
    ch = hijos(b, q0, l0)
    if not ch or ch[0][0] != 0x30:
        raise ValueError("sin tbs")
    tbs = hijos(b, ch[0][2], ch[0][3])
    base = 0
    if len(tbs) == 6 and tbs[0][0] == 0x02:
        # v1 sin [0] explicito: serial,sig,issuer,validity,subject,spki
        base = -1
    elif len(tbs) < 7:
        raise ValueError("tbs corto")
    # subject = tbs[5] (tras [0],serial,sig,issuer,validity)
    t_sub, ini_sub, p_sub, l_sub = tbs[5 + base]
    if t_sub != 0x30:
        raise ValueError("sin subject")
    der_sub = bytes(b[ini_sub:p_sub + l_sub])
    t_spki, _, p_spki, l_spki = tbs[6 + base]
    if t_spki != 0x30:
        raise ValueError("sin spki")
    hs = hijos(b, p_spki, l_spki)
    alg = hijos(b, hs[0][2], hs[0][3])
    o = oid(b, alg[0][2], alg[0][3])
    t_bit, _, p_bit, l_bit = hs[1]
    if t_bit != 0x03 or b[p_bit] != 0:
        raise ValueError("bitstring raro")
    clave = bytes(b[p_bit + 1:p_bit + l_bit])
    cn = cn_de_subject(b, p_sub, l_sub)
    if o == OID_RSA:
        if clave[0] != 0x30:
            raise ValueError("RSA sin SEQ")
        t, q, ln = tlv(clave, 0)
        rn = hijos(clave, q, ln)
        n = entero(clave, rn[0][2], rn[0][3])
        e = entero(clave, rn[1][2], rn[1][3])
        return ("RSA", cn, der_sub, n, e)
    if o == OID_EC:
        if len(alg) < 2 or alg[1][0] != 0x06:
            raise ValueError("EC sin curva")
        co = oid(b, alg[1][2], alg[1][3])
        if co not in CURVAS:
            raise ValueError("curva no soportada %r" % (co,))
        if not clave or clave[0] != 0x04:
            raise ValueError("punto no descomprimido")
        return ("EC", cn, der_sub, CURVAS[co][0], clave)
    raise ValueError("algoritmo no soportado %r" % (o,))


def hx(nombre, datos, por_linea=12):
    out = ["static const unsigned char %s[%d] = {" % (nombre, len(datos))]
    for i in range(0, len(datos), por_linea):
        trozo = datos[i:i + por_linea]
        out.append("    " + ", ".join("0x%02x" % c for c in trozo) + ",")
    out.append("};")
    return "\n".join(out)


def main():
    fichs = sorted(SRC.glob("*.cer"))
    if not fichs:
        print("sin .cer en %s" % SRC)
        sys.exit(1)
    anclas = []
    saltadas = []
    for f in fichs:
        try:
            anclas.append((f.stem,) + procesa(f))
        except ValueError as ex:
            saltadas.append((f.name, str(ex)))
    anclas.sort(key=lambda a: a[1])
    L = []
    L.append("/* nsca.h: anclas de confianza TLS (raices reales).")
    L.append(" * Generado con v2.0/tools/gen_nsca.py desde el almacen")
    L.append(" * Windows (%d raices exportadas, %d aprovechables)." % (
        len(fichs), len(anclas)))
    L.append(" * RSA + ECDSA P-256/384/521 (lo que soporta BearSSL i31).")
    L.append(" * El ancla de test (nsta.h, stub 10.0.2.2) sigue aparte y")
    L.append(" * se concatena en nsfetch.c. NO editar a mano. */")
    L.append("#ifndef F32NS_CA_H")
    L.append("#define F32NS_CA_H")
    L.append("")
    L.append('#include <stdint.h>')
    L.append('#include "bearssl.h"')
    L.append("")
    for i, (_, tipo, cn, der, *resto) in enumerate(anclas):
        L.append("/* %d: %s [%s] */" % (i, cn, tipo))
        L.append(hx("NSCA_DN_%02d" % i, der))
        if tipo == "RSA":
            n, e = resto
            L.append(hx("NSCA_N_%02d" % i, n))
            L.append(hx("NSCA_E_%02d" % i, e))
        else:
            _, punto = resto
            L.append(hx("NSCA_Q_%02d" % i, punto))
        L.append("")
    L.append("static const br_x509_trust_anchor NSCA_TAS[] = {")
    for i, (_, tipo, cn, der, *resto) in enumerate(anclas):
        L.append("    /* %d: %s */" % (i, cn))
        L.append("    {")
        L.append("        { (unsigned char *)NSCA_DN_%02d," % i)
        L.append("          sizeof(NSCA_DN_%02d) }," % i)
        L.append("        BR_X509_TA_CA,")
        if tipo == "RSA":
            L.append("        { BR_KEYTYPE_RSA, { .rsa = {")
            L.append("            (unsigned char *)NSCA_N_%02d," % i)
            L.append("            sizeof(NSCA_N_%02d)," % i)
            L.append("            (unsigned char *)NSCA_E_%02d," % i)
            L.append("            sizeof(NSCA_E_%02d) } } }," % i)
        else:
            curva, _ = resto
            L.append("        { BR_KEYTYPE_EC, { .ec = {")
            L.append("            %s," % curva)
            L.append("            (unsigned char *)NSCA_Q_%02d," % i)
            L.append("            sizeof(NSCA_Q_%02d) } } }," % i)
        L.append("    },")
    L.append("};")
    L.append("#define NSCA_NTAS \\")
    L.append("    (sizeof(NSCA_TAS) / sizeof(NSCA_TAS[0]))")
    L.append("")
    L.append("#endif")
    OUT.write_text("\n".join(L) + "\n", encoding="ascii")
    print("nsca.h: %d anclas (%d RSA, %d EC)" % (
        len(anclas),
        sum(1 for a in anclas if a[1] == "RSA"),
        sum(1 for a in anclas if a[1] == "EC")))
    for n, m in saltadas:
        print("salta %s: %s" % (n, m))


if __name__ == "__main__":
    main()
