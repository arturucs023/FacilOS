# BearSSL en FacilOS (hito TLS, hito 1: stub local)

Alcance decidido: **cliente TLS 1.2 minimo** (ECDHE + AES-GCM/ChaCha20,
RSA/ECDSA, x509-minimal). Sin TLS 1.3, sin sesion externa, sin
verificacion OCSP. Solo valida contra la CA de test (stub local);
internet real necesita bundle de raices (pendiente).

## Fuentes (MIT, https://bearssl.org)

- `bearssl-0.6.tar.gz` (Thomas Pornin).
- Vendoreado recortado en `thirdparty/bearssl-0.6/`: `inc/` completo,
  `src/{ssl,x509,codec,hash,mac,rand,rsa,ec,symcipher,aead,int}/`
  (subconjunto TLS-1.2 cliente, 68 .c) + `src/{inner.h,config.h}`.
  Fuera quedan `tools/`, `test/`, `samples/`, `T0/`, curvas/primitivas
  no usadas (DES, CCM, EAX, TLS 1.0/1.1, RSA-PSS/OAEP, i15/i32/i62).

## Adaptacion (sin UNIX/BSD/Windows)

`build32.py` compila con `-DBR_USE_WIN32_TIME=0 -DBR_USE_WIN32_RAND=0
-DBR_USE_UNIX_TIME=0 -DBR_USE_URANDOM=0 -DBR_RDRAND=0 -DNDEBUG`
mas `-I inc -I src -I nslibc` (freestanding, `-w`).
El arranque usa la nslibc existente (`nsshim.o`: malloc/memcpy/...);
`nslibc/time.h` nuevo solo declara `time_t`/`time()` (64 bits como
MinGW) y `nsshim.c` lo implementa sobre `u_reloj` (ano CMOS de
2 digitos = 2000-2099).

## Medida (gcc32 -m32 -Os, 2026-11-11)

- 68 objetos BearSSL + `https` (motor + bomba): **~391K plano**,
  tope build 512K, 0 fallos, veto .idata/.edata/.rsrc OK.
- `t_tls` (KAT sin red: SHA-256, HMAC, GCM ida+vuelta+retoque,
  ECDH P-256, ECDSA P-256): **8 pruebas 0 fallos** en QEMU.

## Limites conocidos del hito 1

- Confianza: solo ancla de test (`https.c`, CA autofirmada para
  10.0.2.2). No validar contra ella = fallo (correcto).
- Entropia: semilla de reloj+ticks+tiempos de red (suficiente para
  el stub; revisar antes de internet real).
- Sin SNI problematico: se envia el hostname de la URL; el stub
  responde a 10.0.2.2 con SAN dNSName.
- `u_http_url` acepta `https://` (puerto 443 por defecto);
  fbrowser sigue rechazando https (integracion pendiente).
- Catalogo del nucleo ampliado a 63 entradas (cabian 48; hacian
  falta 49 con `https`).
