# BearSSL en FacilOS (hito TLS + hito HTTPS-real v2.5)

Alcance: **cliente TLS 1.2 minimo** (ECDHE + AES-GCM/ChaCha20,
RSA/ECDSA, x509-minimal). Sin TLS 1.3, sin sesion externa, sin
verificacion OCSP.

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

## Limites conocidos (actualizado v2.5)

- Confianza: ancla de test (`nsta.h`, CA autofirmada para
  10.0.2.2, stub local) + 42 raices reales generadas del almacen
  Windows con `v2.0/tools/gen_nsca.py` (`user/lib/nsca.h`, RSA +
  ECDSA P-256/384/521, lo que soporta BearSSL i31). Regenerar el
  bundle tras reinstalar Windows: exportar `Cert:\LocalMachine\Root`
  a `C:\Temp\roots\*.cer` y correr `gen_nsca.py`.
- Entropia: semilla de reloj+ticks+jitter (suficiente para el
  stub; revisar antes de internet real).
- SNI: se envia el hostname de la URL; el stub responde a
  10.0.2.2 con SAN dNSName. Verificacion de nombre via x509-minimal.
- `u_http_url` acepta `https://` (puerto 443 por defecto);
  `netsurf` usa `nsfetch` (HTTP+HTTPS via BearSSL, GET con redirecciones).
  `fbrowser` retirado en v2.5 (duplicado: `netsurf` es el navegador).
- Catalogo del nucleo ampliado a 63 entradas (cabian 48; hacian
  falta 49 con `https`).
