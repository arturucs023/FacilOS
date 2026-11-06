#include "ugfx.h"
#include "utext.h"
#include "ulog.h"

void __main(void) {
}

/* Terminal FacilOS32 v2.2: lenguaje propio en castellano, registro
 * central de comandos, parser con comillas y parametros nombre=valor,
 * autocompletado visual con TAB, ayuda/manuales, historial editable,
 * editor de linea UTF-8 y registros en castellano. Todo Ring3. */

#define NLIN 160
#define NCOL 96
#define NENTB 512
#define NHIST 20
#define NPOP 32
#define NTOK 12
#define NPAR 8

static char pant[NLIN][NCOL];
static int nlin = 0;
/* Lineas que el usuario subio desde el fondo (RePag/AvPag). */
static int desplaza = 0;
static char ent[NENTB];
static int nent = 0, pos = 0;
static char hist[NHIST][NENTB];
static int nhist = 0, hh = 0;
static char borrador[NENTB];
static int hay_borrador = 0;

/* Directorio propio (indice + pila para mostrar_ruta). */
static int cwd_idx = 0;
static char pila[8][32];
static int prof = 0;

/* Selector TAB. */
static int pop_abierto = 0, pop_n = 0, pop_sel = 0, pop_ini = 0;
static char pop_items[NPOP][64];
static char pop_titulo[32];
static char list_dirpath[128];

/* ---- salida ---- */

static void pon(const char *s) {
    desplaza = 0;   /* lo nuevo trae la vista al fondo */
    while (*s && nlin < NLIN) {
        int k = 0;
        while (s[0] && s[0] != '\n') {
            int av = 0;
            ut_dec(s, 4, &av);
            if (av <= 0) av = 1;
            if (k + av >= NCOL) break;   /* sin partir multibyte */
            {
                int j;
                for (j = 0; j < av; j++) pant[nlin][k++] = s[j];
            }
            s += av;
        }
        pant[nlin][k] = 0;
        nlin++;
        if (s[0] == '\n') s++;
        else if (s[0]) {
            /* linea larguisima: sigue en la siguiente fila */
        }
    }
    if (nlin >= NLIN) {
        int d = nlin - NLIN + 1, j, k;
        for (j = 0; j < NLIN - 1; j++) {
            for (k = 0; k < NCOL; k++) pant[j][k] = pant[j + d][k];
        }
        nlin = NLIN - 1;
    }
}

static void pon_reg(int nivel, int cod, const char *msg) {
    char b[192];
    ulog_form(b, sizeof(b), nivel, "TERMINAL", cod, msg);
    pon(b);
}

static void perr(int cod, const char *msg) {
    pon_reg(ULOG_ERROR, cod, msg);
}

static void dos2(char *b, int *t, unsigned v) {
    b[*t] = (char)('0' + (v / 10) % 10);
    b[*t + 1] = (char)('0' + v % 10);
    *t += 2;
}

/* ---- FS: resolver propio (sin tocar el dir global del nucleo) ---- */

static int lista_hijo(int dir, int n, udir_t *d) {
    return u_dir_hijo(dir, n, d);
}

/* Resuelve ruta a (padre_idx, base). 0 ok. Acepta /, ., .. y comillas
   ya quitadas. */
static int resuelve(const char *ruta, int *pdir, char *base) {
    int cur, i = 0, j, k;
    int st[8];
    int sp = 0;
    char comp[32];
    if (!ruta || !ruta[0]) return -1;
    if (ruta[0] == '/') {
        cur = 0;
        i = 1;
    } else {
        /* parte del cwd: re-resuelve la pila a indices frescos */
        char loc[8][32];
        int locn = 0;
        for (k = 0; k < prof && k < 8; k++) {
            int q = 0;
            while (pila[k][q] && q < 31) {
                loc[locn][q] = pila[k][q];
                q++;
            }
            loc[locn][q] = 0;
            locn++;
        }
        cur = 0;
        for (k = 0; k < locn; k++) {
            int n2 = 0, fnd = -1;
            udir_t d2;
            while (lista_hijo(cur, n2, &d2) >= 0) {
                if (d2.tipo == 1 && !u_strcmp(d2.nombre, loc[k])) {
                    fnd = d2.idx;
                    break;
                }
                n2++;
                if (n2 > 200) break;
            }
            if (fnd < 0) return -1;
            if (sp < 8) st[sp++] = fnd;
            cur = fnd;
        }
    }
    for (;;) {
        while (ruta[i] == '/') i++;
        if (!ruta[i]) {
            *pdir = cur;
            base[0] = 0;
            return 0;
        }
        j = 0;
        while (ruta[i] && ruta[i] != '/' && j < 31)
            comp[j++] = ruta[i++];
        comp[j] = 0;
        if (!comp[0]) continue;
        if (!u_strcmp(comp, ".")) continue;
        if (!u_strcmp(comp, "..")) {
            if (sp > 0) {
                sp--;
                cur = sp ? st[sp - 1] : 0;
            } else {
                cur = 0;
            }
            continue;
        }
        {
            int t = i;
            while (ruta[t] == '/') t++;
            if (!ruta[t]) {
                /* ultimo: es la base en cur */
                int q = 0;
                while (comp[q] && q < 31) {
                    base[q] = comp[q];
                    q++;
                }
                base[q] = 0;
                *pdir = cur;
                return 0;
            }
        }
        /* intermedio: debe ser carpeta */
        {
            int n2 = 0, fnd = -1;
            udir_t d2;
            while (lista_hijo(cur, n2, &d2) >= 0) {
                if (d2.tipo == 1 && !u_strcmp(d2.nombre, comp)) {
                    fnd = d2.idx;
                    break;
                }
                n2++;
                if (n2 > 200) break;
            }
            if (fnd < 0) return -1;
            if (sp < 8) st[sp++] = fnd;
            cur = fnd;
        }
    }
}

/* Busca nombre exacto en dir; devuelve idx y tipo. */
static int busca_en(int dir, const char *nom, int *tipo) {
    int n = 0;
    udir_t d;
    while (lista_hijo(dir, n, &d) >= 0) {
        if (!u_strcmp(d.nombre, nom)) {
            if (tipo) *tipo = d.tipo;
            return d.idx;
        }
        n++;
        if (n > 200) break;
    }
    return -1;
}

/* ---- registro central de comandos (v2.2) ----
 * Una sola definicion alimenta ayuda, manual, autocompletado y
 * validacion. La ejecucion valida de nuevo por su cuenta. */

typedef struct {
    const char *nombre;   /* nombre del parametro */
    const char *tipo;     /* "sino" | "texto" | "formato" | "orden" */
    const char *defecto;  /* valor por defecto ("" = sin defecto) */
    const char *valores;  /* admitidos separados por | ("" = libre) */
} par_t;

typedef struct {
    const char *nombre;
    const char *categoria;
    const char *breve;
    const char *detalle;
    const char *uso;
    const char *ejemplo;
    int min_pos;
    int max_pos;
    const par_t *pars;
    int npars;
} cmd_t;

static const par_t P_PADRES[] = {
    { "crear_padres", "sino", "si", "si|no" },
};
static const par_t P_LISTA[] = {
    { "formato", "formato", "simple", "simple|detallado" },
    { "orden", "orden", "nombre", "nombre|tipo" },
};
static const par_t P_COPIA[] = {
    { "sobrescribir", "sino", "no", "si|no" },
    { "verificar", "sino", "si", "si|no" },
};

static const cmd_t CMDS[] = {
    { "ayuda", "documentacion",
      "Muestra los comandos disponibles.",
      "Lista todos los comandos agrupados por categorias. Con un nombre muestra su ayuda breve.",
      "ayuda [comando]", "ayuda manual", 0, 1, 0, 0 },
    { "manual", "documentacion",
      "Manual completo de un comando.",
      "Muestra descripcion, sintaxis, opciones, valores, ejemplos y errores de un comando.",
      "manual <comando>", "manual crear_carpeta", 1, 1, 0, 0 },
    { "crear_carpeta", "carpetas",
      "Crea una carpeta nueva.",
      "Crea una carpeta con el nombre indicado. Con crear_padres=si crea tambien las intermedias que falten.",
      "crear_carpeta \"nombre\" [crear_padres=si|no]",
      "crear_carpeta \"Documentos\"", 1, 1, P_PADRES, 1 },
    { "crear_archivo", "archivos",
      "Crea un archivo vacio.",
      "Crea un archivo vacio con el nombre indicado.",
      "crear_archivo \"nombre\"", "crear_archivo \"notas.txt\"", 1, 1, 0, 0 },
    { "eliminar_archivo", "archivos",
      "Elimina un archivo.",
      "Elimina el archivo indicado. Pide el nombre exacto; no hay papelera.",
      "eliminar_archivo \"nombre\"", "eliminar_archivo \"viejo.txt\"",
      1, 1, 0, 0 },
    { "eliminar_carpeta", "carpetas",
      "Elimina una carpeta vacia.",
      "Elimina la carpeta indicada solo si esta vacia.",
      "eliminar_carpeta \"nombre\"", "eliminar_carpeta \"Vacia\"",
      1, 1, 0, 0 },
    { "listar_archivos", "carpetas",
      "Lista archivos y carpetas.",
      "Muestra el contenido de una carpeta (la actual si no se indica ruta).",
      "listar_archivos [ruta] [formato=simple|detallado] [orden=nombre|tipo]",
      "listar_archivos formato=detallado", 0, 1, P_LISTA, 2 },
    { "entrar_carpeta", "carpetas",
      "Entra en una carpeta.",
      "Cambia la carpeta actual. Acepta rutas, / y ..",
      "entrar_carpeta \"ruta\"", "entrar_carpeta \"Documentos\"", 1, 1, 0, 0 },
    { "mostrar_ruta", "carpetas",
      "Muestra la carpeta actual.",
      "Escribe la ruta de la carpeta donde estas.",
      "mostrar_ruta", "mostrar_ruta", 0, 0, 0, 0 },
    { "mostrar_contenido", "archivos",
      "Muestra el contenido de un archivo.",
      "Escribe el contenido del archivo indicado (texto).",
      "mostrar_contenido \"nombre\"", "mostrar_contenido \"notas.txt\"",
      1, 1, 0, 0 },
    { "copiar_archivo", "archivos",
      "Copia un archivo.",
      "Copia el origen al destino. Si el destino es una carpeta, conserva el nombre.",
      "copiar_archivo \"origen\" \"destino\" [sobrescribir=si|no] [verificar=si|no]",
      "copiar_archivo \"a.txt\" \"copia.txt\"", 2, 2, P_COPIA, 2 },
    { "mover_archivo", "archivos",
      "Mueve un archivo o carpeta.",
      "Cambia de sitio o de nombre. No sobrescribe destinos existentes.",
      "mover_archivo \"origen\" \"destino\"",
      "mover_archivo \"a.txt\" \"Documentos/b.txt\"", 2, 2, 0, 0 },
    { "cambiar_nombre", "archivos",
      "Cambia el nombre.",
      "Renombra un archivo o carpeta dentro de su carpeta.",
      "cambiar_nombre \"actual\" \"nuevo\"",
      "cambiar_nombre \"a.txt\" \"b.txt\"", 2, 2, 0, 0 },
    { "limpiar_pantalla", "sistema",
      "Limpia la terminal.",
      "Borra todo el texto visible.",
      "limpiar_pantalla", "limpiar_pantalla", 0, 0, 0, 0 },
    { "informacion_sistema", "sistema",
      "Datos del sistema.",
      "Muestra memoria, procesos y modo de video.",
      "informacion_sistema", "informacion_sistema", 0, 0, 0, 0 },
    { "mostrar_fecha", "sistema",
      "Muestra la fecha.",
      "Escribe la fecha del reloj del sistema.",
      "mostrar_fecha", "mostrar_fecha", 0, 0, 0, 0 },
    { "mostrar_hora", "sistema",
      "Muestra la hora.",
      "Escribe la hora del reloj del sistema.",
      "mostrar_hora", "mostrar_hora", 0, 0, 0, 0 },
    { "teclado", "sistema",
      "Distribucion de teclado.",
      "Muestra o cambia la distribucion (es, ca, us).",
      "teclado [es|ca|us]", "teclado es", 0, 1, 0, 0 },
    { "historial", "sistema",
      "Comandos anteriores.",
      "Lista los comandos escritos en esta sesion.",
      "historial", "historial", 0, 0, 0, 0 },
    { "eco", "sistema",
      "Repite un texto.",
      "Escribe los argumentos tal cual.",
      "eco \"texto\"", "eco hola mundo", 0, 4, 0, 0 },
    { "ejecutar", "sistema",
      "Ejecuta un programa o un script .f32.",
      "Lanza un programa F32X con sus argumentos. Si el nombre termina en .f32 (o existe con esa extension), lo ejecuta con el interprete F32 Lang.",
      "ejecutar <programa> [args]", "ejecutar hola.f32", 1, 4, 0, 0 },
    { "red", "sistema",
      "Informacion y administracion general de la red.",
      "Muestra el estado de la red propia (adaptador, MAC, contadores). Sin adaptador indica arranque sin red.",
      "red [estado]", "red", 0, 1, 0, 0 },
    { "mostrar_interfaz", "sistema",
      "Informacion del adaptador de red.",
      "Muestra adaptador, estado, direccion MAC y estadisticas de tramas.",
      "mostrar_interfaz", "mostrar_interfaz", 0, 0, 0, 0 },
    { "configurar_ip", "sistema",
      "Configuracion manual de IPv4.",
      "Muestra o fija IP, mascara, puerta de enlace y DNS.",
      "configurar_ip [ip | mascara|puerta|dns <ip>]", "configurar_ip mascara 255.255.255.0", 0, 2, 0, 0 },
    { "mostrar_rutas", "sistema",
      "Consulta la tabla de rutas.",
      "Muestra destino, mascara y puerta de cada ruta.",
      "mostrar_rutas", "mostrar_rutas", 0, 0, 0, 0 },
    { "comprovar_conexion", "sistema",
      "Comprueba conectividad mediante eco.",
      "Envia ecos y muestra respuestas (seq, ttl, tiempo).",
      "comprovar_conexion [-c N] <ip>", "comprovar_conexion -c 4 10.0.2.2", 1, 3, 0, 0 },
    { "configurar_dhcp", "sistema",
      "Configuracion automatica por DHCP.",
      "Obtiene IP, mascara, puerta y DNS del servidor (renovar y liberar disponibles).",
      "configurar_dhcp [renovar|liberar]", "configurar_dhcp", 0, 1, 0, 0 },
    { "salir", "sistema",
      "Cierra la terminal.",
      "Cierra esta ventana de terminal.",
      "salir", "salir", 0, 0, 0, 0 },
};
#define NCMDS (int)(sizeof(CMDS) / sizeof(CMDS[0]))

static int busca_cmd(const char *nom) {
    int i;
    for (i = 0; i < NCMDS; i++) {
        if (!u_strcmp(CMDS[i].nombre, nom)) return i;
    }
    return -1;
}

static int sugiere(const char *nom, char dst[4][32]);
static void ejecuta(ug_win_t *v, const char *ln);
static int pop_construye(void);
static void pop_abre_uno(void);
static void pop_aplica(void);
static void op_ruta(const cmd_t *c);
static void pon_ip(const char *tit, unsigned int v);
static int lee_ip(const char *s, unsigned int *v);
static ug_win_t *v_term = 0;   /* ventana actual (para modales) */
static void muestra_sal_fich(const char *ruta);

/* ---- parser (v2.2): comillas, posicionales y nombre=valor ---- */

static char tok_buf[NTOK][128];
static int ntok = 0;
static char par_nom[NPAR][32];
static char par_val[NPAR][96];
static int npar = 0;
static char pos_arg[4][96];
static int npos = 0;

static int es_esp(char c) { return c == ' ' || c == '\t'; }

/* Trocea la linea en tokens (respeta "..."). 0 ok, -1 comillas sin
   cerrar, -2 demasiados. */
static int trocea(const char *ln) {
    int p = 0, t = 0;
    ntok = 0;
    while (ln[p]) {
        int q = 0;
        while (es_esp(ln[p])) p++;
        if (!ln[p]) break;
        if (ntok >= NTOK) return -2;
        t = 0;
        if (ln[p] == '"') {
            p++;
            while (ln[p] && ln[p] != '"' && t < 127)
                tok_buf[ntok][t++] = ln[p++];
            if (ln[p] != '"') return -1;
            p++;
        } else {
            while (ln[p] && !es_esp(ln[p]) && t < 127)
                tok_buf[ntok][t++] = ln[p++];
        }
        tok_buf[ntok][t] = 0;
        ntok++;
    }
    return 0;
}

/* Separa posicionales y parametros. i_cmd = indice del comando (0).
   Devuelve: 0 ok, 1 param sin =, 2 duplicado, 3 excede topes. */
static int separa(int *bad) {
    int i;
    npos = 0;
    npar = 0;
    *bad = 0;
    for (i = 1; i < ntok; i++) {
        int e = 0, j;
        while (tok_buf[i][e] && tok_buf[i][e] != '=') e++;
        if (!tok_buf[i][e]) {
            if (npos >= 4) return 3;
            u_strcpy(pos_arg[npos++], tok_buf[i]);
            continue;
        }
        if (npar >= NPAR) return 3;
        {
            int k = 0;
            while (k < e && k < 31) {
                par_nom[npar][k] = tok_buf[i][k];
                k++;
            }
            par_nom[npar][k] = 0;
            if (!par_nom[npar][0]) {
                *bad = i;
                return 1;
            }
            for (j = 0; j < npar; j++) {
                if (!u_strcmp(par_nom[j], par_nom[npar])) {
                    *bad = i;
                    return 2;
                }
            }
            {
                int v = e + 1, w = 0;
                while (tok_buf[i][v] && w < 95)
                    par_val[npar][w++] = tok_buf[i][v++];
                par_val[npar][w] = 0;
            }
            npar++;
        }
    }
    return 0;
}

/* Valor de parametro o defecto del registro (0 si no existe). */
static const char *param_de(const cmd_t *c, const char *nom, int *dado) {
    int i;
    for (i = 0; i < npar; i++) {
        if (!u_strcmp(par_nom[i], nom)) {
            if (dado) *dado = 1;
            return par_val[i];
        }
    }
    for (i = 0; i < c->npars; i++) {
        if (!u_strcmp(c->pars[i].nombre, nom)) {
            if (dado) *dado = 0;
            return c->pars[i].defecto;
        }
    }
    if (dado) *dado = 0;
    return 0;
}

/* Comprueba que un valor esta en la lista a|b. */
static int valor_ok(const char *lista, const char *v) {
    int i = 0, j;
    char it[32];
    if (!lista || !lista[0]) return 1;
    while (lista[i]) {
        j = 0;
        while (lista[i] && lista[i] != '|' && j < 31)
            it[j++] = lista[i++];
        it[j] = 0;
        if (!u_strcmp(it, v)) return 1;
        if (lista[i] == '|') i++;
    }
    return 0;
}

/* Valida params contra el registro: 0 ok, else indice de parametro
   malo en *mal (+motivo: 1 desconocido, 2 valor). */
static int valida_pars(const cmd_t *c, int *mal, int *motivo) {
    int i, j;
    for (i = 0; i < npar; i++) {
        int hay = 0;
        for (j = 0; j < c->npars; j++) {
            if (!u_strcmp(par_nom[i], c->pars[j].nombre)) {
                hay = 1;
                if (!valor_ok(c->pars[j].valores, par_val[i])) {
                    *mal = i;
                    *motivo = 2;
                    return -1;
                }
                break;
            }
        }
        if (!hay) {
            *mal = i;
            *motivo = 1;
            return -1;
        }
    }
    return 0;
}

static int es_si(const char *v) { return !u_strcmp(v, "si"); }

/* ---- ayuda y manual (del registro) ---- */

static void cmd_ayuda(const char *arg) {
    int i;
    if (arg && arg[0]) {
        int id = busca_cmd(arg);
        if (id < 0) {
            char sug[4][32];
            int n = sugiere(arg, sug), k;
            char b[96];
            int t = 0;
            pon("No existe ningun comando con ese nombre.");
            if (n) {
                pon("Quisiste decir:");
                for (k = 0; k < n; k++) {
                    t = 0;
                    b[t++] = '-';
                    b[t++] = ' ';
                    {
                        int q = 0;
                        while (sug[k][q] && t < 90) b[t++] = sug[k][q++];
                    }
                    b[t] = 0;
                    pon(b);
                }
            }
            return;
        }
        pon(CMDS[id].nombre);
        pon(CMDS[id].breve);
        pon("Uso:");
        pon(CMDS[id].uso);
        return;
    }
    pon("FACILOS32 - AYUDA GENERAL");
    pon("Archivos:");
    for (i = 0; i < NCMDS; i++) {
        if (!u_strcmp(CMDS[i].categoria, "archivos")) {
            char b[96];
            int t = 0, q = 0;
            b[t++] = ' ';
            b[t++] = ' ';
            while (CMDS[i].nombre[q] && t < 90) b[t++] = CMDS[i].nombre[q++];
            b[t] = 0;
            pon(b);
        }
    }
    pon("Carpetas:");
    for (i = 0; i < NCMDS; i++) {
        if (!u_strcmp(CMDS[i].categoria, "carpetas")) {
            char b[96];
            int t = 0, q = 0;
            b[t++] = ' ';
            b[t++] = ' ';
            while (CMDS[i].nombre[q] && t < 90) b[t++] = CMDS[i].nombre[q++];
            b[t] = 0;
            pon(b);
        }
    }
    pon("Sistema:");
    for (i = 0; i < NCMDS; i++) {
        if (!u_strcmp(CMDS[i].categoria, "sistema")) {
            char b[96];
            int t = 0, q = 0;
            b[t++] = ' ';
            b[t++] = ' ';
            while (CMDS[i].nombre[q] && t < 90) b[t++] = CMDS[i].nombre[q++];
            b[t] = 0;
            pon(b);
        }
    }
    pon("Documentaci\xc3\xb3n:");
    for (i = 0; i < NCMDS; i++) {
        if (!u_strcmp(CMDS[i].categoria, "documentacion")) {
            char b[96];
            int t = 0, q = 0;
            b[t++] = ' ';
            b[t++] = ' ';
            while (CMDS[i].nombre[q] && t < 90) b[t++] = CMDS[i].nombre[q++];
            b[t] = 0;
            pon(b);
        }
    }
    pon("Para saber mas: manual nombre_del_comando");
    pon("Teclas: flechas editan, arriba/abajo historial,");
    pon("  repag/avpag y rueda desplazan la pantalla.");
}

static void cmd_manual(const char *arg) {
    int id = busca_cmd(arg), i;
    char b[128];
    int t;
    if (id < 0) {
        char sug[4][32];
        int n = sugiere(arg, sug), k;
        pon("No existe ningun comando con ese nombre.");
        if (n) {
            pon("Quisiste decir:");
            for (k = 0; k < n; k++) {
                t = 0;
                b[t++] = '-';
                b[t++] = ' ';
                {
                    int q = 0;
                    while (sug[k][q] && t < 120) b[t++] = sug[k][q++];
                }
                b[t] = 0;
                pon(b);
            }
        }
        return;
    }
    pon("MANUAL DE FACILOS32");
    pon(CMDS[id].nombre);
    t = 0;
    {
        const char *s = "Categor\xc3" "\xada: ";
        while (*s && t < 120) b[t++] = *s++;
    }
    {
        int q = 0;
        while (CMDS[id].categoria[q] && t < 120)
            b[t++] = CMDS[id].categoria[q++];
    }
    b[t] = 0;
    pon(b);
    pon("Descripci\xc3\xb3n:");
    pon(CMDS[id].detalle);
    pon("Sintaxis:");
    pon(CMDS[id].uso);
    if (CMDS[id].npars) {
        pon("Opciones:");
        for (i = 0; i < CMDS[id].npars; i++) {
            t = 0;
            {
                int q = 0;
                while (CMDS[id].pars[i].nombre[q] && t < 110)
                    b[t++] = CMDS[id].pars[i].nombre[q++];
            }
            {
                const char *s = " (valores: ";
                while (*s && t < 120) b[t++] = *s++;
            }
            {
                int q = 0;
                if (CMDS[id].pars[i].valores[0]) {
                    while (CMDS[id].pars[i].valores[q] && t < 100)
                        b[t++] = CMDS[id].pars[i].valores[q++];
                } else {
                    const char *s = "texto libre";
                    while (*s && t < 100) b[t++] = *s++;
                }
            }
            {
                const char *s = ", por defecto: ";
                while (*s && t < 120) b[t++] = *s++;
            }
            {
                int q = 0;
                while (CMDS[id].pars[i].defecto[q] && t < 120)
                    b[t++] = CMDS[id].pars[i].defecto[q++];
            }
            b[t++] = ')';
            b[t] = 0;
            pon(b);
        }
    } else {
        pon("Sin opciones.");
    }
    pon("Ejemplo:");
    pon(CMDS[id].ejemplo);
}

/* ---- operaciones reales ---- */

/* Construye ruta absoluta: si ya lo es la copia; si no, cuelga de
   la pila (camino actual). */
static void a_absoluta(const char *ruta, char *out) {
    int o = 0, k, j;
    if (ruta[0] == '/') {
        while (ruta[o] && o < 120) {
            out[o] = ruta[o];
            o++;
        }
        out[o] = 0;
        return;
    }
    out[o++] = '/';
    for (k = 0; k < prof && k < 8; k++) {
        j = 0;
        while (pila[k][j] && o < 110) out[o++] = pila[k][j++];
        out[o++] = '/';
    }
    j = 0;
    while (ruta[j] && o < 120) out[o++] = ruta[j++];
    out[o] = 0;
}

/* Ruta absoluta del DIRECTORIO resuelto: entrada cruda + base. */
static void abs_dir_de(const char *entrada, const char *base, char *out) {
    int o = 0;
    a_absoluta(entrada, out);
    if (base[0]) {
        /* quita el ultimo componente */
        int l = u_strlen(out);
        while (l > 1 && out[l - 1] == '/') l--;
        while (l > 0 && out[l - 1] != '/') l--;
        if (l <= 1) {
            out[0] = '/';
            out[1] = 0;
            return;
        }
        out[l - 1] = 0;
    }
    {
        int l = u_strlen(out);
        while (l > 1 && out[l - 1] == '/') l--;
        out[l] = 0;
    }
}

static void op_crear_carpeta(const cmd_t *c) {
    const char *v;
    int padres, dir;
    char base[32];
    if (npos < 1) {
        perr(10, "Falta indicar el nombre de la carpeta.");
        pon("Uso:");
        pon(c->uso);
        pon("Ejemplo:");
        pon(c->ejemplo);
        return;
    }
    v = param_de(c, "crear_padres", 0);
    padres = es_si(v);
    if (padres) {
        /* crea intermedios uno a uno con rutas absolutas */
        char cur[128];
        int i = (pos_arg[0][0] == '/') ? 1 : 0;
        char comp[32];
        if (pos_arg[0][0] == '/') {
            cur[0] = '/';
            cur[1] = 0;
        } else {
            a_absoluta("", cur);
        }
        for (;;) {
            int j = 0, cl;
            while (pos_arg[0][i] == '/') i++;
            if (!pos_arg[0][i]) break;
            while (pos_arg[0][i] && pos_arg[0][i] != '/' && j < 31)
                comp[j++] = pos_arg[0][i++];
            comp[j] = 0;
            if (!u_strcmp(comp, ".")) continue;
            if (!u_strcmp(comp, "..")) {
                perr(11, "crear_padres no sube con ..; indica la ruta.");
                return;
            }
            cl = u_strlen(cur);
            if (cl > 1) {
                cur[cl++] = '/';
                cur[cl] = 0;
            }
            j = 0;
            while (comp[j] && cl < 120) cur[cl++] = comp[j++];
            cur[cl] = 0;
            if (u_dir_crear(cur)) {
                int dd;
                char bs[32];
                if (resuelve(cur, &dd, bs) || bs[0]) {
                    perr(12, "No se pudo crear una intermedia.");
                    return;
                }
            }
        }
        pon("Carpeta creada.");
        return;
    }
    if (resuelve(pos_arg[0], &dir, base)) {
        perr(14, "Ruta no valida.");
        return;
    }
    if (!base[0]) {
        perr(10, "Falta indicar el nombre de la carpeta.");
        return;
    }
    {
        char ruta[128];
        int r, t2 = 0;
        int idx = busca_en(dir, base, &t2);
        if (idx >= 0) {
            perr(15, "Ya existe con ese nombre.");
            return;
        }
        a_absoluta(pos_arg[0], ruta);
        r = u_dir_crear(ruta);
        if (r) perr(17, "No se pudo crear (nombre o disco lleno).");
        else pon("Carpeta creada.");
    }
}

static void op_crear_archivo(const cmd_t *c) {
    char ruta[128];
    int dir, t2 = 0;
    char base[32];
    (void)c;
    if (npos < 1) {
        perr(10, "Falta indicar el nombre del archivo.");
        return;
    }
    if (resuelve(pos_arg[0], &dir, base) || !base[0]) {
        perr(14, "Ruta no valida.");
        return;
    }
    if (busca_en(dir, base, &t2) >= 0) {
        perr(15, "Ya existe con ese nombre.");
        return;
    }
    a_absoluta(pos_arg[0], ruta);
    {
        UFILE *f = u_fopen(ruta, "w");
        if (!f) perr(17, "No se pudo crear (nombre o disco lleno).");
        else {
            u_fclose(f);
            pon("Archivo creado.");
        }
    }
}

static void op_eliminar(const cmd_t *c, int quiere_dir) {
    int dir, t2 = 0, idx;
    char base[32];
    char ruta[128];
    (void)c;
    if (npos < 1) {
        perr(10, "Falta indicar el nombre.");
        return;
    }
    if (resuelve(pos_arg[0], &dir, base) || !base[0]) {
        perr(14, "Ruta no valida.");
        return;
    }
    idx = busca_en(dir, base, &t2);
    if (idx < 0) {
        perr(20, "No existe con ese nombre.");
        return;
    }
    if (quiere_dir && t2 != 1) {
        perr(21, "No es una carpeta (es un archivo).");
        return;
    }
    if (!quiere_dir && t2 != 2) {
        perr(22, "No es un archivo (es una carpeta).");
        return;
    }
    a_absoluta(pos_arg[0], ruta);
    {
        int r = u_remove(ruta);
        if (r == -2) perr(23, "La carpeta no esta vacia.");
        else if (r) perr(24, "No se pudo eliminar.");
        else pon(quiere_dir ? "Carpeta eliminada." : "Archivo eliminado.");
    }
}

static int vuelca_lista(int dir, char nombres[64][33], int tipos[64]) {
    int n = 0, k = 0, nn = 0;
    udir_t d;
    while (nn < 64 && lista_hijo(dir, k, &d) >= 0) {
        int q = 0;
        while (d.nombre[q] && q < 32) {
            nombres[nn][q] = d.nombre[q];
            q++;
        }
        nombres[nn][q] = 0;
        tipos[nn] = d.tipo;
        nn++;
        k++;
        if (k > 200) break;
    }
    return nn;
}

static void op_listar(const cmd_t *c) {
    const char *fmt, *ord;
    int dir = cwd_idx;
    char nombres[64][33];
    int tipos[64];
    int nn, i, j;
    fmt = param_de(c, "formato", 0);
    ord = param_de(c, "orden", 0);
    if (npos >= 1) {
        char base[32];
        int dd, t2 = 0, idx;
        if (resuelve(pos_arg[0], &dd, base)) {
            perr(14, "Ruta no valida.");
            return;
        }
        if (!base[0]) {
            dir = dd;
        } else {
            idx = busca_en(dd, base, &t2);
            if (idx < 0 || t2 != 1) {
                perr(20, "No existe esa carpeta.");
                return;
            }
            dir = idx;
        }
    }
    nn = vuelca_lista(dir, nombres, tipos);
    /* camino absoluto del dir listado (para tamanos reales) */
    if (npos >= 1) {
        char bb[32];
        int ddi;
        if (!resuelve(pos_arg[0], &ddi, bb)) abs_dir_de(pos_arg[0], bb, list_dirpath);
        else {
            list_dirpath[0] = '/';
            list_dirpath[1] = 0;
        }
    } else {
        int k, t = 1;
        list_dirpath[0] = '/';
        for (k = 0; k < prof && k < 8; k++) {
            int q = 0;
            while (pila[k][q] && t < 110) list_dirpath[t++] = pila[k][q++];
            list_dirpath[t++] = '/';
        }
        list_dirpath[t] = 0;
    }
    /* ordena (burbuja, max 64): real */
    for (i = 0; i < nn; i++) {
        for (j = i + 1; j < nn; j++) {
            int sw = 0;
            if (!u_strcmp(ord, "tipo")) {
                if (tipos[j] < tipos[i]) sw = 1;
                else if (tipos[j] == tipos[i] &&
                         u_strcmp(nombres[j], nombres[i]) < 0)
                    sw = 1;
            } else {
                if (u_strcmp(nombres[j], nombres[i]) < 0) sw = 1;
            }
            if (sw) {
                char tn[33];
                int tt;
                u_strcpy(tn, nombres[i]);
                u_strcpy(nombres[i], nombres[j]);
                u_strcpy(nombres[j], tn);
                tt = tipos[i];
                tipos[i] = tipos[j];
                tipos[j] = tt;
            }
        }
    }
    if (!nn) {
        pon("(vacia)");
        return;
    }
    for (i = 0; i < nn; i++) {
        if (!u_strcmp(fmt, "detallado")) {
            char b[96];
            int t = 0, q = 0;
            b[t++] = tipos[i] == 1 ? 'D' : tipos[i] == 3 ? 'C' : 'F';
            b[t++] = ' ';
            while (nombres[i][q] && t < 60) b[t++] = nombres[i][q++];
            if (tipos[i] == 2) {
                char nb[12];
                char ruta[128];
                int t9 = 0, w9 = 0;
                b[t++] = ' ';
                b[t++] = '(';
                while (list_dirpath[t9] && t9 < 110) {
                    ruta[t9] = list_dirpath[t9];
                    t9++;
                }
                if (t9 > 1) ruta[t9++] = '/';
                while (nombres[i][w9] && t9 < 126) {
                    ruta[t9] = nombres[i][w9];
                    w9++;
                }
                ruta[t9] = 0;
                {
                    UFILE *ff = u_fopen(ruta, "r");
                    if (ff) {
                        u_itoa(u_fsize(ff), nb);
                        u_fclose(ff);
                    } else {
                        u_itoa(0, nb);
                    }
                }
                {
                    int q2 = 0;
                    while (nb[q2] && t < 90) b[t++] = nb[q2++];
                }
                b[t++] = ')';
            }
            b[t] = 0;
            pon(b);
        } else {
            pon(nombres[i]);
        }
    }
}

/* Sube desde idx hasta la raiz devolviendo la pila de nombres.
   0 ok (prof_out = profundidad). */
static int nombres_hasta(int idx, char pila_out[8][32], int *prof_out) {
    int rev[8], rn = 0, cur = idx, k;
    while (cur != 0 && rn < 8) {
        int p, fnd = -1, n2;
        udir_t d2;
        for (p = 0; p < 64 && fnd < 0; p++) {
            n2 = 0;
            while (lista_hijo(p, n2, &d2) >= 0) {
                if (d2.idx == cur) {
                    fnd = p;
                    break;
                }
                n2++;
                if (n2 > 200) break;
            }
        }
        if (fnd < 0) return -1;
        rev[rn++] = cur;
        cur = fnd;
    }
    if (cur != 0) return -1;
    for (k = 0; k < rn; k++) {
        int want = rev[rn - 1 - k], p, fnd = 0, n2;
        udir_t d2;
        int padre = (k == 0) ? 0 : rev[rn - k];
        n2 = 0;
        while (lista_hijo(padre, n2, &d2) >= 0) {
            if (d2.idx == want) {
                int w = 0;
                while (d2.nombre[w] && w < 31) {
                    pila_out[k][w] = d2.nombre[w];
                    w++;
                }
                pila_out[k][w] = 0;
                fnd = 1;
                break;
            }
            n2++;
            if (n2 > 200) break;
        }
        if (!fnd) return -1;
    }
    *prof_out = rn;
    return 0;
}

static void op_entrar(const cmd_t *c) {
    int dir;
    char base[32];
    (void)c;
    if (npos < 1) {
        perr(10, "Falta indicar la carpeta.");
        return;
    }
    if (resuelve(pos_arg[0], &dir, base)) {
        perr(14, "Ruta no valida.");
        return;
    }
    if (!base[0]) {
        /* es un directorio ya (acaba en / o es ..): adopta */
        if (dir == 0) {
            prof = 0;
            cwd_idx = 0;
            pon("Estas en /.");
        } else {
            char npila[8][32];
            int nprof = 0, k;
            if (nombres_hasta(dir, npila, &nprof)) {
                perr(14, "Ruta no valida.");
                return;
            }
            for (k = 0; k < nprof; k++) {
                int q = 0;
                while (npila[k][q] && q < 31) {
                    pila[k][q] = npila[k][q];
                    q++;
                }
                pila[k][q] = 0;
            }
            prof = nprof;
            cwd_idx = dir;
            op_ruta(&CMDS[8]);
        }
        return;
    }
    {
        int t2 = 0;
        int idx = busca_en(dir, base, &t2);
        if (idx < 0 || t2 != 1) {
            perr(20, "No existe esa carpeta.");
            return;
        }
        if (prof >= 8) {
            perr(25, "Demasiada profundidad (m\xc3\xa1ximo 8).");
            return;
        }
        {
            int q = 0;
            while (base[q] && q < 31) {
                pila[prof][q] = base[q];
                q++;
            }
            pila[prof][q] = 0;
            prof++;
        }
        cwd_idx = idx;
    }
}

static void op_ruta(const cmd_t *c) {
    char b[128];
    int t = 0, k;
    (void)c;
    b[t++] = '/';
    for (k = 0; k < prof && k < 8; k++) {
        int q = 0;
        if (k) b[t++] = '/';
        while (pila[k][q] && t < 120) b[t++] = pila[k][q++];
    }
    b[t] = 0;
    pon(b);
}

static void op_ver(const cmd_t *c) {
    int dir, t2 = 0, idx;
    char base[32];
    char ruta[128];
    UFILE *f;
    char b[128];
    int n, total = 0;
    (void)c;
    if (npos < 1) {
        perr(10, "Falta indicar el archivo.");
        return;
    }
    if (resuelve(pos_arg[0], &dir, base) || !base[0]) {
        perr(14, "Ruta no valida.");
        return;
    }
    idx = busca_en(dir, base, &t2);
    if (idx < 0 || t2 != 2) {
        perr(20, "No existe ese archivo.");
        return;
    }
    a_absoluta(pos_arg[0], ruta);
    f = u_fopen(ruta, "r");
    if (!f) {
        perr(24, "No se pudo abrir.");
        return;
    }
    while (total < 6000 && (n = u_fread(b, 100, f)) > 0) {
        int i = 0;
        while (i < n && total < 6000) {
            char ln[100];
            int k = 0;
            while (i < n && b[i] != '\n' && k < NCOL - 1) ln[k++] = b[i++];
            ln[k] = 0;
            pon(ln);
            if (i < n && b[i] == '\n') i++;
            total += k + 1;
        }
    }
    u_fclose(f);
}

static void op_copiar(const cmd_t *c) {
    const char *sb, *vf;
    int sob, ver;
    int ddir, dt2 = 0, didx;
    char dbase[32];
    int sdir, st2 = 0, sidx;
    char sbase[32];
    char sruta[128], druta[128];
    UFILE *fo, *fd;
    char buf[256];
    int n, mal = 0;
    unsigned sum1 = 0, sum2 = 0;
    if (npos < 2) {
        perr(10, "Faltan origen y destino.");
        pon("Uso:");
        pon(c->uso);
        return;
    }
    sb = param_de(c, "sobrescribir", 0);
    vf = param_de(c, "verificar", 0);
    sob = es_si(sb);
    ver = es_si(vf);
    if (resuelve(pos_arg[0], &sdir, sbase) || !sbase[0]) {
        perr(14, "Origen no valido.");
        return;
    }
    sidx = busca_en(sdir, sbase, &st2);
    if (sidx < 0 || st2 != 2) {
        perr(20, "No existe ese archivo de origen.");
        return;
    }
    if (resuelve(pos_arg[1], &ddir, dbase)) {
        perr(14, "Destino no valido.");
        return;
    }
    if (!dbase[0]) {
        /* destino es carpeta: conserva nombre */
        int q = 0;
        while (sbase[q] && q < 31) {
            dbase[q] = sbase[q];
            q++;
        }
        dbase[q] = 0;
    } else {
        didx = busca_en(ddir, dbase, &dt2);
        if (didx >= 0 && dt2 == 1) {
            /* destino es carpeta escrita sin /: entra */
            int q = 0;
            ddir = didx;
            while (sbase[q] && q < 31) {
                dbase[q] = sbase[q];
                q++;
            }
            dbase[q] = 0;
            didx = -1;
        }
        if (didx >= 0 && !sob) {
            perr(15, "El destino ya existe (usa sobrescribir=si).");
            return;
        }
    }
    a_absoluta(pos_arg[0], sruta);
    /* destino: si es carpeta existente, conserva el nombre */
    a_absoluta(pos_arg[1], druta);
    if (!dbase[0] || (didx >= 0 && dt2 == 1)) {
        int l = u_strlen(druta), w = 0;
        while (l > 1 && druta[l - 1] == '/') l--;
        if (druta[l - 1] != '/') druta[l++] = '/';
        while (sbase[w] && l < 120) druta[l++] = sbase[w++];
        druta[l] = 0;
    }
    fo = u_fopen(sruta, "r");
    if (!fo) {
        perr(24, "No se pudo abrir el origen.");
        return;
    }
    fd = u_fopen(druta, "w");
    if (!fd) {
        u_fclose(fo);
        perr(17, "No se pudo crear el destino.");
        return;
    }
    while (!mal && (n = u_fread(buf, sizeof(buf), fo)) > 0) {
        int i;
        for (i = 0; i < n; i++) sum1 += (unsigned char)buf[i];
        if (u_fwrite(buf, n, fd) != n) mal = 1;
    }
    u_fclose(fo);
    u_fclose(fd);
    if (mal) {
        u_remove(druta);
        perr(24, "Error al copiar.");
        return;
    }
    if (ver) {
        UFILE *fv = u_fopen(druta, "r");
        if (!fv) {
            perr(24, "No se pudo verificar.");
            return;
        }
        while ((n = u_fread(buf, sizeof(buf), fv)) > 0) {
            int i;
            for (i = 0; i < n; i++) sum2 += (unsigned char)buf[i];
        }
        u_fclose(fv);
        if (sum1 != sum2) {
            perr(26, "La copia no coincide (sumas distintas).");
            return;
        }
    }
    pon("Archivo copiado.");
}

static void op_mover(const cmd_t *c) {
    int sdir, st2 = 0, ddir, dt2 = 0, didx;
    char sbase[32], dbase[32];
    char sruta[128], druta[128];
    (void)c;
    if (npos < 2) {
        perr(10, "Faltan origen y destino.");
        return;
    }
    if (resuelve(pos_arg[0], &sdir, sbase) || !sbase[0]) {
        perr(14, "Origen no valido.");
        return;
    }
    if (busca_en(sdir, sbase, &st2) < 0) {
        perr(20, "No existe ese origen.");
        return;
    }
    if (resuelve(pos_arg[1], &ddir, dbase)) {
        perr(14, "Destino no valido.");
        return;
    }
    if (!dbase[0]) {
        int q = 0;
        while (sbase[q] && q < 31) {
            dbase[q] = sbase[q];
            q++;
        }
        dbase[q] = 0;
    } else {
        didx = busca_en(ddir, dbase, &dt2);
        if (didx >= 0 && dt2 == 1) {
            int q = 0;
            ddir = didx;
            while (sbase[q] && q < 31) {
                dbase[q] = sbase[q];
                q++;
            }
            dbase[q] = 0;
            didx = -1;
        }
    }
    a_absoluta(pos_arg[0], sruta);
    if (ddir == sdir) {
        int r = u_renombrar(sruta, dbase);
        if (r == -2) perr(15, "Ya existe con ese nombre.");
        else if (r) perr(24, "No se pudo mover.");
        else pon("Movido.");
        return;
    }
    /* otra carpeta: copiar y eliminar (no sobrescribe) */
    {
        char dd2[128];
        abs_dir_de(pos_arg[1], dbase, dd2);
        {
            int w = 0;
            char dst[128];
            int k = 0;
            while (dd2[k] && k < 110) {
                dst[k] = dd2[k];
                k++;
            }
            if (k > 1) dst[k++] = '/';
            while (dbase[w] && k < 126) dst[k++] = dbase[w++];
            dst[k] = 0;
            {
                UFILE *fo = u_fopen(sruta, "r");
                UFILE *fd;
                char buf[256];
                int n, mal = 0;
                if (!fo) {
                    perr(24, "No se pudo abrir el origen.");
                    return;
                }
                /* no sobrescribe: comprueba */
                {
                    UFILE *fx = u_fopen(dst, "r");
                    if (fx) {
                        u_fclose(fx);
                        u_fclose(fo);
                        perr(15, "El destino ya existe.");
                        return;
                    }
                }
                fd = u_fopen(dst, "w");
                if (!fd) {
                    u_fclose(fo);
                    perr(17, "No se pudo crear el destino.");
                    return;
                }
                while (!mal && (n = u_fread(buf, sizeof(buf), fo)) > 0) {
                    if (u_fwrite(buf, n, fd) != n) mal = 1;
                }
                u_fclose(fo);
                u_fclose(fd);
                if (mal) {
                    u_remove(dst);
                    perr(24, "Error al mover.");
                    return;
                }
                if (u_remove(sruta)) {
                    perr(24, "Copiado pero no se pudo borrar el origen.");
                    return;
                }
                pon("Movido.");
            }
        }
    }
}

static void op_renombrar(const cmd_t *c) {
    int dir;
    char base[32], nuevo[32];
    char ruta[128];
    int t2 = 0;
    (void)c;
    if (npos < 2) {
        perr(10, "Faltan el nombre actual y el nuevo.");
        return;
    }
    if (resuelve(pos_arg[0], &dir, base) || !base[0]) {
        perr(14, "Ruta no valida.");
        return;
    }
    {
        int q = 0;
        while (pos_arg[1][q] && pos_arg[1][q] != '/' && q < 31) {
            nuevo[q] = pos_arg[1][q];
            q++;
        }
        nuevo[q] = 0;
        if (pos_arg[1][q]) {
            perr(27, "El nuevo nombre no lleva ruta (misma carpeta).");
            return;
        }
    }
    if (busca_en(dir, base, &t2) < 0) {
        perr(20, "No existe con ese nombre.");
        return;
    }
    a_absoluta(pos_arg[0], ruta);
    {
        int r = u_renombrar(ruta, nuevo);
        if (r == -2) perr(15, "Ya existe con ese nombre.");
        else if (r) perr(24, "No se pudo cambiar el nombre.");
        else pon("Nombre cambiado.");
    }
}

static void op_limpiar(const cmd_t *c) {
    (void)c;
    nlin = 0;
}

static void op_info(const cmd_t *c, ug_win_t *v) {
    unsigned int m[8];
    char b[80];
    int t = 0, q;
    char nb[12];
    uproc_t ps[24];
    int n, i, vivos = 0;
    (void)c;
    if (u_info_memoria2(m)) {
        perr(30, "No se pudo leer la memoria.");
    } else {
        const char *s = "memoria libres=";
        while (*s && t < 70) b[t++] = *s++;
        u_itoa((int)m[0], nb);
        q = 0;
        while (nb[q] && t < 70) b[t++] = nb[q++];
        b[t] = 0;
        pon(b);
    }
    n = u_listar(ps, 24);
    if (n >= 0) {
        for (i = 0; i < n; i++) {
            if (ps[i].estado != 4) vivos++;
        }
        {
            const char *s = "procesos=";
            t = 0;
            while (*s && t < 70) b[t++] = *s++;
            u_itoa(vivos, nb);
            q = 0;
            while (nb[q] && t < 70) b[t++] = nb[q++];
            b[t] = 0;
            pon(b);
        }
    }
    {
        const char *s = "ventana=";
        t = 0;
        while (*s && t < 70) b[t++] = *s++;
        u_itoa(v->w, nb);
        q = 0;
        while (nb[q] && t < 70) b[t++] = nb[q++];
        b[t++] = 'x';
        u_itoa(v->h, nb);
        q = 0;
        while (nb[q] && t < 70) b[t++] = nb[q++];
        b[t] = 0;
        pon(b);
    }
}

static void op_fecha(const cmd_t *c) {
    unsigned int v[6];
    char b[32];
    int t = 0;
    (void)c;
    if (u_reloj(v)) {
        perr(31, "No se pudo leer el reloj.");
        return;
    }
    dos2(b, &t, v[2]);
    b[t++] = '/';
    dos2(b, &t, v[1]);
    b[t++] = '/';
    {
        char nb[12];
        unsigned yr = v[0] < 100 ? v[0] + 2000 : v[0] % 10000;
        u_itoa((int)yr, nb);
        {
            int k = 0;
            while (k < 4 - (int)u_strlen(nb) && t < 28) {
                b[t++] = '0';
                k++;
            }
            k = 0;
            while (nb[k] && t < 28) b[t++] = nb[k++];
        }
    }
    b[t] = 0;
    pon(b);
}

static void op_hora(const cmd_t *c) {
    unsigned int v[6];
    char b[16];
    int t = 0;
    (void)c;
    if (u_reloj(v)) {
        perr(31, "No se pudo leer el reloj.");
        return;
    }
    dos2(b, &t, v[3]);
    b[t++] = ':';
    dos2(b, &t, v[4]);
    b[t++] = ':';
    dos2(b, &t, v[5]);
    b[t] = 0;
    pon(b);
}

static void op_teclado(const cmd_t *c) {
    int id;
    (void)c;
    if (npos < 1) {
        id = sys_teclado_dist(0, 0);
        if (id == 2) pon("Teclado actual: catalan.");
        else if (id == 1) pon("Teclado actual: espanol.");
        else pon("Teclado actual: americano.");
        return;
    }
    if (!u_strcmp(pos_arg[0], "es")) id = 1;
    else if (!u_strcmp(pos_arg[0], "ca")) id = 2;
    else if (!u_strcmp(pos_arg[0], "us")) id = 0;
    else {
        perr(32, "Distribucion desconocida (es, ca, us).");
        return;
    }
    if (sys_teclado_dist(1, id)) perr(24, "No se pudo cambiar.");
    else pon("Distribucion cambiada.");
}

static void op_historial(const cmd_t *c) {
    int i;
    (void)c;
    for (i = 0; i < nhist; i++) {
        char b[300];
        int t = 0, q = 0;
        char nb[12];
        u_itoa(i + 1, nb);
        while (nb[q] && t < 280) b[t++] = nb[q++];
        b[t++] = ' ';
        b[t++] = ' ';
        q = 0;
        while (hist[i][q] && t < 290) b[t++] = hist[i][q++];
        b[t] = 0;
        pon(b);
    }
    if (!nhist) pon("(vacio)");
}

static void op_eco(const cmd_t *c) {
    char b[400];
    int t = 0, i;
    (void)c;
    for (i = 0; i < npos; i++) {
        int q = 0;
        if (i) b[t++] = ' ';
        while (pos_arg[i][q] && t < 390) b[t++] = pos_arg[i][q++];
    }
    b[t] = 0;
    pon(b);
}

/* ---- red propia (v2.3-red F1) ---- */
static void red_pon_num(const char *tit, unsigned int v) {
    char b[64], nb[12];
    int t = 0, q = 0;
    while (tit[q] && t < 50) b[t++] = tit[q++];
    b[t++] = '=';
    u_utoa(v, nb, 10);
    q = 0;
    while (nb[q] && t < 62) b[t++] = nb[q++];
    b[t] = 0;
    pon(b);
}

static void op_mostrar_interfaz(const cmd_t *c) {
    red_if_t f;
    int i, rc;
    (void)c;
    for (i = 0; i < 32; i++) ((unsigned char *)&f)[i] = 0;
    rc = u_red_interfaz(&f);
    if (rc || !f.presente) {
        pon("Red no disponible: sin adaptador.");
        pon("Arranque sin red (conecta una RTL8139).");
        return;
    }
    pon("Adaptador Ethernet (RTL8139).");
    pon("Estado: activo.");
    {
        char b[32];
        int t = 0, i;
        b[t++] = 'M';
        b[t++] = 'A';
        b[t++] = 'C';
        b[t++] = ':';
        b[t++] = ' ';
        for (i = 0; i < 6 && t < 28; i++) {
            unsigned char v = f.mac[i];
            unsigned char hi = (unsigned char)(v >> 4);
            unsigned char lo = (unsigned char)(v & 15);
            b[t++] = hi < 10 ? (char)('0' + hi) : (char)('A' + hi - 10);
            b[t++] = lo < 10 ? (char)('0' + lo) : (char)('A' + lo - 10);
            if (i < 5) b[t++] = ':';
        }
        b[t] = 0;
        pon(b);
    }
    red_pon_num("MTU", f.mtu);
    red_pon_num("Tramas enviadas", f.tx_ok);
    red_pon_num("Tramas recibidas", f.rx_ok);
    red_pon_num("Errores envio", f.tx_err);
    red_pon_num("Errores recepcion", f.rx_err);
    {
        red_cfg_t c0;
        int i;
        for (i = 0; i < 16; i++) ((unsigned char *)&c0)[i] = 0;
        if (!u_red_configurar_leer(&c0) &&
            (c0.ip || c0.mascara || c0.puerta || c0.dns)) {
            pon_ip("IP", c0.ip);
            pon_ip("Mascara", c0.mascara);
            pon_ip("Puerta", c0.puerta);
            pon_ip("DNS", c0.dns);
        } else {
            pon("IPv4 sin configurar.");
        }
    }
    pon("TTL: 64");
    red_pon_num("Entradas ARP en cache", (unsigned int)f.rsv[0]);
    red_pon_num("ICMP ok", (unsigned int)f.rsv[1]);
    red_pon_num("ICMP errores", (unsigned int)f.rsv[2]);
}

static void op_red(const cmd_t *c) {
    (void)c;
    if (npos > 0 && u_strcmp(pos_arg[0], "estado")) {
        perr(10, "Uso: red [estado].");
        return;
    }
    if (npos == 0) {
        red_if_t f;
        int i;
        for (i = 0; i < 32; i++) ((unsigned char *)&f)[i] = 0;
        if (u_red_interfaz(&f) || !f.presente) {
            pon("Red: sin adaptador.");
            return;
        }
        pon("Red: adaptador activo.");
        return;
    }
    op_mostrar_interfaz(c);
}

/* ---- IPv4 manual (F2): parsea a.b.c.d a u32 (byte bajo = a). */
static int lee_ip(const char *s, unsigned int *v) {
    unsigned int b[4];
    int i, q = 0;
    for (i = 0; i < 4; i++) {
        unsigned int n = 0;
        int dig = 0;
        while (s[q] >= '0' && s[q] <= '9') {
            n = n * 10 + (unsigned int)(s[q] - '0');
            if (n > 255) return 0;
            q++;
            dig = 1;
        }
        if (!dig) return 0;
        b[i] = n;
        if (i < 3) {
            if (s[q] != '.') return 0;
            q++;
        }
    }
    if (s[q]) return 0;
    *v = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
    return 1;
}

static void pon_ip(const char *tit, unsigned int v) {
    char b[64];
    int t = 0, q = 0, i;
    while (tit[q] && t < 40) b[t++] = tit[q++];
    b[t++] = ':';
    b[t++] = ' ';
    for (i = 0; i < 4; i++) {
        unsigned int o = (v >> (i * 8)) & 0xFFu;
        char nb[12];
        int k = 0;
        u_utoa(o, nb, 10);
        while (nb[k] && t < 60) b[t++] = nb[k++];
        if (i < 3) b[t++] = '.';
    }
    b[t] = 0;
    pon(b);
}

static void op_configurar_ip(const cmd_t *c) {
    red_cfg_t c0;
    int i;
    (void)c;
    for (i = 0; i < 16; i++) ((unsigned char *)&c0)[i] = 0;
    if (npos == 0) {
        if (u_red_configurar_leer(&c0)) {
            perr(10, "No se pudo leer la configuracion.");
            return;
        }
        if (!c0.ip && !c0.mascara && !c0.puerta && !c0.dns)
            pon("IPv4 sin configurar.");
        else {
            pon_ip("IP", c0.ip);
            pon_ip("Mascara", c0.mascara);
            pon_ip("Puerta", c0.puerta);
            pon_ip("DNS", c0.dns);
        }
        return;
    }
    if (npos == 1) {
        unsigned int v;
        if (!lee_ip(pos_arg[0], &v)) {
            perr(10, "IP invalida (usa a.b.c.d).");
            return;
        }
        if (u_red_fijar(1, v)) perr(33, "No se pudo fijar la IP.");
        else pon("IP fijada.");
        return;
    }
    if (npos == 2) {
        unsigned int v;
        int campo = 0;
        if (!u_strcmp(pos_arg[0], "mascara")) campo = 2;
        else if (!u_strcmp(pos_arg[0], "puerta")) campo = 3;
        else if (!u_strcmp(pos_arg[0], "dns")) campo = 4;
        else {
            perr(10, "Uso: configurar_ip [mascara|puerta|dns] <ip>.");
            return;
        }
        if (!lee_ip(pos_arg[1], &v)) {
            perr(10, "IP invalida (usa a.b.c.d).");
            return;
        }
        if (u_red_fijar(campo, v)) perr(33, "No se pudo fijar.");
        else pon("Valor fijado.");
        return;
    }
    perr(10, "Uso: configurar_ip [ip] | [mascara|puerta|dns] <ip>.");
}

static void op_mostrar_rutas(const cmd_t *c) {
    unsigned char rutas[96];
    int n, i;
    (void)c;
    for (i = 0; i < 96; i++) rutas[i] = 0;
    n = u_red_rutas(rutas, 96);
    if (n < 0) {
        perr(33, "No se pudo leer la tabla de rutas.");
        return;
    }
    if (!n) {
        pon("Sin rutas: configura una IP.");
        return;
    }
    pon("Destino          Mascara          Puerta");
    for (i = 0; i + 12 <= n; i += 12) {
        char b[72];
        int t = 0, k, o;
        unsigned int v;
        v = (unsigned int)rutas[i] |
            ((unsigned int)rutas[i + 1] << 8) |
            ((unsigned int)rutas[i + 2] << 16) |
            ((unsigned int)rutas[i + 3] << 24);
        for (o = 0; o < 4; o++) {
            char nb[12];
            int q2 = 0;
            u_utoa((v >> (o * 8)) & 0xFFu, nb, 10);
            while (nb[q2] && t < 16) b[t++] = nb[q2++];
            b[t++] = o < 3 ? '.' : ' ';
        }
        while (t < 18) b[t++] = ' ';
        v = (unsigned int)rutas[i + 4] |
            ((unsigned int)rutas[i + 5] << 8) |
            ((unsigned int)rutas[i + 6] << 16) |
            ((unsigned int)rutas[i + 7] << 24);
        for (o = 0; o < 4; o++) {
            char nb[12];
            int q2 = 0;
            u_utoa((v >> (o * 8)) & 0xFFu, nb, 10);
            while (nb[q2] && t < 36) b[t++] = nb[q2++];
            b[t++] = o < 3 ? '.' : ' ';
        }
        while (t < 38) b[t++] = ' ';
        v = (unsigned int)rutas[i + 8] |
            ((unsigned int)rutas[i + 9] << 8) |
            ((unsigned int)rutas[i + 10] << 16) |
            ((unsigned int)rutas[i + 11] << 24);
        if (!v) {
            const char *s = "directa";
            while (*s && t < 60) b[t++] = *s++;
        } else {
            for (o = 0; o < 4; o++) {
                char nb[12];
                int q2 = 0;
                u_utoa((v >> (o * 8)) & 0xFFu, nb, 10);
                while (nb[q2] && t < 60) b[t++] = nb[q2++];
                if (o < 3) b[t++] = '.';
            }
        }
        b[t] = 0;
        pon(b);
        (void)k;
    }
}

static void op_comprovar_conexion(const cmd_t *c) {
    red_peticion_t pet;
    unsigned char resp[60];
    red_cfg_t cfg;
    unsigned int ip = 0;
    int i, n = 4, r;
    (void)c;
    /* Sintaxis: comprovar_conexion [-c N] <ip> (F3: solo IPv4). */
    if (npos >= 1 && !u_strcmp(pos_arg[0], "-c")) {
        if (npos != 3) {
            perr(10, "Uso: comprovar_conexion [-c N] <ip>.");
            return;
        }
        n = u_atoi(pos_arg[1]);
        if (n < 1 || n > 10) {
            perr(10, "N debe estar entre 1 y 10.");
            return;
        }
        if (!lee_ip(pos_arg[2], &ip)) {
            perr(10, "IP invalida (usa a.b.c.d).");
            return;
        }
    } else if (npos == 1) {
        if (!lee_ip(pos_arg[0], &ip)) {
            perr(10, "IP invalida (usa a.b.c.d). Nombres en F4.");
            return;
        }
    } else {
        perr(10, "Uso: comprovar_conexion [-c N] <ip>.");
        return;
    }
    for (i = 0; i < 16; i++) ((unsigned char *)&cfg)[i] = 0;
    if (u_red_configurar_leer(&cfg) || !cfg.ip) {
        perr(10, "Sin IP: configura una IP o DHCP primero.");
        return;
    }
    pet.ip = ip;
    pet.n = (unsigned short)n;
    pet.espera = 1000;
    pet.ttl = 0;
    pet.res = 0;
    pet.res2 = 0;
    for (i = 0; i < 60; i++) resp[i] = 0;
    r = u_red_comprobar(&pet, (red_respuesta_t *)resp,
                        (int)sizeof(resp));
    if (r < 0) {
        if (r == -1) perr(33, "Red no disponible: sin adaptador.");
        else if (r == -3) perr(33, "Sin IP configurada.");
        else perr(33, "No se pudo comprobar.");
        return;
    }
    {
        int ok = 0, k;
        for (k = 0; k + 6 <= r; k += 6) {
            unsigned int seq = ((unsigned int)resp[k] << 8) | resp[k + 1];
            unsigned int ms = ((unsigned int)resp[k + 2] << 8) | resp[k + 3];
            if (resp[k + 4]) {
                char b[80];
                int t = 0, q = 0;
                const char *s = "respuesta de ";
                unsigned int v = ip;
                int o;
                while (s[q] && t < 60) b[t++] = s[q++];
                for (o = 0; o < 4; o++) {
                    char nb[12];
                    int w = 0;
                    u_utoa((v >> (o * 8)) & 0xFFu, nb, 10);
                    while (nb[w] && t < 60) b[t++] = nb[w++];
                    if (o < 3) b[t++] = '.';
                }
                b[t++] = ':';
                b[t++] = ' ';
                b[t] = 0;
                pon(b);
                {
                    char b3[64];
                    int t3 = 0;
                    char nb[12];
                    int w3 = 0;
                    const char *s3 = "seq=";
                    while (s3[w3] && t3 < 50) b3[t3++] = s3[w3++];
                    u_utoa(seq, nb, 10);
                    w3 = 0;
                    while (nb[w3] && t3 < 50) b3[t3++] = nb[w3++];
                    {
                        const char *s4 = "  ttl=";
                        int w4 = 0;
                        while (s4[w4] && t3 < 50) b3[t3++] = s4[w4++];
                    }
                    u_utoa(resp[k + 5], nb, 10);
                    w3 = 0;
                    while (nb[w3] && t3 < 50) b3[t3++] = nb[w3++];
                    {
                        const char *s4 = "  tiempo=";
                        int w4 = 0;
                        while (s4[w4] && t3 < 50) b3[t3++] = s4[w4++];
                    }
                    u_utoa(ms, nb, 10);
                    w3 = 0;
                    while (nb[w3] && t3 < 50) b3[t3++] = nb[w3++];
                    {
                        const char *s5 = " ms";
                        int w5 = 0;
                        while (s5[w5] && t3 < 50) b3[t3++] = s5[w5++];
                    }
                    b3[t3] = 0;
                    pon(b3);
                }
                ok++;
            } else {
                char b[48];
                int t = 0, q = 0;
                const char *s = "sin respuesta (seq=";
                char nb[12];
                int w = 0;
                while (s[q] && t < 40) b[t++] = s[q++];
                u_utoa(seq, nb, 10);
                while (nb[w] && t < 40) b[t++] = nb[w++];
                b[t++] = ')';
                b[t] = 0;
                pon(b);
            }
        }
        {
            char b[48];
            int t = 0;
            char nb[12];
            int w = 0;
            u_utoa((unsigned int)ok, nb, 10);
            while (nb[w] && t < 40) b[t++] = nb[w++];
            {
                const char *s = " de ";
                int q = 0;
                while (s[q] && t < 40) b[t++] = s[q++];
            }
            u_utoa((unsigned int)n, nb, 10);
            w = 0;
            while (nb[w] && t < 40) b[t++] = nb[w++];
            {
                const char *s = " respuestas.";
                int q = 0;
                while (s[q] && t < 40) b[t++] = s[q++];
            }
            b[t] = 0;
            pon(b);
        }
    }
}

static void op_configurar_dhcp(const cmd_t *c) {
    char args[32];
    int t = 0, rc;
    char rb[32];
    (void)c;
    if (npos > 1) {
        perr(10, "Uso: configurar_dhcp [renovar|liberar].");
        return;
    }
    if (npos == 1) {
        if (u_strcmp(pos_arg[0], "renovar") &&
            u_strcmp(pos_arg[0], "liberar")) {
            perr(10, "Uso: configurar_dhcp [renovar|liberar].");
            return;
        }
        while (pos_arg[0][t] && t < 30) {
            args[t] = pos_arg[0][t];
            t++;
        }
        args[t] = 0;
    } else {
        args[0] = 0;
    }
    ug_modal(v_term, 1);
    rc = u_ejecutar("dhcp", args);
    ug_modal(v_term, 0);
    muestra_sal_fich("/dhcsal.txt");
    {
        char nb[12];
        int w = 0, q = 0;
        rb[w++] = 'r';
        rb[w++] = 'c';
        rb[w++] = '=';
        u_itoa(rc, nb);
        while (nb[q] && w < 30) rb[w++] = nb[q++];
        rb[w] = 0;
    }
    pon(rb);
    if (rc == 0) pon("DHCP aplicado correctamente.");
    else if (rc < -100) perr(33, "No se pudo lanzar (nombre o formato).");
    else perr(33, "DHCP sin respuesta (revisa la red).");
}

static int termina_en_f32(const char *r) {
    int l = u_strlen(r);
    if (l < 4) return 0;
    return r[l - 4] == '.' &&
           (r[l - 3] == 'f' || r[l - 3] == 'F') &&
           (r[l - 2] == '3') &&
           (r[l - 1] == '2');
}

static int tiene_punto(const char *r) {
    int i = 0;
    while (r[i]) {
        if (r[i] == '.') return 1;
        i++;
    }
    return 0;
}

/* Captura Ring3 (sin pipes en ABI): el hijo modal duplica su
   salida a un fichero temporal; la terminal la vuelca en la
   ventana tras el hijo. Sin este volcado, en escritorio solo se
   veria rc=N. */
static void muestra_sal_fich(const char *ruta) {
    UFILE *f = u_fopen(ruta, "r");
    char buf[513];
    int n, total = 0;
    if (!f) return;
    while (total < 8192 && (n = u_fread(buf, 512, f)) > 0) {
        buf[n] = 0;
        pon(buf);
        total += n;
    }
    u_fclose(f);
    u_remove(ruta);
}

static void op_ejecutar(ug_win_t *v, const cmd_t *c) {
    char args[256];
    int t = 0, i, rc;
    char rb[32];
    char ruta[128];
    int es_f32 = 0;
    (void)c;
    if (npos < 1) {
        perr(10, "Falta indicar el programa o archivo .f32.");
        pon("Uso:");
        pon("ejecutar <programa> [args]");
        return;
    }
    /* Rama .f32 (F32 Lang v0.1): el argumento es un script, no un
       programa F32X. Sin extension se prueba anadiendo .f32. */
    if (termina_en_f32(pos_arg[0])) {
        es_f32 = 1;
        a_absoluta(pos_arg[0], ruta);
    } else if (!tiene_punto(pos_arg[0])) {
        char con_ext[128];
        int q = 0;
        UFILE *pr;
        while (pos_arg[0][q] && q < 120) {
            con_ext[q] = pos_arg[0][q];
            q++;
        }
        con_ext[q++] = '.';
        con_ext[q++] = 'f';
        con_ext[q++] = '3';
        con_ext[q++] = '2';
        con_ext[q] = 0;
        a_absoluta(con_ext, ruta);
        pr = u_fopen(ruta, "r");
        if (pr) {
            u_fclose(pr);
            es_f32 = 1;
        }
    }
    if (es_f32) {
        if (npos > 1) {
            perr(10, "ejecutar archivo.f32 no admite argumentos.");
            return;
        }
        ug_modal(v, 1);
        rc = u_ejecutar("f32", ruta);
        ug_modal(v, 0);
        muestra_sal_fich("/f32sal.tmp");
        {
            char nb[12];
            int w = 0, q = 0;
            rb[w++] = 'r';
            rb[w++] = 'c';
            rb[w++] = '=';
            u_itoa(rc, nb);
            while (nb[q] && w < 30) rb[w++] = nb[q++];
            rb[w] = 0;
        }
        pon(rb);
        if (rc == 0) pon("Programa finalizado correctamente.");
        else if (rc < -100) perr(33, "No se pudo lanzar (nombre o formato).");
        return;
    }
    for (i = 1; i < npos; i++) {
        int q = 0;
        if (t) args[t++] = ' ';
        while (pos_arg[i][q] && t < 250) args[t++] = pos_arg[i][q++];
    }
    args[t] = 0;
    ug_modal(v, 1);
    rc = u_ejecutar(pos_arg[0], args);
    ug_modal(v, 0);
    if (!u_strcmp(pos_arg[0], "f32")) muestra_sal_fich("/f32sal.tmp");
    {
        char nb[12];
        int w = 0, q = 0;
        rb[w++] = 'r';
        rb[w++] = 'c';
        rb[w++] = '=';
        u_itoa(rc, nb);
        while (nb[q] && w < 30) rb[w++] = nb[q++];
        rb[w] = 0;
    }
    pon(rb);
    if (rc < -100) {
        perr(33, "No se pudo lanzar (nombre o formato).");
    }
}

/* Sugerencias: prefijo primero, luego contenido (max 4). */
static int sugiere(const char *nom, char dst[4][32]) {
    int n = 0, i;
    for (i = 0; i < NCMDS && n < 4; i++) {
        int k = 0;
        while (nom[k] && CMDS[i].nombre[k] == nom[k]) k++;
        if (nom[k] == 0 && CMDS[i].nombre[k]) {
            u_strcpy(dst[n++], CMDS[i].nombre);
        }
    }
    if (n) return n;
    for (i = 0; i < NCMDS && n < 4; i++) {
        const char *s = CMDS[i].nombre;
        int k = 0, ok = 0;
        if (!u_strcmp(s, nom)) continue;
        while (s[k]) {
            int j = 0;
            while (nom[j] && s[k + j] == nom[j]) j++;
            if (nom[j] == 0 && j > 0) {
                ok = 1;
                break;
            }
            k++;
        }
        if (ok) u_strcpy(dst[n++], CMDS[i].nombre);
    }
    return n;
}

/* Aplica el item seleccionado: sustituye el token actual. */
static void pop_aplica(void) {
    int ts = pos;
    char *it;
    int extra = 0;
    if (!pop_abierto || pop_n <= 0) return;
    if (pop_sel < 0) pop_sel = 0;
    if (pop_sel >= pop_n) pop_sel = pop_n - 1;
    it = pop_items[pop_sel];
    while (ts > 0 && ent[ts - 1] != ' ' && ent[ts - 1] != '\t') ts--;
    /* comillas: si el token empezaba con ", se conserva y se cierra;
       si el item lleva espacios y no habia comilla, se envuelve. */
    {
        int j = 0, solo_esp = 1, citado = 0, con_esp = 0;
        for (j = 0; j < ts; j++) {
            if (ent[j] != ' ' && ent[j] != '\t') {
                solo_esp = 0;
                break;
            }
        }
        if (ent[ts] == '"') {
            citado = 1;
            ts++;
        }
        {
            int li = u_strlen(it), k;
            for (k = 0; k < li; k++) {
                if (it[k] == ' ') {
                    con_esp = 1;
                    break;
                }
            }
            if (solo_esp) extra = 1;
            else if (citado) extra = 2;
            else if (con_esp) extra = 3;
            else if (li > 0 && it[li - 1] != '/' && it[li - 1] != '=') {
                extra = 1;
            }
        }
    }
    {
        int li = u_strlen(it);
        int resto = nent - pos;
        if (ts + li + extra + resto >= NENTB - 1) return;
        {
            int i;
            for (i = resto - 1; i >= 0; i--) ent[ts + li + extra + i] = ent[pos + i];
        }
        {
            int i;
            for (i = 0; i < li; i++) ent[ts + i] = it[i];
        }
        if (extra == 1) {
            ent[ts + li] = ' ';
        } else if (extra == 2) {
            ent[ts + li] = '"';
            ent[ts + li + 1] = ' ';
        } else if (extra == 3) {
            /* envuelve: comilla de apertura antes del item */
            int i;
            for (i = resto + li + 2; i > 0; i--) ent[ts + i] = ent[ts + i - 1];
            ent[ts] = '"';
            ent[ts + li + 1] = '"';
            ent[ts + li + 2] = ' ';
        }
        nent = ts + li + extra + resto;
        ent[nent] = 0;
        pos = ts + li + extra;
    }
    pop_abierto = 0;
}

static void pop_abre(void) {
    int n = pop_construye();
    if (n <= 0) {
        pop_abierto = 1;
        pop_n = 0;
        u_strcpy(pop_titulo, "SIN COINCIDENCIAS");
        pop_sel = 0;
        pop_ini = 0;
        return;
    }
    if (n == 1) {
        pop_n = n;
        pop_sel = 0;
        pop_abierto = 0;
        pop_abre_uno();
        return;
    }
    pop_abierto = 1;
    pop_n = n;
    pop_sel = 0;
    pop_ini = 0;
}

static void pop_abre_uno(void) {
    /* completa directamente la unica coincidencia */
    pop_abierto = 1;
    pop_aplica();
}

static void hist_guarda_ent(void) {
    int l = u_strlen(ent), i;
    if (!l) return;
    if (nhist > 0 && !u_strcmp(hist[nhist - 1], ent)) {
        hh = nhist;
        return;
    }
    if (nhist < NHIST) {
        u_strcpy(hist[nhist++], ent);
    } else {
        for (i = 0; i < NHIST - 1; i++) u_strcpy(hist[i], hist[i + 1]);
        u_strcpy(hist[NHIST - 1], ent);
    }
    hh = nhist;
}

static void hist_recupera(void) {
    if (hh >= 0 && hh < nhist) {
        u_strcpy(ent, hist[hh]);
        nent = u_strlen(ent);
        pos = nent;
    } else {
        ent[0] = 0;
        nent = 0;
        pos = 0;
    }
}

static void inserta_cp(unsigned cp) {
    char tmp[5];
    int w = ut_enc(tmp, 5, cp), i;
    if (!w) return;
    if (nent + w >= NENTB - 1) return;
    for (i = nent; i >= pos; i--) ent[i + w] = ent[i];
    for (i = 0; i < w; i++) ent[pos + i] = tmp[i];
    nent += w;
    pos += w;
}

static void borra_antes(void) {
    if (pos <= 0) return;
    {
        int a = ut_anterior(ent, pos), i;
        for (i = a; i <= nent; i++) ent[i] = ent[i + (pos - a)];
        nent -= pos - a;
        pos = a;
    }
}

static void borra_en(void) {
    int s;
    if (pos >= nent) return;
    s = ut_siguiente(ent, pos);
    {
        int i;
        for (i = pos; i <= nent; i++) ent[i] = ent[i + (s - pos)];
        nent -= s - pos;
    }
}

/* Caracteres en s[0..pos): columna del cursor de edicion. */
static int cuenta_hasta(const char *s, int pos) {
    int n = 0, p = 0;
    if (pos < 0) pos = 0;
    while (p < pos && s[p]) {
        p = ut_siguiente(s, p);
        n++;
    }
    return n;
}

/* Lineas de historial que caben en la ventana. */
static int vis_lineas(ug_win_t *v) {
    int vis = (v->h - UG_CHROME - 24) / UG_FH;
    return vis < 1 ? 1 : vis;
}

static void dibuja_term(ug_win_t *v) {
    int vis = vis_lineas(v), ini, li, i;
    ug_comienza(v);
    ug_limpia(v, T_VENT_FONDO);
    if (vis < 1) vis = 1;
    {
        /* Ventana de historial: del fondo hacia arriba segun desplaza. */
        int maxdes = nlin > vis ? nlin - vis : 0;
        if (desplaza > maxdes) desplaza = maxdes;
        if (desplaza < 0) desplaza = 0;
        ini = nlin > vis ? nlin - vis - desplaza : 0;
    }
    for (li = 0; li < vis && ini + li < nlin; li++)
        ug_texto(v, 4, 4 + li * UG_FH, pant[ini + li], C_GRIS);
    {
        char pr[32];
        int p = 0;
        const char *s = "FacilOS32> ";
        int maxc = (v->w - 60) / UG_FW, st = 0;
        int yb = 4 + li * UG_FH;
        while (*s && p < 30) pr[p++] = *s++;
        pr[p] = 0;
        ug_texto(v, 4, yb, pr, C_VERDE_CLARO);
        if (maxc > NENTB) maxc = NENTB;
        if (maxc < 1) maxc = 1;
        {
            /* La ventana sigue al cursor de edicion (antes se quedaba
               fija al final y las flechas parecian no hacer nada). */
            int col_cur = cuenta_hasta(ent, pos);
            if (col_cur < st) st = col_cur;
            else if (col_cur >= st + maxc) st = col_cur - maxc + 1;
        }
        /* dibuja con desplazamiento por caracteres */
        {
            int bi = 0, ci = 0;
            while (ent[bi] && ci < st) {
                bi = ut_siguiente(ent, bi);
                ci++;
            }
            {
                char vv[NENTB + 2];
                int t = 0;
                while (ent[bi] && t < maxc && t < NENTB) {
                    int av = 0;
                    ut_dec(ent + bi, 4, &av);
                    if (av <= 0) av = 1;
                    {
                        int k;
                        for (k = 0; k < av && t < maxc; k++)
                            vv[t++] = ent[bi + k];
                    }
                    bi += av;
                }
                vv[t] = 0;
                ug_texto(v, 4 + 11 * UG_FW, yb, vv, C_BLANCO);
            }
        }
        if (v->foco && !pop_abierto) {
            int col = cuenta_hasta(ent, pos) - st;
            if (col < 0) col = 0;
            if (col >= maxc) col = maxc - 1;
            ug_relleno(v, 4 + 11 * UG_FW + col * UG_FW, yb, 2, UG_FH,
                       C_BLANCO);
        }
    }
    /* selector TAB */
    if (pop_abierto) {
        int filas = (v->h - 60) / UG_FH - 2;
        int x0 = 4, w0 = v->w - 8, nvis, r;
        if (filas > 6) filas = 6;
        if (filas < 2) filas = 2;
        {
            int yb = v->h - UG_CHROME - (filas + 2) * UG_FH - 4;
            if (yb < 20) yb = 20;
            ug_relleno(v, x0, yb, w0, (filas + 2) * UG_FH, C_NEGRO);
            ug_rect(v, x0, yb, w0, (filas + 2) * UG_FH, C_CYAN);
            ug_texto(v, x0 + 4, yb + 2, pop_titulo, C_AMARILLO);
            if (pop_sel < pop_ini) pop_ini = pop_sel;
            if (pop_sel >= pop_ini + filas) pop_ini = pop_sel - filas + 1;
            nvis = pop_n - pop_ini;
            if (nvis > filas) nvis = filas;
            for (r = 0; r < nvis; r++) {
                int yy = yb + UG_FH + r * UG_FH;
                if (pop_ini + r == pop_sel)
                    ug_relleno(v, x0 + 1, yy, w0 - 2, UG_FH, C_AZUL);
                ug_texto(v, x0 + 8, yy,
                         pop_ini + r == pop_sel ? "> " : "  ", C_BLANCO);
                ug_texto(v, x0 + 24, yy, pop_items[pop_ini + r],
                         C_BLANCO);
            }
            if (pop_n > filas)
                ug_texto(v, x0 + w0 - 40, yb + 2, "...", C_GRIS);
        }
    }
    (void)i;
    ug_fin(v);
}

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0;
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    ent[0] = 0;
    if (ug_abre(&v, srv, 480, 320, "terminal")) return 98;
    ug_edit(&v, 1);   /* TAB y ESC para autocompletar */
    pon("Terminal FacilOS32 (escribe ayuda)");
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        int i;
        if (t == UG_EV_SALIR) return 0;
        if (t == UG_EV_TECLA) {
            int k = ev.tecla;
            /* v2.2-editor: teclas con modificador del driver:
               ctrl+letra y ctrl+retroceso se ignoran aqui (la
               terminal no usa atajos); may+especial = base. */
            if ((k >= 0xE100 && k <= 0xE219) || k == 0x7F) continue;
            if ((k >= 0x301 && k <= 0x30A) ||
                (k >= 0x501 && k <= 0x50A) ||
                (k >= 0x701 && k <= 0x70A))
                k &= 0x1FF;
            if (pop_abierto) {
                if (k == TEC_ARRIBA_U) {
                    if (pop_sel > 0) pop_sel--;
                    continue;
                } else if (k == TEC_ABAJO_U) {
                    if (pop_sel < pop_n - 1) pop_sel++;
                    continue;
                } else if (k == 13) {
                    if (pop_n > 0) pop_aplica();
                    else pop_abierto = 0;
                    continue;
                } else if (k == 27) {
                    pop_abierto = 0;
                    continue;
                } else if (k == 9) {
                    if (pop_n > 1) {
                        pop_sel++;
                        if (pop_sel >= pop_n) pop_sel = 0;
                    }
                    continue;
                } else if (k == 8 || k == TEC_IZQ_U || k == TEC_DER_U ||
                           k == TEC_SUPR_U || k == TEC_INICIO_U ||
                           k == TEC_FIN_U) {
                    pop_abierto = 0;
                } else if (k >= 32) {
                    pop_abierto = 0;
                } else {
                    continue;
                }
            }
            if (k == 13) {
                char eco[NENTB + 16];
                int e = 0;
                const char *pr = "FacilOS32> ";
                while (*pr && e < NENTB + 14) eco[e++] = *pr++;
                for (i = 0; i < nent && e < NENTB + 14; i++)
                    eco[e++] = ent[i];
                eco[e] = 0;
                pon(eco);
                hist_guarda_ent();
                {
                    char ln[NENTB + 1];
                    for (i = 0; i <= nent; i++) ln[i] = ent[i];
                    ejecuta(&v, ln);
                }
                ent[0] = 0;
                nent = 0;
                pos = 0;
                hay_borrador = 0;
            } else if (k == 8) {
                borra_antes();
            } else if (k == TEC_SUPR_U) {
                borra_en();
            } else if (k == TEC_IZQ_U) {
                pos = ut_anterior(ent, pos);
            } else if (k == TEC_DER_U) {
                pos = ut_siguiente(ent, pos);
                if (pos > nent) pos = nent;
            } else if (k == TEC_INICIO_U) {
                pos = 0;
            } else if (k == TEC_FIN_U) {
                pos = nent;
            } else if (k == TEC_ARRIBA_U) {
                if (hh > 0) {
                    if (hh == nhist && !hay_borrador) {
                        u_strcpy(borrador, ent);
                        hay_borrador = 1;
                    }
                    hh--;
                    u_strcpy(ent, hist[hh]);
                    nent = u_strlen(ent);
                    pos = nent;
                }
            } else if (k == TEC_ABAJO_U) {
                if (hh < nhist) {
                    hh++;
                    if (hh == nhist) {
                        if (hay_borrador) {
                            u_strcpy(ent, borrador);
                            hay_borrador = 0;
                        } else {
                            ent[0] = 0;
                        }
                        nent = u_strlen(ent);
                        pos = nent;
                    } else {
                        hist_recupera();
                    }
                }
            } else if (k == TEC_REPAG_U) {
                /* Sube una pagina del historial de salida. */
                desplaza += vis_lineas(&v);
            } else if (k == TEC_AVPA_U) {
                /* Baja hacia lo mas reciente. */
                desplaza -= vis_lineas(&v);
                if (desplaza < 0) desplaza = 0;
            } else if (k == 9) {
                pop_abre();
            } else if (k == 27) {
                ent[0] = 0;
                nent = 0;
                pos = 0;
            } else if (k >= 32 && k != 0x7F &&
                       !(k >= 0x101 && k <= 0x10A) &&
                       !(k >= 0xE000 && k <= 0xE004) &&
                       !(k >= 0xE100 && k <= 0xE219) &&
                       !(k >= 0x300 && k <= 0x7FF)) {
                inserta_cp((unsigned)k);
            }
        } else if (t == UG_EV_RATON) {
            /* Rueda: desplaza el historial (3 lineas por paso). */
            if (ev.rueda > 0) desplaza += ev.rueda * 3;
            else if (ev.rueda < 0) {
                desplaza += ev.rueda * 3;
                if (desplaza < 0) desplaza = 0;
            }
        } else if (t == UG_EV_DRAW) {
            dibuja_term(&v);
        } else if (t == UG_EV_FOCO) {
            /* el cursor se dibuja segun foco en DRAW */
        }
        (void)i;
    }
}

/* ---- autocompletado visual TAB (v2.2) ---- */

static int empieza_con(const char *s, const char *pref) {
    int i = 0;
    while (pref[i] && s[i] == pref[i]) i++;
    return !pref[i];
}

/* Construye el popup segun ent[0..pos]. Devuelve nº° items (0=cierra). */
static int pop_construye(void) {
    int ts = pos, ti = 0, i;
    char tok[128];
    int tl = 0;
    while (ts > 0 && ent[ts - 1] != ' ' && ent[ts - 1] != '\t') ts--;
    /* indice del token: cuenta separadores antes de ts */
    for (i = 0; i < ts; i++) {
        if ((ent[i] == ' ' || ent[i] == '\t') &&
            (i + 1 >= ts || (ent[i + 1] != ' ' && ent[i + 1] != '\t')))
            ti++;
    }
    while (tl < 127 && ts + tl < pos) {
        tok[tl] = ent[ts + tl];
        tl++;
    }
    tok[tl] = 0;
    /* quita comilla inicial para el prefijo */
    {
        char pref[128];
        int p = 0, q = (tok[0] == '"') ? 1 : 0;
        while (tok[q] && p < 127) pref[p++] = tok[q++];
        pref[p] = 0;
        if (ti == 0) {
            int n = 0;
            u_strcpy(pop_titulo, "COMANDOS");
            for (i = 0; i < NCMDS && n < NPOP; i++) {
                if (!pref[0] || empieza_con(CMDS[i].nombre, pref)) {
                    u_strcpy(pop_items[n++], CMDS[i].nombre);
                }
            }
            return n;
        }
        /* comando de la linea */
        {
            char cn[32];
            int c = 0, wskip = 0;
            while (ent[wskip] == ' ' || ent[wskip] == '\t') wskip++;
            while (ent[wskip] && ent[wskip] != ' ' && ent[wskip] != '\t' &&
                   c < 31)
                cn[c++] = ent[wskip++];
            cn[c] = 0;
            {
                int id = busca_cmd(cn);
                int eq = -1;
                for (i = 0; pref[i]; i++) {
                    if (pref[i] == '=') eq = i;
                }
                if (eq >= 0) {
                    /* nombre=valor: completa valores o nada */
                    char nn2[32];
                    int k = 0;
                    while (k < eq && k < 31) {
                        nn2[k] = pref[k];
                        k++;
                    }
                    nn2[k] = 0;
                    if (id >= 0) {
                        int j;
                        for (j = 0; j < CMDS[id].npars; j++) {
                            if (!u_strcmp(CMDS[id].pars[j].nombre, nn2)) {
                                const char *vv = CMDS[id].pars[j].valores;
                                const char *vpref = pref + eq + 1;
                                int n = 0;
                                u_strcpy(pop_titulo, "VALORES");
                                while (*vv && n < NPOP) {
                                    int w = 0;
                                    char it[32];
                                    while (*vv && *vv != '|' && w < 31)
                                        it[w++] = *vv++;
                                    it[w] = 0;
                                    if (*vv == '|') vv++;
                                    if (empieza_con(it, vpref))
                                        u_strcpy(pop_items[n++], it);
                                }
                                return n;
                            }
                        }
                    }
                    return 0;
                }
                /* ¿es prefijo de parametro (letras, sin / ni espacios)? */
                {
                    int verspre = 1, k;
                    for (k = 0; pref[k]; k++) {
                        char ch = pref[k];
                        if (!(ch == '_' || (ch >= 'a' && ch <= 'z'))) {
                            verspre = 0;
                            break;
                        }
                    }
                    if (verspre && id >= 0 && CMDS[id].npars > 0) {
                        int n = 0, j;
                        u_strcpy(pop_titulo, "OPCIONES");
                        for (j = 0; j < CMDS[id].npars && n < NPOP; j++) {
                            if (empieza_con(CMDS[id].pars[j].nombre,
                                            pref)) {
                                int t = 0, w = 0;
                                while (CMDS[id].pars[j].nombre[w] &&
                                       t < 60) {
                                    pop_items[n][t] =
                                        CMDS[id].pars[j].nombre[w];
                                    t++;
                                    w++;
                                }
                                pop_items[n][t++] = '=';
                                pop_items[n][t] = 0;
                                n++;
                            }
                        }
                        if (n) return n;
                    }
                }
                if (id < 0) return 0;
                /* manual/ayuda completan comandos */
                if (!u_strcmp(cn, "manual") || !u_strcmp(cn, "ayuda")) {
                    int n = 0;
                    u_strcpy(pop_titulo, "COMANDOS");
                    for (i = 0; i < NCMDS && n < NPOP; i++) {
                        if (empieza_con(CMDS[i].nombre, pref))
                            u_strcpy(pop_items[n++], CMDS[i].nombre);
                    }
                    return n;
                }
                /* rutas: separa dir/prefijo por ultimo / */
                {
                    int slash = -1, k;
                    for (k = 0; pref[k]; k++) {
                        if (pref[k] == '/') slash = k;
                    }
                    if (slash >= 0) {
                        char part[128];
                        char pf[64];
                        int dd;
                        char bb[32];
                        for (k = 0; k <= slash && k < 127; k++)
                            part[k] = pref[k];
                        part[k] = 0;
                        {
                            int w = 0;
                            k = slash + 1;
                            while (pref[k] && w < 63) pf[w++] = pref[k++];
                            pf[w] = 0;
                        }
                        if (resuelve(part, &dd, bb)) return 0;
                        {
                            /* part es dir (acaba en /): lista ahi */
                            int ddx = dd, t2 = 0;
                            if (bb[0]) {
                                int ix = busca_en(dd, bb, &t2);
                                if (ix < 0 || t2 != 1) return 0;
                                ddx = ix;
                            }
                            u_strcpy(pop_titulo, "RUTA");
                            {
                                int nn2 = 0, kk = 0, n2 = 0;
                                udir_t d2;
                                int lp = 0;
                                while (pf[lp]) lp++;
                                while (nn2 < NPOP &&
                                       lista_hijo(ddx, kk, &d2) >= 0) {
                                    int m = 0;
                                    while (m < lp && d2.nombre[m] == pf[m])
                                        m++;
                                    if (m == lp) {
                                        int t = 0;
                                        for (k = 0; k <= slash && t < 40; k++)
                                            pop_items[nn2][t++] = pref[k];
                                        {
                                            int w = 0;
                                            while (d2.nombre[w] && t < 60) {
                                                pop_items[nn2][t++] =
                                                    d2.nombre[w++];
                                            }
                                        }
                                        if (d2.tipo == 1 && t < 62)
                                            pop_items[nn2][t++] = '/';
                                        pop_items[nn2][t] = 0;
                                        nn2++;
                                    }
                                    kk++;
                                    if (kk > 200) break;
                                }
                                (void)n2;
                                return nn2;
                            }
                        }
                    }
                    /* sin /: completa en el dir del contexto */
                    {
                        /* para el primer posicional de comandos de
                           archivo/carpeta, o valores de ruta */
                        int dctx = cwd_idx;
                        u_strcpy(pop_titulo, "RUTA");
                        {
                            int nn2 = 0, kk = 0;
                            udir_t d2;
                            int lp = 0;
                            while (pref[lp]) lp++;
                            while (nn2 < NPOP &&
                                   lista_hijo(dctx, kk, &d2) >= 0) {
                                int m = 0;
                                while (m < lp && d2.nombre[m] == pref[m])
                                    m++;
                                if (m == lp) {
                                    int t = 0;
                                    while (d2.nombre[t] && t < 60) {
                                        pop_items[nn2][t] = d2.nombre[t];
                                        t++;
                                    }
                                    if (d2.tipo == 1 && t < 62)
                                        pop_items[nn2][t++] = '/';
                                    pop_items[nn2][t] = 0;
                                    nn2++;
                                }
                                kk++;
                                if (kk > 200) break;
                            }
                            return nn2;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/* ---- despacho ---- */

static void ejecuta(ug_win_t *v, const char *ln) {
    int id, bad = 0, mal = 0, motivo = 0;
    char cmd[32];
    int i = 0;
    v_term = v;
    while (ln[i] == ' ') i++;
    {
        int j = 0;
        while (ln[i] && ln[i] != ' ' && j < 31) cmd[j++] = ln[i++];
        cmd[j] = 0;
    }
    if (!cmd[0]) return;
    id = busca_cmd(cmd);
    if (id < 0) {
        char sug[4][32];
        int n = sugiere(cmd, sug), k;
        char b[96];
        perr(40, "No se reconoce el comando.");
        if (n) {
            pon("Quisiste decir:");
            for (k = 0; k < n; k++) {
                int t = 0, q = 0;
                b[t++] = '-';
                b[t++] = ' ';
                while (sug[k][q] && t < 90) b[t++] = sug[k][q++];
                b[t] = 0;
                pon(b);
            }
        } else {
            pon("Prueba con ayuda.");
        }
        return;
    }
    {
        int r = trocea(ln);
        if (r == -1) {
            perr(41, "Comillas sin cerrar.");
            return;
        }
        if (r == -2) {
            perr(42, "Demasiados argumentos (m\xc3\xa1ximo 11).");
            return;
        }
    }
    {
        int r = separa(&bad);
        if (r == 1) {
            perr(43, "Par\xc3\xa1metro sin nombre antes del =.");
            return;
        }
        if (r == 2) {
            perr(44, "Par\xc3\xa1metro duplicado.");
            return;
        }
        if (r == 3) {
            perr(42, "Demasiados argumentos.");
            return;
        }
    }
    if (npos < CMDS[id].min_pos) {
        perr(10, "Faltan argumentos.");
        pon("Uso:");
        pon(CMDS[id].uso);
        pon("Ejemplo:");
        pon(CMDS[id].ejemplo);
        return;
    }
    if (npos > CMDS[id].max_pos) {
        perr(45, "Sobran argumentos.");
        pon("Uso:");
        pon(CMDS[id].uso);
        return;
    }
    if (valida_pars(&CMDS[id], &mal, &motivo)) {
        if (motivo == 1) {
            char b[96];
            int t = 0, q = 0;
            const char *s = "Par\xc3\xa1metro desconocido: ";
            while (*s && t < 80) b[t++] = *s++;
            while (par_nom[mal][q] && t < 90) b[t++] = par_nom[mal][q++];
            b[t] = 0;
            perr(46, b);
            pon("Uso:");
            pon(CMDS[id].uso);
        } else {
            char b[96];
            int t = 0, q = 0;
            const char *s = "Valor no admitido en ";
            while (*s && t < 70) b[t++] = *s++;
            while (par_nom[mal][q] && t < 90) b[t++] = par_nom[mal][q++];
            b[t] = 0;
            perr(47, b);
        }
        return;
    }
    if (id == 0) {
        if (npos) cmd_ayuda(pos_arg[0]);
        else cmd_ayuda(0);
    } else if (id == 1) {
        cmd_manual(pos_arg[0]);
    } else if (!u_strcmp(cmd, "crear_carpeta")) {
        op_crear_carpeta(&CMDS[id]);
    } else if (!u_strcmp(cmd, "crear_archivo")) {
        op_crear_archivo(&CMDS[id]);
    } else if (!u_strcmp(cmd, "eliminar_archivo")) {
        op_eliminar(&CMDS[id], 0);
    } else if (!u_strcmp(cmd, "eliminar_carpeta")) {
        op_eliminar(&CMDS[id], 1);
    } else if (!u_strcmp(cmd, "listar_archivos")) {
        op_listar(&CMDS[id]);
    } else if (!u_strcmp(cmd, "entrar_carpeta")) {
        op_entrar(&CMDS[id]);
    } else if (!u_strcmp(cmd, "mostrar_ruta")) {
        op_ruta(&CMDS[id]);
    } else if (!u_strcmp(cmd, "mostrar_contenido")) {
        op_ver(&CMDS[id]);
    } else if (!u_strcmp(cmd, "copiar_archivo")) {
        op_copiar(&CMDS[id]);
    } else if (!u_strcmp(cmd, "mover_archivo")) {
        op_mover(&CMDS[id]);
    } else if (!u_strcmp(cmd, "cambiar_nombre")) {
        op_renombrar(&CMDS[id]);
    } else if (!u_strcmp(cmd, "limpiar_pantalla")) {
        op_limpiar(&CMDS[id]);
    } else if (!u_strcmp(cmd, "informacion_sistema")) {
        op_info(&CMDS[id], v);
    } else if (!u_strcmp(cmd, "mostrar_fecha")) {
        op_fecha(&CMDS[id]);
    } else if (!u_strcmp(cmd, "mostrar_hora")) {
        op_hora(&CMDS[id]);
    } else if (!u_strcmp(cmd, "teclado")) {
        op_teclado(&CMDS[id]);
    } else if (!u_strcmp(cmd, "historial")) {
        op_historial(&CMDS[id]);
    } else if (!u_strcmp(cmd, "eco")) {
        op_eco(&CMDS[id]);
    } else if (!u_strcmp(cmd, "ejecutar")) {
        op_ejecutar(v, &CMDS[id]);
    } else if (!u_strcmp(cmd, "red")) {
        op_red(&CMDS[id]);
    } else if (!u_strcmp(cmd, "mostrar_interfaz")) {
        op_mostrar_interfaz(&CMDS[id]);
    } else if (!u_strcmp(cmd, "configurar_ip")) {
        op_configurar_ip(&CMDS[id]);
    } else if (!u_strcmp(cmd, "mostrar_rutas")) {
        op_mostrar_rutas(&CMDS[id]);
    } else if (!u_strcmp(cmd, "comprovar_conexion")) {
        op_comprovar_conexion(&CMDS[id]);
    } else if (!u_strcmp(cmd, "configurar_dhcp")) {
        op_configurar_dhcp(&CMDS[id]);
    } else if (!u_strcmp(cmd, "salir")) {
        ug_cierra(v);
    }
    (void)i;
}
