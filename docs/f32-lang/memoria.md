# F32 Lang v0.1 — Memoria

Sin estáticos grandes (irían al plano del binario: se vio
440 KB con tablas estáticas; el límite v3 es 128K): todo lo
grande vive en el heap de 1.5 MB vía `u_malloc` comprobado:

- Fuente 32K + 5 índices de línea (5×16K) + tokens
  (8192×16 = 128K) + símbolos + funciones + marcos (64×~2.5K
  ≈ 164K) + ámbito semántico (~9K) ≈ 420K peor caso.
- Arena de AST/cadenas con tope 384K (F32-090 al superar).
- Peor caso teórico ≈ 840K < 1.5 MB; típico («hola») < 200K.

Regla de oro Ring3 detectada y documentada (ver
`hallazgo-kernel.md`): en este sistema, abrir un fichero
(syscalls 3/9) reiniciaba el `brk`, invalidando `malloc`
previos. El intérprete abre primero y reserva después;
el núcleo se corrigió para que el heap sea monótono.
