# Roadmap del kernel v2.0

## Etapa 1 (esta): COMPLETADA salvo documentación final
E1 auditoría/arquitectura · E2 memoria · E3 planificador observable ·
E4 tabla syscalls · E5 IPC · E6 ktimers/drivers · E7 VFS · E8 seguridad/WP.

## Etapa 2 (propuesta, escritorio real)
1. Servidor gráfico: dueño único, superficies por proceso, composición.
2. Gestor de ventanas + administrador de archivos (sobre VFS/IPC).
3. Eventos de entrada por IPC (teclado; ratón PS/2: driver nuevo).
4. Backends VFS persistentes (diseño listo; sin driver ATA aún).
5. Endurecer G: drenaje explícito o reclamación en espera (ver E5 de etapa 1).

## Deuda / pendientes conocidos
- `t2_gfx` 11/12 (política de propiedad; app anterior a la política).
- Doble búfer efectivo (dorso sin asignar si destino se deniega pre-propiedad).
- `proc_termina`/`proc_libera` sin uso (reservadas); `k_eax/ecx/edx/k_ret`
  sin uso; `sc_count` sin lectura (telemetría futura).
- MAXPROC=8 por diseño (margen en PD/PT/pilas/marcos para 10+ si se requiere).
- Sin SMP, sin COW/demanda/swap, sin POSIX (decisiones, no carencias).
