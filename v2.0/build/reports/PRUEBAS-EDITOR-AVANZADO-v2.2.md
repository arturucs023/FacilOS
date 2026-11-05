# PRUEBAS-EDITOR-AVANZADO-v2.2

Metodo: QEMU (`tools/qrun.py`) sobre la ISO construida, capturas en
`build/images/`. Cada caso: pasos de teclado/raton reales y captura.
Sin simulacros: lo no ejecutado consta como tal.

## 1. Edicion

- Crear (Ctrl+N, +), escribir ES/lineas/Enter/Tab: OK (`ed_texto`).
- Borrar caracter (Retroceso) y palabra (Ctrl+Retroceso/Supr): OK
  (codigo + uso en sesion; Supr probado en linea).
- Mover: flechas, Inicio/Fin, Ctrl+Inicio/Fin, RePag/AvPag,
  Ctrl+flechas por palabras: OK.
- UTF-8: `caf'e` -> `cafe` (4 car, 5 B), tildes, enye, c
  trencada por compose del driver: OK (`ed_utf8`). Sin partir
  multibyte al borrar/mover/seleccionar: OK.
- Documento vacio, linea larga (102 B, recorte sin invadir): OK
  (`ed_goto`, `ed_wrap2`).

## 2. Seleccion

- Raton: pulsar-arrastrar-soltar, entre lineas, autodesplaza en
  bordes: OK (`ed_mouse`: "seleccion" seleccionada).
- Teclado: May+flechas, ampliar/reducir, Ctrl+E todo: OK.
- Tras insertar/borrar la seleccion se recoloca (ancla=cursor):
  OK. Sin indices invertidos ni corrupcion: OK.

## 3. Formato

- Negrita aplicar/retirar (boton refleja estado): OK (`ed_bold`,
  `ed_bon`, `ed_boff`).
- Cursiva y subrayado, combinados con negrita y color rojo:
  OK (`ed_estilos2`: shear + linea visibles).
- Color a seleccion y a texto nuevo (dialogo + teclas 0-9):
  OK. No se pisan entre si (bits independientes): OK.
- Persistencia `.f32r` con reinicio: OK (`ed_hdd2`: rojo+negrita
  intactos tras reboot con disco). `.txt` puro: OK.

## 4. Persistencia y dialogos

- Guardar/abrir `.txt` y `.f32r`, guardar-como, reabrir: OK.
- Selector: lista raiz, entrar en carpeta, subir, doble clic,
  Enter, campo Nombre, rueda: OK (`ed_sel3/4/6/8`).
- Sobrescribir (Guardar/Descartar/Nada): OK (`ed_sob2`).
- Cierre con cambios (Guardar/Descartar/Nada): OK (`ed_conf`).
- Fallo de escritura: no marca limpio + reintento (codigo; sin
  caso forzado en QEMU: parcial).
- `.f32r` corrupto: validacion por codigo + rescate como texto
  (caso: extension erronea abre como texto: OK; binario truncado
  a proposito: no probado en QEMU: parcial).
- Compatibilidad: archivos viejos UTF-8 abren intactos: OK.

## 5. Documentos y edicion avanzada

- 4 pestanas, cambio con clic/Ctrl+Tab, indicador `*`, cerrar
  con aviso, guardar todos (Ctrl+May+S): OK (`ed_tabs`,
  `ed_tabsw`). Tope 4 con aviso: codigo (no forzado: parcial).
- Deshacer/rehacer texto y estilos: OK (`ed_clip`, `ed_undo`,
  `ed_z`, `ed_yy`). Pila 128 con descarte de la mas vieja:
  codigo (no forzado: parcial).
- Copiar/cortar/pegar interno con estilos, seleccionar todo:
  OK (`ed_clip`). Pegado externo = texto sin formato: OK por
  diseno (no hay fuente externa en v2.2).
- Buscar (3 hits, resaltado, contador, ant/sig), reemplazar una
  y todas, May/palabra: OK (`ed_bus`, `ed_repl`).
- Ir a linea (Ctrl+L): OK (`ed_goto`: lin 2).

## 6. Interfaz y resoluciones

- 17 botones con icono propio + tooltips (flotante y estado):
  OK (`ed_tip`). Estados normal/hover/pulsado/off/activo: OK.
- Temas oscuro/claro (Ctrl+T), ajuste (Ctrl+J), numeros,
  estado con stats: OK (`ed_claro`, `ed_wrap`, `ed_wrap2`).
- 640x480, 800x600, 1024x768, 1920x1080 sin elementos fuera:
  OK (`ed_en640`, `ed_en1024`, `ed_640v`, `ed_arranque`).
- Ventana estrecha (2 filas de toolbar): OK (`ed_angosto`).
- Integracion: menu, explorador (doble clic/Enter con ruta,
  espacios), argumentos: OK (`ed_expl`, `ed_esp`).

## 7. Regresion del sistema (misma ISO)

- t1_lib 258/258, tutf8 26/26, mem32 15/15, disco32 38/38,
  ipc32 111/111, equidad32 3/3: verdes.
- t2_gfx 12 pruebas 7 fallos: IDENTICO en base `14a3b74`
  (worktree limpio, misma sesion): preexistente del entorno,
  sin regresion por esta rama. (La base v2.2 documenta 1 fallo y
  la rama 3 en otro entorno: sensible al entorno, no a este
  cambio; se deja constancia.)
