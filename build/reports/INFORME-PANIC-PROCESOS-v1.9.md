# FacilOS32 v1.9 — Informe de panic de procesos

## Causa raíz

El cambio de contexto de kernel (`entra_kernel`) no actualizaba
`TSS.esp0` al reanudar un proceso bloqueado en modo kernel.

Cuando un proceso dormía o esperaba, el planificador cedía a otro
proceso, pero la TSS seguía apuntando a la pila del proceso anterior.
Las IRQ posteriores se ejecutaban sobre la pila del proceso bloqueado,
corrompiendo `esp_guard` y provocando PANICO al reanudar.

## Corrección

- `entra_kernel` ahora llama `tss_pon_esp0(p->esp_k)` antes de restaurar
  el contexto.
- Se mantuvo el resto del diseño.

## Archivos modificados

- src/kernel32/proc.c

## Pruebas

- Crear 1 proceso: SUPERADA.
- Crear 2 procesos: SUPERADA.
- Crear 5 procesos: SUPERADA.
- reloj individual: SUPERADA.
- Shell tras pruebas: SUPERADA.
- No PANICO ni excepciones inesperadas.

## Conclusión

El panic por creación repetida de procesos queda resuelto. v1.9 sigue
requiriendo validación adicional antes de declararse estable.