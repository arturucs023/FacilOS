#!/usr/bin/env python3
"""Regenera la CA de test + cert del stub TLS y nsta.h.

El stub (tls_stub.py) sirve 10.0.2.2 con un cert firmado por la CA de
test; el cliente (nsfetch) solo confia en esa CA (nsta.h) + raices
reales (nsca.h). Los .pem vivian en C:/Temp y se pierden al
reinstalar Windows: este script los recrea y rota nsta.h.

Uso: python v2.0/tools/gen_tls_stub.py
Requiere: pip install cryptography (solo maquina de build, no FacilOS).
"""
import datetime
import pathlib
import sys

BASE = pathlib.Path(__file__).parent.parent
sys.path.insert(0, str(BASE / "tools"))
import gen_nsca

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID


def crea():
    ca_clave = rsa.generate_private_key(public_exponent=65537,
                                        key_size=2048)
    nombre_ca = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,
                                              "F32 Test TLS CA")])
    ahora = datetime.datetime.now(datetime.timezone.utc)
    ca_cert = (x509.CertificateBuilder()
               .subject_name(nombre_ca).issuer_name(nombre_ca)
               .public_key(ca_clave.public_key())
               .serial_number(x509.random_serial_number())
               .not_valid_before(ahora)
               .not_valid_after(ahora + datetime.timedelta(days=3650))
               .add_extension(x509.BasicConstraints(ca=True, path_length=0),
                              critical=True)
               .add_extension(x509.SubjectKeyIdentifier.from_public_key(
                   ca_clave.public_key()), critical=False)
               .sign(ca_clave, hashes.SHA256()))
    srv_clave = rsa.generate_private_key(public_exponent=65537,
                                         key_size=2048)
    srv_cert = (x509.CertificateBuilder()
                .subject_name(x509.Name([x509.NameAttribute(
                    NameOID.COMMON_NAME, "10.0.2.2")]))
                .issuer_name(nombre_ca)
                .public_key(srv_clave.public_key())
                .serial_number(x509.random_serial_number())
                .not_valid_before(ahora)
                .not_valid_after(ahora + datetime.timedelta(days=1825))
                .add_extension(x509.BasicConstraints(ca=False,
                                                     path_length=None),
                               critical=True)
                .add_extension(x509.SubjectAlternativeName([
                    x509.DNSName("10.0.2.2"),
                    x509.DNSName("localhost"),
                    x509.IPAddress(__import__("ipaddress").ip_address(
                        "10.0.2.2"))]), critical=False)
                .sign(ca_clave, hashes.SHA256()))
    return ca_cert, srv_clave, srv_cert


def hx(nombre, datos, por_linea=12):
    out = ["static const unsigned char %s[%d] = {" % (nombre,
                                                     len(datos))]
    for i in range(0, len(datos), por_linea):
        trozo = datos[i:i + por_linea]
        out.append("    " + ", ".join("0x%02x" % c for c in trozo) + ",")
    out.append("};")
    return "\n".join(out)


def main():
    ca_cert, srv_clave, srv_cert = crea()
    tmp = pathlib.Path(r"C:\Temp")
    tmp.mkdir(parents=True, exist_ok=True)
    (tmp / "tls_srv_cert.pem").write_bytes(
        srv_cert.public_bytes(serialization.Encoding.PEM))
    (tmp / "tls_srv_key.pem").write_bytes(
        srv_clave.private_bytes(serialization.Encoding.PEM,
                                serialization.PrivateFormat.TraditionalOpenSSL,
                                serialization.NoEncryption()))
    # nsta.h desde la CA recien creada (mismo extractor que nsca.h).
    (tmp / "testca.cer").write_bytes(
        ca_cert.public_bytes(serialization.Encoding.DER))
    tipo, cn, der, n, e = gen_nsca.procesa(tmp / "testca.cer")
    assert tipo == "RSA", tipo
    L = []
    L.append("/* nsta.h: ancla de test TLS + suites (Hito 1/2).")
    L.append(" * Generado con v2.0/tools/gen_tls_stub.py (CA de test")
    L.append(" * autofirmada, solo stub local 10.0.2.2). No editar a mano.")
    L.append(" * Solo confia en la CA de test; internet real usa nsca.h.")
    L.append(" * Si se pierden C:/Temp/*.pem (reinstall), regenerar con")
    L.append(" * gen_tls_stub.py (rota la CA y este ancla). */")
    L.append("#ifndef F32NS_TA_H")
    L.append("#define F32NS_TA_H")
    L.append("")
    L.append('#include <stdint.h>')
    L.append('#include "bearssl.h"')
    L.append("")
    L.append(hx("TA_DN", der))
    L.append("")
    L.append(hx("TA_N", n))
    L.append("")
    L.append(hx("TA_E", e))
    L.append("")
    L.append("static const uint16_t suites[] = {")
    L.append("        BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,")
    L.append("        BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,")
    L.append("        BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,")
    L.append("        BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256")
    L.append("    };")
    L.append("")
    L.append("#endif")
    (BASE / "user" / "lib" / "nsta.h").write_text("\n".join(L) + "\n",
                                                  encoding="ascii")
    print("nsta.h: CN=%s N=%dB E=%dB" % (cn, len(n), len(e)))
    print("OK pems en C:\\Temp + nsta.h actualizado")


if __name__ == "__main__":
    main()
