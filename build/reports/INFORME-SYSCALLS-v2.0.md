# Syscalls v2.0 (ABI INT 0x80)

Tabla central `sc_tabla[40]` + nombres + puerta central (desconocida/
reservada → -1) + contadores. Handlers con `optimize("O0")`. Marco por
proceso (`sc_regs`). ABI 0–33 CONGELADO (compatible v1.9).

| Nº | Nombre | Args (ebx,ecx,edx[/esi,/edi]) | Ret | Bloquea |
|---|---|---|---|---|
| 0 | salir | rc | — | no (muere) |
| 1 | imprimir | ptr,len | 0/-1 | no |
| 2 | ticks | — | ticks | no |
| 3 | abrir | ruta | fd/-1 | no |
| 4 | leer | fd,buf,n | leídos/-1 | no |
| 5 | escribir | fd,buf,n | escritos/-1 | no |
| 6 | cerrar | fd | 0/-1 | no |
| 7 | memoria | n | base/-1 | no |
| 8 | leer_tecla | — | tecla | SÍ |
| 9 | crear | ruta | fd/-1 | no |
| 10 | pixel | x,y,color | 0/-1 | no |
| 11 | linea | x1,y1,x2,(y2,color) | 0/-1 | no |
| 12 | rectangulo | x,y,w,(h,color) | 0/-1 | no |
| 13 | texto | x,y,ptr,(len,color) | 0/-1 | no |
| 14 | limpiar_pantalla | color | 0/-1 | no |
| 15 | buscar | fd,off,base | pos/-1 | no |
| 16 | borrar | ruta | 0/-1 | no |
| 17 | dormir | ms | 0 | SÍ |
| 18 | destino | d | 0/-1 | no |
| 19 | presentar | x,y,w,(h) | 0/-1 | no |
| 20 | sprite | x,y,spr | 0/-1 | no |
| 21 | sprite_t | x,y,spr,(clave) | 0/-1 | no |
| 22 | blit | op[6] | 0/-1 | no |
| 23 | leer_pixel | x,y | color | no |
| 24 | truncar | fd,tam | 0/-1 | no |
| 25 | ejecutar | ruta,args | rc/<0 | SÍ |
| 26 | terminal | — | 0 | (shell anidada) |
| 27 | hay_tecla | — | 0/1 | no |
| 28 | crear_proceso | nombre,args | pid/<0 | no |
| 29 | mi_pid | — | pid | no |
| 30 | listar | buf,max | n | no |
| 31 | esperar | pid | rc/-1 | SÍ |
| 32 | ceder | — | 0 | (cede) |
| 33 | info_memoria | buf5 | 0/-1 | no |
| 34 | ipc_enviar | dst,len,ptr | 0/-1/-2 | no |
| 35 | ipc_recibir | buf,max,&emisor | len/-1 | SÍ |
| 36 | info_memoria2 | buf8 | 0/-1 | no |
| 37 | proc_stat | pid,buf5 | 0/-1 | no |
| 38–39 | reservadas | — | -1 | — |

Validación: punteros con `pag_valida_usuario` (escritura pedida si toca),
tamaños acotados, fds/pid/base verificados. Gfx exige propiedad (menos
`leer_pixel` y toma en `texto`). Pruebas: `sucio32` 32/32 + regresión total.
