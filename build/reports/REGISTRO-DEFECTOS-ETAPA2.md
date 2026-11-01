# REGISTRO-DEFECTOS-ETAPA2 (Fase 15)

## P0 (corrupción grave / pérdida de control)
1. **EAX/ECX/EDX de usuario perdidos en preempción** (proc.c/idt32.asm).
   Reproducción: `equidad32` → `quemador pid=32` (=0x20 residual).
   Causa: `entra_usuario` no los restauraba. Fix: `salta_usuario` (7 GPR).
   Prueba: equidad 3/3 ×3 + regresión total. Cerrado.

## P1 (aislamiento / memoria / integridad)
2. **sbrk sin anotar (fuga heredada)** (exec.c caso 7, condición invertida).
   Reproducción: `mem32` → `FUGA presentes=39 anotados=38`. Fix: `!`.
   Prueba: cero FUGA en ~500 destrucciones. Cerrado.
3. **Tabla IPC a NULL por inicializadores duplicados** (exec.c).
   Reproducción: `ipc32` 7 fallos + breakpoint GDB que jamás salta. Fix:
   quitar duplicados. Prueba: 111/0. Cerrado (defecto de desarrollo,
   nunca en release).

## P2 (fugas / bloqueos / recursos / funcional)
4. **Menú fuera del catálogo (tope 24)** (cdcat.h + orden de build).
   Reproducción: ISO etapa2 sin menú (`-100`). Fix: tope 32 (copia RAM
   contigua verificada). Prueba: 28/28 + menú arranca. Cerrado.
5. **Deadlock de test P4-IPC** (ráfaga sin ceder + ecos rancios). Fix en el
   test (drenaje + `u_ceder`). Cerrado (no era kernel).

## P3 (menores / robustez / diagnóstico)
6. **Ventanas de despertar** (tecla/espera): preset-BLOQ + no-pisar-LISTO +
   recheck. Sin reproducción (teórico, ~ns); mitigado. Abierto→mitigado.
7. **VMA merge solo-adyacente** (falso FUGA): unión por solape. Cerrado.
8. **Publicación a medio inicializar** (riesgo C): NUEVO→LISTO. Cerrado.

## P4 (opcionales)
9. Telemetría `sc_count` sin lectura; `proc_termina/libera` sin uso;
   drenajes por tiempo en G/H; heap-k sin carga real. Documentados.

## Sin reproducir (no se corrigen, se vigilan)
- 1 aborto aislado de `t1_lib` (etapa 1, sin mecanismo; 10+ verdes después).
