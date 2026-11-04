# Terminal: cursor visible y desplazamiento (v2.2)

## Fallos
- Las flechas izquierda/derecha movian `pos` pero el cursor se
  pintaba siempre al final: parecian no hacer nada.
- No habia forma de subir/bajar por la salida (RePag/AvPag no
  hacian nada) y la rueda no existia en ningun sitio.

## Arreglo
- `terminal.c`: la ventana sigue al cursor de edicion y el bloque
  se dibuja en `pos`; `desplaza` con RePag/AvPag y rueda
  (3 lineas por paso); `ayuda` anuncia las teclas.
- Nucleo (`ps2.c/h`): activa la rueda (tasas 200,100,80 + modelo;
  si no la hay sigue en 3 B) y la sirve en la syscall 38
  ([x,y,bot,rueda]).
- Servidor: acumula el giro del lote y lo manda en `MOU x y bot
  giro` (compatible: sin giro vale 0); si solo hay giro tambien
  avisa para que la app lo vea.

## Pruebas (QEMU, capturas)
- `ayuda` + izquierda x2 + `zz` -> `ayuzz|da` con el bloque en medio.
- `ayuda` (salida larga) + RePag: se ven las primeras lineas;
  + AvPag: vuelve al fondo con el cursor.
- Rueda +5: sube 15 lineas; rueda -5: vuelve abajo.
- Arriba/abajo (historial de comandos) intactos.
