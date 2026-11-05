# F32 Lang v0.1 — Lexer (`f32.c`, parte 1)

Tokenización por líneas (máx. 4096 líneas, 32768 B; CRLF
tolerado como terminador). Cada línea guarda inicio,
longitud e indentación (múltiplos de 4 espacios; tab = error
F32-022).

Tokens (`MAXTOK` 8192): `T_RES` (18 palabras, insensible a
mayúsculas al comparar pero se exige minúscula por estilo en
el error F32-011), `T_ID` (ASCII `[A-Za-z_][A-Za-z0-9_]*;
UTF-8 fuera de cadenas = F32-010), `T_ENT` (acumulado con
detección de desbordamiento 32 bit, F32-031; letra pegada a
número = F32-010), `T_CAD` (una línea, UTF-8 validado con
`ut_valido`, F32-030/032), `T_OP` (`+ - * / % = == != < <= >
> ( ) ,`; `!` solo no existe → F32-010).

Ignora blancos y comentarios `#` (resto de línea; línea solo
comentario = vacía y no cuenta para estructura). Cada token
conserva su línea para los errores F32-0NN.
