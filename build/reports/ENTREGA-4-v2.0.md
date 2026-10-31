# v2.0 Etapa 1 — ENTREGA 4: tabla central de syscalls + args inválidos

Estado: COMPLETADA (cubre Fase 8 y parte de Fase 12).

## 1. Problema y causa

El despacho era un `switch` monolítico de ~500 líneas en `exec.c`: ampliarlo
(IPC 34/35) o auditarlo exigía tocar una estructura frágil sin punto central
de validación ni contadores (Fase 8).

## 2. Solución (diseño D6)

- Conversión mecánica `case N` → `sc_NN(regs_t*, int)` (38 handlers, cuerpos
  intactos; `break` de salida → `return` dirigido por el compilador en 39
  sitios verificados uno a uno; `break` de bucles intactos).
- Tabla `sc_tabla[40]` + `sc_nombre[40]` + puerta central (nº inválido o
  reservado → -1) + `sc_count[40]`.
- Todos los handlers con `optimize("O0")` (mitigación Fase 6 intacta).
- ABI 0–33 idéntico; 34/35 reservadas documentadas; 36/37 ya migradas.
- App nueva `sucio32`: 32 comprobaciones de argumentos inválidos
  (punteros, fds, pid, syscall desconocida/reservada, sbrk enorme) con
  integridad del núcleo al final.

## 3. Pruebas (ejecución)

- `sucio32`: 32/32 SUPERADA (`v20_sucio.log`).
- Regresión en el binario refactorizado, misma sesión: batería 8/8,
  `mem32` 15/15, `equidad32` 3/3, `t1_lib` 258/0 SUPERADA (`v20_bat3.log`).
- Sin PANIC, sin FUGA, sin EXCEPCION inesperada.
