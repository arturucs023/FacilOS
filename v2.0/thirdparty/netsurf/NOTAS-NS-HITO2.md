# Hito 2 NetSurf — Fetch HTTP/HTTPS (`nsfetch`) + KAT (`t_fetch`)

Rama `v2.4.3`, sin tocar `main`. FacilBrowser intacto.
Nada enlazado al sistema salvo `t_fetch` y `https`.

## 1. Qué es `nsfetch`

`v2.0/user/lib/nsfetch.{h,c}`: fetch síncrono con redirecciones
(301/302/303/307/308, max 3, con `Location` relativa o absoluta
vía `u_html_resuelve_url`):
- `http://` → `u_http_trae` (DNS + TCP + HTTP/1.0 + cierre).
- `https://` → motor TLS movido de `https.c` (**sin duplicar**)
  + el MISMO parser incremental `u_http_res_*` alimentado con
  los registros descifrados (chunked gratis si apareciera).
- API: `int nsfetch_get(url, &res)` con
  `res = { cuerpo, tam, codigo, suite, detalle }`. El cuerpo vive
  en estático interno (256K+1, como `https.c`): válido hasta la
  próxima llamada, sin `free`.
- Códigos: 0 ok (cualquier HTTP, `codigo` 100..599); -1 red/TCP/
  TLS/timeout (`detalle` = BearSSL/`u_http`); -2 URL/esquema;
  -3 DNS; -4 redirecciones; -5 cuerpo > 256K.
- Límite conocido: el camino https asume Content-Length o
  cierre (igual que `https.c`); chunked-sobre-TLS funcionaría por
  el parser compartido pero no hay KAT (el stub no lo emite).

## 2. `https.c` adelgazado a CLI

El motor vive ahora en `nsfetch.c`; `https.c` parsea args,
llama a `nsfetch_get` y presenta el resultado. Cambios visibles
respecto al Hito TLS (documentados, el programa no está en FULL):
- "sin adaptador / sin IP / sin TCP / timeout / TCP roto" se
  consolidan en "https: fallo de red/TLS (<detalle>)", rc 1.
- "DNS fallo" (rc 4) y "cuerpo demasiado grande" (rc 4) iguales.
- Nuevo: "demasiadas redirecciones", rc 1 (antes no seguía).
- OK idéntico: "https: TLS ok suite 0x... bytes ..." + cuerpo.
- Regla nueva: **los blobs binarios no se copian a mano.**
  La causa del `NOT_TRUSTED` fue mi transcripción del ancla
  (bytes desplazados); se extrae mecánicamente de `https.c`
  (git HEAD) a `v2.0/user/lib/nsta.h`, verificado byte a byte
  con script antes de compilar.

## 3. `t_fetch`: 10 pruebas, 0 fallos (QEMU, stubs locales)

| KAT | Qué comprueba |
|---|---|
| http-ok | `GET /normal` → 200 + cuerpo exacto `"Hello, world!"` |
| http-pagina | `GET /pagina` → 200 + cuerpo |
| http-404 | `GET /noencontrado` → 404 |
| redir-sigue | `GET /redir302` → sigue a `/normal` → 200 + cuerpo exacto |
| https-ok/eco | `GET https://.../pagina` → 200 + `"TLS ok"`; `/hola` → `"eco tls:"` (handshake TLS 1.2 real contra tls_stub) |
| https-suite | suite negociada != 0 |
| transporte-cerrado | puerto 9 cerrado → rc < 0 |
| url-mala/esquema | `"://mal"`, `"ftp://x/"` → `NSFETCH_E_URL` |

## 4. Build y harness

- `build32.py`: `NSFETCH` (nsfetch.c con BRINCS), enlazado con
  `con_br` (`t_fetch`, `https`); tope `t_fetch` 524288 (igual
  que `https`); `t_fetch` registrado. Sin `-lgcc` en programas:
  `nsfetch.c` no usa división 64 bit (el `u_http`/`BearSSL`
  ya enlazaba sin ella).
- `ftest.py`: `t_fetch` en FULL + IMPACTO; `arranca_qemu` ahora
  levanta también `tls_stub.py 18443` (`stub_tls=`, activo si
  `t_fetch` está en la selección). `http_stub` como antes.
- FULL: 694 + 10 = 704 pruebas, 0 fallos (con `t_ns` 24/24).

## 5. Siguiente (Hito 3)

`netsurf_f32` mínimo: ventana `ugfx` + `nsfetch` + layout de
texto sobre documentos locales. Requiere libdom + libnsfb o
plotters propios; la decisión CSS del Hito 1 se reevalúa con
uso real (select sobre DOM).
