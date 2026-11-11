#include "ulib.h"
#include "utext.h"

#ifndef F32_LIB
void __main(void) {
}
#endif

/* F32 Lang v0.1: interprete Ring3 (lexer, parser, AST, walker).
 * Uso: f32 programa.f32 | f32 -t programa.f32 | f32 -p programa.f32
 * Salida: 0 ok, 1 error del programa, 2 mal uso/fichero. */

#define MAXSRC 32768
#define MAXLIN 4096
#define MAXTOK 8192
#define MAXNOD 4096
#define MAXSTR 512
#define MAXVAR 256
#define MAXLOC 64
#define MAXFUN 64
#define MAXPAR 8
#define MAXLLAM 64
#define MAXNID 32
#define MAXITER 100000
#define MAXARR 256            /* elementos por arreglo */
#define MAXCAMPO 32           /* campos por registro */
#define ARENAMAX 393216

/* ---- valores ---- */
#define V_INT 1
#define V_STR 2
#define V_BOL 3
#define V_ARR 4               /* arreglo: n elementos en pdato */
#define V_REG 5               /* registro: n campos en pdato */
typedef struct {
    int tipo;
    int num;
    char *txt;
    int n;                    /* ARR/REG: elementos */
    void *pdato;              /* ARR: val_t[]; REG: reg_campo_t[] */
} val_t;
typedef struct {
    char nombre[64];
    val_t v;
} reg_campo_t;

/* ---- error ---- */
static int ERR = 0, ERRL = 0;
static char ARCHIVO[128];
static void falla(int cod, int lin) {
    if (!ERR) { ERR = cod; ERRL = lin; }
}

/* Captura Ring3 para la terminal en ventana (v0.1): sin pipes en la
   ABI, el hijo modal hereda consola y su u_print no entra en la
   ventana. Se duplica toda salida propia a /f32sal.tmp; la terminal
   la vuelca con pon() tras u_ejecutar. Fallo de apertura = solo
   consola (degradado seguro). */
#define SAL_TMP "/f32sal.tmp"
static UFILE *FSAL = 0;
static void emit(const char *s) {
    u_print(s);
    if (FSAL) u_fwrite(s, u_strlen(s), FSAL);
}
static void emit_num(int v) {
    char nb[12];
    u_itoa(v, nb);
    emit(nb);
}
static void cierra_sal(void) {
    if (FSAL) { u_fclose(FSAL); FSAL = 0; }
}

/* ---- arena ---- */
typedef struct abloq {
    struct abloq *sig;
    int usado;
    char datos[4000];
} abloq_t;
static abloq_t *araiz = 0;
static int atotal = 0;
static void *areserva(int n) {
    abloq_t *b;
    n = (n + 3) & ~3;
    if (n > 4000) { falla(90, 0); return 0; }
    if (atotal + n > ARENAMAX) { falla(90, 0); return 0; }
    for (b = araiz; b; b = b->sig) {
        if (4000 - b->usado >= n) {
            void *p = b->datos + b->usado;
            b->usado += n;
            atotal += n;
            return p;
        }
    }
    b = (abloq_t *)u_malloc(sizeof(abloq_t));
    if (!b) { falla(90, 0); return 0; }
    b->sig = araiz;
    b->usado = 0;
    araiz = b;
    {
        void *p = b->datos;
        b->usado = n;
        atotal += n;
        return p;
    }
}
static void alibera(void) {
    abloq_t *b = araiz, *s;
    while (b) { s = b->sig; u_free(b); b = s; }
    araiz = 0;
    atotal = 0;
}
static char *adup(const char *s, int n) {
    char *d = (char *)areserva(n + 1);
    int i;
    if (!d) return 0;
    for (i = 0; i < n; i++) d[i] = s[i];
    d[n] = 0;
    return d;
}

/* ---- fuente por lineas ---- */
static char *SRC = 0;
static int SRCL = 0, NLIN = 0;
static int *LINI = 0, *LNL = 0, *LIND = 0;

static int es_idini(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int es_idc(char c) {
    return es_idini(c) || (c >= '0' && c <= '9');
}

static int carga(UFILE *f) {
    char *tmp;
    int n, tot = 0;
    tmp = (char *)u_malloc(MAXSRC + 1);
    if (!tmp) { falla(90, 0); return -1; }
    while (tot <= MAXSRC && (n = u_fread(tmp + tot, MAXSRC + 1 - tot, f)) > 0)
        tot += n;
    if (tot > MAXSRC) { u_free(tmp); falla(2, 0); return -1; }
    SRC = tmp;
    SRCL = tot;
    return 0;
}

static int parte_lineas(void) {
    int p = 0, l = 0;
    while (p < SRCL && l < MAXLIN) {
        int ini = p, ind = 0;
        while (p < SRCL && (SRC[p] == ' ' || SRC[p] == '\t')) {
            if (SRC[p] == '\t') { falla(22, l + 1); return -1; }
            ind++;
            p++;
        }
        if (ind % 4) { falla(22, l + 1); return -1; }
        {
            int f = p;
            while (f < SRCL && SRC[f] != '\n') f++;
            /* tolera CRLF: el \r no forma parte de la linea */
            if (f > p && SRC[f - 1] == '\r') {
                LINI[l] = ini;
                LNL[l] = f - 1 - ini;
                LIND[l] = ind;
                l++;
                p = f + 1;
                continue;
            }
            LINI[l] = ini;
            LNL[l] = f - ini;
            LIND[l] = ind;
            l++;
            p = f < SRCL ? f + 1 : f;
        }
    }
    if (p < SRCL) { falla(3, 0); return -1; }
    NLIN = l;
    return 0;
}

/* linea vacia o solo comentario */
static int lin_vacia(int l) {
    int p = LINI[l] + LIND[l], f = LINI[l] + LNL[l];
    if (p < f && SRC[p] == '#') return 1;
    return p >= f;
}

/* ---- tokens ---- */
#define T_FIN 0
#define T_ID 1
#define T_ENT 2
#define T_CAD 3
#define T_RES 4
#define T_OP 5
/* ops: + - * / % = == != < <= > >= ( ) , . [ ] */
#define O_MAS 1
#define O_MEN 2
#define O_POR 3
#define O_DIV 4
#define O_MOD 5
#define O_ASIG 6
#define O_EQ 7
#define O_NE 8
#define O_LT 9
#define O_LE 10
#define O_GT 11
#define O_GE 12
#define O_PAR 13
#define O_CPA 14
#define O_COM 15
#define O_PUN 16
#define O_COR 17
#define O_CCA 18
#define O_DOS 19
/* reservadas */
#define R_PROGRAMA 1
#define R_FIN 2
#define R_ESCRIBIR 3
#define R_LEER 4
#define R_VARIABLE 5
#define R_SI 6
#define R_SINOSI 7
#define R_SINO 8
#define R_FINSI 9
#define R_MIENTRAS 10
#define R_FINMIENTRAS 11
#define R_FUNCION 12
#define R_FINFUNCION 13
#define R_RETORNAR 14
#define R_Y 15
#define R_O 16
#define R_NO 17
#define R_VERDAD 18
#define R_FALSO 19
#define R_IMPORTAR 20
#define R_PARA 21
#define R_DESDE 22
#define R_HASTA 23
#define R_PASO 24
#define R_FINPARA 25

typedef struct {
    int tipo;
    int lin;
    int ival;
    char *sval;
} tok_t;
static tok_t *TOKS = 0;
static int NTOK = 0;

static int res_de(const char *s, int n) {
    static const char *noms[] = {
        "programa", "fin", "escribir", "leer", "variable", "si",
        "sinosi", "sino", "finsi", "mientras", "finmientras",
        "funcion", "finfuncion", "retornar", "y", "o", "no",
        "verdadero", "falso", "importar", "para", "desde",
        "hasta", "paso", "finpara", 0
    };
    int i = 0, j;
    while (noms[i]) {
        int m = 0;
        while (noms[i][m]) m++;
        if (m == n) {
            int ok = 1;
            for (j = 0; j < n; j++) {
                char a = s[j], b = noms[i][j];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (a != b) { ok = 0; break; }
            }
            if (ok) return i + 1;
        }
        i++;
    }
    return 0;
}

static int mete_tok(int tipo, int lin, int ival, char *sval) {
    if (NTOK >= MAXTOK) { falla(91, lin); return -1; }
    TOKS[NTOK].tipo = tipo;
    TOKS[NTOK].lin = lin;
    TOKS[NTOK].ival = ival;
    TOKS[NTOK].sval = sval;
    NTOK++;
    return 0;
}

/* tokeniza la linea l (1-based en errores) */
static int lex_lin(int l) {
    int p = LINI[l] + LIND[l], f = LINI[l] + LNL[l], ln = l + 1;
    if (p < f && SRC[p] == '#') return 0;
    while (p < f) {
        char c = SRC[p];
        if (c == ' ' || c == '\t') { p++; continue; }
        if (c == '#') break;
        if (es_idini(c)) {
            int i0 = p, r;
            while (p < f && es_idc(SRC[p])) p++;
            r = res_de(SRC + i0, p - i0);
            if (r) {
                if (mete_tok(T_RES, ln, r, 0)) return -1;
            } else {
                char *s = adup(SRC + i0, p - i0);
                int k;
                if (!s) return -1;
                /* mayusculas en identificador no reservado */
                for (k = 0; s[k]; k++) {
                    if (s[k] >= 'A' && s[k] <= 'Z') {
                        /* se admite pero se avisa? no: valido */
                    }
                }
                if (mete_tok(T_ID, ln, 0, s)) return -1;
            }
            continue;
        }
        if (c >= '0' && c <= '9') {
            unsigned int v = 0;
            while (p < f && SRC[p] >= '0' && SRC[p] <= '9') {
                v = v * 10 + (unsigned)(SRC[p] - '0');
                if (v > 0x7FFFFFFFu) { falla(31, ln); return -1; }
                p++;
            }
            if (p < f && es_idc(SRC[p])) { falla(10, ln); return -1; }
            if (mete_tok(T_ENT, ln, (int)v, 0)) return -1;
            continue;
        }
        if (c == '"') {
            int i0 = ++p, ok = 0;
            while (p < f) {
                if (SRC[p] == '"') { ok = 1; break; }
                p++;
            }
            if (!ok) { falla(30, ln); return -1; }
            if (!ut_valido(SRC + i0, p - i0)) { falla(32, ln); return -1; }
            {
                char *s = adup(SRC + i0, p - i0);
                if (!s) return -1;
                if (mete_tok(T_CAD, ln, 0, s)) return -1;
            }
            p++;
            continue;
        }
        if (c == '=' || c == '!' || c == '<' || c == '>') {
            if (p + 1 < f && SRC[p + 1] == '=') {
                int o = c == '=' ? O_EQ : c == '!' ? O_NE : c == '<' ? O_LE
                                                                     : O_GE;
                if (c == '!') {
                    /* != valido; ! solo no */
                }
                p += 2;
                if (mete_tok(T_OP, ln, o, 0)) return -1;
                continue;
            }
            if (c == '!') { falla(10, ln); return -1; }
            {
                int o = c == '=' ? O_ASIG : c == '<' ? O_LT : O_GT;
                p++;
                if (mete_tok(T_OP, ln, o, 0)) return -1;
                continue;
            }
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '(' || c == ')' || c == ',') {
            int o = c == '+' ? O_MAS : c == '-' ? O_MEN : c == '*' ? O_POR
                 : c == '/' ? O_DIV : c == '%' ? O_MOD : c == '(' ? O_PAR
                 : c == ')' ? O_CPA
                            : O_COM;
            p++;
            if (mete_tok(T_OP, ln, o, 0)) return -1;
            continue;
        }
        if (c == '[' || c == ']') {
            p++;
            if (mete_tok(T_OP, ln, c == '[' ? O_COR : O_CCA, 0))
                return -1;
            continue;
        }
        if (c == ':') {
            p++;
            if (mete_tok(T_OP, ln, O_DOS, 0)) return -1;
            continue;
        }
        if (c == '.') {
            /* Separador modulo.miembro (0.2): los numeros son
               enteros, el punto nunca forma parte de un literal. */
            p++;
            if (mete_tok(T_OP, ln, O_PUN, 0)) return -1;
            continue;
        }
        falla(10, ln);
        return -1;
    }
    return 0;
}

static int lex_todo(void) {
    int l;
    NTOK = 0;
    for (l = 0; l < NLIN; l++) {
        if (lin_vacia(l)) continue;
        if (lex_lin(l)) return -1;
    }
    return 0;
}

/* ---- AST ---- */
#define N_BLOQ 1
#define N_VAR 2
#define N_ASIG 3
#define N_PRINT 4
#define N_READ 5
#define N_IF 6
#define N_WHILE 7
#define N_DEF 8
#define N_CALL 9
#define N_RET 10
#define N_BIN 11
#define N_UN 12
#define N_LIT 13
#define N_REF 14
#define N_PROG 15
#define N_IMP 16
#define N_ARR 17           /* literal [e, ...] */
#define N_IDX 18          /* base[indice]: h = base, indice */
#define N_ASI 19          /* base[indice] = valor: h = base, indice, valor */
#define N_REG 20          /* registro [a: v, ...]: h = claveLIT, valor, ... */
#define N_PARA 21         /* para var desde a hasta b [paso s]: h = a, b, s?, cuerpo */

typedef struct nodo {
    int tipo;
    int lin;
    int n1, n2;
    char *s;
    struct nodo **h;
    int nh;
} nodo_t;

static nodo_t *nuevo_n(int tipo, int lin) {
    nodo_t *n = (nodo_t *)areserva(sizeof(nodo_t));
    if (!n) return 0;
    n->tipo = tipo;
    n->lin = lin;
    n->n1 = n->n2 = 0;
    n->s = 0;
    n->h = 0;
    n->nh = 0;
    return n;
}

static int pon_h(nodo_t *p, nodo_t *c) {
    nodo_t **nv;
    int i;
    if (!p || !c) { falla(91, p ? p->lin : 0); return -1; }
    nv = (nodo_t **)areserva(sizeof(nodo_t *) * (p->nh + 1));
    if (!nv) return -1;
    for (i = 0; i < p->nh; i++) nv[i] = p->h[i];
    nv[p->nh] = c;
    p->h = nv;
    p->nh++;
    return 0;
}

/* parser sobre lineas: agrupa tokens por linea */
static int *LTINI = 0, *LTN = 0;
static int POS = 0;

static void agrupa(void) {
    int i, l = -1;
    for (i = 0; i < MAXLIN; i++) { LTINI[i] = 0; LTN[i] = 0; }
    for (i = 0; i < NTOK; i++) {
        int ln = TOKS[i].lin - 1;
        if (ln != l) {
            l = ln;
            LTINI[l] = i;
        }
        LTN[l]++;
    }
}

static int nodos_usados = 0;
static nodo_t *haz(int tipo, int lin) {
    nodo_t *n;
    if (nodos_usados >= MAXNOD) { falla(91, lin); return 0; }
    n = nuevo_n(tipo, lin);
    if (n) nodos_usados++;
    return n;
}

/* forward */
static nodo_t *p_expr(void);
static nodo_t *p_cmp(void);

static int tk_es(int t, int r) {
    return POS < NTOK && TOKS[POS].tipo == t &&
           (r < 0 || TOKS[POS].ival == r);
}

/* Lee ID (. ID)* en buf (cap, con NUL). Definida junto a p_llamada. */
static int lee_punteado(char *buf, int cap, int lin);

/* Sufijos [indice] sobre una base ya parseada (arreglos y
   cadenas). Devuelve el nodo envuelto o 0 con ERR. */
static nodo_t *p_sufijo(nodo_t *n, int ln) {
    while (tk_es(T_OP, O_COR)) {
        nodo_t *ni, *ix;
        POS++;
        ix = p_expr();
        if (ERR || !ix) return 0;
        if (!tk_es(T_OP, O_CCA)) { falla(33, ln); return 0; }
        POS++;
        ni = haz(N_IDX, ln);
        if (!ni) return 0;
        if (pon_h(ni, n) || pon_h(ni, ix)) return 0;
        n = ni;
    }
    return n;
}

static nodo_t *p_prim(void) {
    int ln;
    if (POS >= NTOK) { falla(33, 0); return 0; }
    ln = TOKS[POS].lin;
    if (TOKS[POS].tipo == T_ENT) {
        nodo_t *n = haz(N_LIT, ln);
        if (!n) return 0;
        n->n1 = V_INT;
        n->n2 = TOKS[POS].ival;
        POS++;
        return n;
    }
    if (TOKS[POS].tipo == T_CAD) {
        nodo_t *n = haz(N_LIT, ln);
        if (!n) return 0;
        n->n1 = V_STR;
        n->s = TOKS[POS].sval;
        POS++;
        return n;
    }
    if (TOKS[POS].tipo == T_RES &&
        (TOKS[POS].ival == R_VERDAD || TOKS[POS].ival == R_FALSO)) {
        nodo_t *n = haz(N_LIT, ln);
        if (!n) return 0;
        n->n1 = V_BOL;
        n->n2 = TOKS[POS].ival == R_VERDAD ? 1 : 0;
        POS++;
        return n;
    }
    if (tk_es(T_OP, O_COR)) {
        /* Literal arreglo [e, ...] o registro [a: v, ...]. */
        nodo_t *n;
        int es_reg = 0;
        POS++;
        if (tk_es(T_OP, O_CCA)) {
            n = haz(N_ARR, ln);
            if (!n) return 0;
            POS++;
            return n;
        }
        if (TOKS[POS].tipo == T_ID) {
            int p1 = POS + 1;
            if (p1 < NTOK && TOKS[p1].tipo == T_OP &&
                TOKS[p1].ival == O_DOS)
                es_reg = 1;
        }
        if (es_reg) {
            n = haz(N_REG, ln);
            if (!n) return 0;
            for (;;) {
                nodo_t *kc, *vc;
                if (POS >= NTOK || TOKS[POS].tipo != T_ID) {
                    falla(33, ln);
                    return 0;
                }
                kc = haz(N_LIT, ln);
                if (!kc) return 0;
                kc->n1 = V_STR;
                kc->s = TOKS[POS].sval;
                POS++;
                if (!tk_es(T_OP, O_DOS)) { falla(33, ln); return 0; }
                POS++;
                vc = p_expr();
                if (ERR || !vc) return 0;
                if (pon_h(n, kc) || pon_h(n, vc)) return 0;
                if (tk_es(T_OP, O_COM)) {
                    POS++;
                    continue;
                }
                break;
            }
        } else {
            n = haz(N_ARR, ln);
            if (!n) return 0;
            for (;;) {
                nodo_t *e = p_expr();
                if (ERR || !e) return 0;
                if (pon_h(n, e)) return 0;
                if (n->nh > MAXARR) { falla(91, ln); return 0; }
                if (tk_es(T_OP, O_COM)) {
                    POS++;
                    continue;
                }
                break;
            }
        }
        if (!tk_es(T_OP, O_CCA)) { falla(33, ln); return 0; }
        POS++;
        return n;
    }
    if (TOKS[POS].tipo == T_ID) {
        char nb[64];
        int l2 = ln, q = 0;
        char *nm;
        if (lee_punteado(nb, (int)sizeof(nb), ln)) return 0;
        while (nb[q]) q++;
        nm = adup(nb, q);
        if (!nm) return 0;
        if (tk_es(T_OP, O_PAR)) {
            /* llamada */
            nodo_t *n = haz(N_CALL, l2);
            nodo_t *a;
            if (!n) return 0;
            n->s = nm;
            POS++;
            if (tk_es(T_OP, O_CPA)) {
                POS++;
                return p_sufijo(n, l2);
            }
            for (;;) {
                a = p_expr();
                if (ERR || !a) return 0;
                if (pon_h(n, a)) return 0;
                if (n->nh > MAXPAR) { falla(84, l2); return 0; }
                if (tk_es(T_OP, O_COM)) {
                    POS++;
                    continue;
                }
                break;
            }
            if (!tk_es(T_OP, O_CPA)) { falla(33, l2); return 0; }
            POS++;
            return p_sufijo(n, l2);
        }
        {
            /* referencia (simple o punteada) */
            nodo_t *n = haz(N_REF, l2);
            if (!n) return 0;
            n->s = nm;
            return p_sufijo(n, l2);
        }
    }
    if (tk_es(T_OP, O_PAR)) {
        nodo_t *e;
        POS++;
        e = p_expr();
        if (ERR || !e) return 0;
        if (!tk_es(T_OP, O_CPA)) { falla(33, ln); return 0; }
        POS++;
        return e;
    }
    falla(33, ln);
    return 0;
}

static nodo_t *p_un(void) {
    int ln;
    if (POS >= NTOK) { falla(33, 0); return 0; }
    ln = TOKS[POS].lin;
    if (tk_es(T_OP, O_MEN)) {
        nodo_t *n, *e;
        POS++;
        e = p_un();
        if (ERR || !e) return 0;
        n = haz(N_UN, ln);
        if (!n) return 0;
        n->n1 = O_MEN;
        if (pon_h(n, e)) return 0;
        return n;
    }
    return p_prim();
}

/* `no` liga menos que las comparaciones (nivel propio entre `y`
   y comparacion): `no a == b` es `no (a == b)`. */
static nodo_t *p_no(void) {
    int ln;
    if (POS < NTOK && TOKS[POS].tipo == T_RES &&
        TOKS[POS].ival == R_NO) {
        nodo_t *n, *e;
        ln = TOKS[POS].lin;
        POS++;
        e = p_no();
        if (ERR || !e) return 0;
        n = haz(N_UN, ln);
        if (!n) return 0;
        n->n1 = R_NO;
        if (pon_h(n, e)) return 0;
        return n;
    }
    return p_cmp();
}

static nodo_t *p_mul(void) {
    nodo_t *e = p_un();
    if (ERR || !e) return 0;
    while (tk_es(T_OP, O_POR) || tk_es(T_OP, O_DIV) ||
           tk_es(T_OP, O_MOD)) {
        int o = TOKS[POS].ival, ln = TOKS[POS].lin;
        nodo_t *n, *d;
        POS++;
        d = p_un();
        if (ERR || !d) return 0;
        n = haz(N_BIN, ln);
        if (!n) return 0;
        n->n1 = o;
        if (pon_h(n, e) || pon_h(n, d)) return 0;
        e = n;
    }
    return e;
}

static nodo_t *p_sum(void) {
    nodo_t *e = p_mul();
    if (ERR || !e) return 0;
    while (tk_es(T_OP, O_MAS) || tk_es(T_OP, O_MEN)) {
        int o = TOKS[POS].ival, ln = TOKS[POS].lin;
        nodo_t *n, *d;
        POS++;
        d = p_mul();
        if (ERR || !d) return 0;
        n = haz(N_BIN, ln);
        if (!n) return 0;
        n->n1 = o;
        if (pon_h(n, e) || pon_h(n, d)) return 0;
        e = n;
    }
    return e;
}

static nodo_t *p_cmp(void) {
    nodo_t *e = p_sum();
    int ln;
    int o;
    nodo_t *n, *d;
    if (ERR || !e) return 0;
    if (!(tk_es(T_OP, O_EQ) || tk_es(T_OP, O_NE) || tk_es(T_OP, O_LT) ||
          tk_es(T_OP, O_LE) || tk_es(T_OP, O_GT) || tk_es(T_OP, O_GE)))
        return e;
    o = TOKS[POS].ival;
    ln = TOKS[POS].lin;
    POS++;
    d = p_sum();
    if (ERR || !d) return 0;
    /* no encadenables */
    if (tk_es(T_OP, O_EQ) || tk_es(T_OP, O_NE) || tk_es(T_OP, O_LT) ||
        tk_es(T_OP, O_LE) || tk_es(T_OP, O_GT) || tk_es(T_OP, O_GE)) {
        falla(34, ln);
        return 0;
    }
    n = haz(N_BIN, ln);
    if (!n) return 0;
    n->n1 = o;
    if (pon_h(n, e) || pon_h(n, d)) return 0;
    return n;
}

static nodo_t *p_y(void) {
    nodo_t *e = p_no();
    if (ERR || !e) return 0;
    while (POS < NTOK && TOKS[POS].tipo == T_RES &&
           TOKS[POS].ival == R_Y) {
        int ln = TOKS[POS].lin;
        nodo_t *n, *d;
        POS++;
        d = p_no();
        if (ERR || !d) return 0;
        n = haz(N_BIN, ln);
        if (!n) return 0;
        n->n1 = R_Y;
        if (pon_h(n, e) || pon_h(n, d)) return 0;
        e = n;
    }
    return e;
}

static nodo_t *p_expr(void) {
    nodo_t *e = p_y();
    if (ERR || !e) return 0;
    while (POS < NTOK && TOKS[POS].tipo == T_RES &&
           TOKS[POS].ival == R_O) {
        int ln = TOKS[POS].lin;
        nodo_t *n, *d;
        POS++;
        d = p_y();
        if (ERR || !d) return 0;
        n = haz(N_BIN, ln);
        if (!n) return 0;
        n->n1 = R_O;
        if (pon_h(n, e) || pon_h(n, d)) return 0;
        e = n;
    }
    return e;
}

/* linea actual y su indent */
static int lin_de_tok(int p) {
    if (p < 0 || p >= NTOK) return 0;
    return TOKS[p].lin - 1;
}

/* parsea una instruccion en la linea l; devuelve linea siguiente */
static int p_inst(int l, nodo_t *bloq, int nivel);

/* cuerpo con indent > ind desde la linea l */
static int p_cuerpo(int l, int ind, nodo_t *padre, int nivel) {
    nodo_t *b;
    if (nivel > MAXNID) { falla(62, l + 1); return -1; }
    b = haz(N_BLOQ, l + 1);
    if (!b) return -1;
    if (l >= NLIN || LIND[l] <= ind) {
        /* cuerpo vacio no permitido */
        falla(20, l + 1);
        return -1;
    }
    while (l < NLIN) {
        if (lin_vacia(l)) { l++; continue; }
        if (LIND[l] <= ind) break;
        l = p_inst(l, b, nivel + 1);
        if (ERR) return -1;
    }
    if (pon_h(padre, b)) return -1;
    return l;
}

static int es_cierre(int r) {
    return r == R_SINOSI || r == R_SINO || r == R_FINSI ||
           r == R_FINMIENTRAS || r == R_FINFUNCION || r == R_FIN;
}

/* primera palabra reservada de la linea l (0 si no) */
static int lin_res(int l) {
    int p;
    if (l < 0 || l >= NLIN) return 0;
    p = LTINI[l];
    if (p < LTINI[l] + LTN[l] && TOKS[p].tipo == T_RES) return TOKS[p].ival;
    return 0;
}

static int sig_lin(int l) {
    l++;
    while (l < NLIN && lin_vacia(l)) l++;
    return l;
}

static int exige_fin_linea(int l) {
    if (POS != LTINI[l] + LTN[l]) { falla(33, l + 1); return -1; }
    return 0;
}

/* definido mas abajo; lo usa ev_expr para llamadas como valor */
int ex_inst(nodo_t *n);
static int ex_bloque(nodo_t *b);

/* Lee ID (. ID)* en buf (cap, con NUL). 0 ok, -1 malformado.
   Avanza POS tras el ultimo ID (sin consumir lo siguiente). */
static int lee_punteado(char *buf, int cap, int lin) {
    int t = 0, k;
    if (cap < 2) { falla(91, lin); return -1; }
    if (POS >= NTOK || TOKS[POS].tipo != T_ID) { falla(33, lin); return -1; }
    for (;;) {
        const char *s = TOKS[POS].sval;
        if (t > 0) {
            if (t + 1 >= cap) { falla(91, lin); return -1; }
            buf[t++] = '.';
        }
        for (k = 0; s[k]; k++) {
            if (t + 1 >= cap) { falla(91, lin); return -1; }
            buf[t++] = s[k];
        }
        POS++;
        if (tk_es(T_OP, O_PUN)) {
            POS++;
            if (POS >= NTOK || TOKS[POS].tipo != T_ID) {
                falla(33, lin);
                return -1;
            }
            continue;
        }
        break;
    }
    buf[t] = 0;
    return t > 0 ? 0 : -1;
}

/* parsea llamada como instruccion: ID (. ID)* ( args ) fin */
static nodo_t *p_llamada(int l) {
    nodo_t *n;
    char nb[64];
    int ln, q = 0;
    nodo_t *a;
    if (POS >= NTOK || TOKS[POS].tipo != T_ID) { falla(33, l + 1); return 0; }
    ln = TOKS[POS].lin;
    if (lee_punteado(nb, (int)sizeof(nb), l + 1)) return 0;
    if (!tk_es(T_OP, O_PAR)) { falla(33, l + 1); return 0; }
    POS++;             /* ( */
    n = haz(N_CALL, ln);
    if (!n) return 0;
    while (nb[q]) q++;
    n->s = adup(nb, q);
    if (!n->s) return 0;
    if (tk_es(T_OP, O_CPA)) {
        POS++;
        return n;
    }
    for (;;) {
        a = p_expr();
        if (ERR || !a) return 0;
        if (pon_h(n, a)) return 0;
        if (n->nh > MAXPAR) { falla(84, ln); return 0; }
        if (tk_es(T_OP, O_COM)) {
            POS++;
            continue;
        }
        break;
    }
    if (!tk_es(T_OP, O_CPA)) { falla(33, ln); return 0; }
    POS++;
    return n;
}

static int p_si(int l, nodo_t *bloq, int nivel) {
    nodo_t *n = haz(N_IF, l + 1);
    nodo_t *c;
    int ind = LIND[l];
    if (!n) return -1;
    POS = LTINI[l] + 1;
    c = p_expr();
    if (ERR || !c) return -1;
    if (pon_h(n, c)) return -1;
    if (exige_fin_linea(l)) return -1;
    l = sig_lin(l);
    l = p_cuerpo(l, ind, n, nivel);
    if (ERR) return -1;
    for (;;) {
        int r;
        l = sig_lin(l - 1);
        if (l >= NLIN) { falla(12, NLIN); return -1; }
        if (LIND[l] != ind) { falla(12, l + 1); return -1; }
        r = lin_res(l);
        if (r == R_SINOSI) {
            POS = LTINI[l] + 1;
            c = p_expr();
            if (ERR || !c) return -1;
            if (pon_h(n, c)) return -1;
            if (exige_fin_linea(l)) return -1;
            l = sig_lin(l);
            l = p_cuerpo(l, ind, n, nivel);
            if (ERR) return -1;
            continue;
        }
        if (r == R_SINO) {
            POS = LTINI[l] + 1;
            if (exige_fin_linea(l)) return -1;
            /* marcador de sino */
            c = haz(N_LIT, l + 1);
            if (!c) return -1;
            c->n1 = V_BOL;
            c->n2 = 1;
            if (pon_h(n, c)) return -1;
            l = sig_lin(l);
            l = p_cuerpo(l, ind, n, nivel);
            if (ERR) return -1;
            l = sig_lin(l - 1);
            if (l >= NLIN || LIND[l] != ind) { falla(12, l + 1); return -1; }
            r = lin_res(l);
            if (r == R_SINOSI || r == R_SINO) { falla(61, l + 1); return -1; }
            if (r != R_FINSI) { falla(12, l + 1); return -1; }
            l++;
            break;
        }
        if (r == R_FINSI) {
            l++;
            break;
        }
        falla(12, l + 1);
        return -1;
    }
    if (pon_h(bloq, n)) return -1;
    return l;
}

static int p_mientras(int l, nodo_t *bloq, int nivel) {
    nodo_t *n = haz(N_WHILE, l + 1);
    nodo_t *c;
    int ind = LIND[l];
    int r;
    if (!n) return -1;
    POS = LTINI[l] + 1;
    c = p_expr();
    if (ERR || !c) return -1;
    if (pon_h(n, c)) return -1;
    if (exige_fin_linea(l)) return -1;
    l = sig_lin(l);
    l = p_cuerpo(l, ind, n, nivel);
    if (ERR) return -1;
    l = sig_lin(l - 1);
    if (l >= NLIN || LIND[l] != ind) { falla(13, l + 1); return -1; }
    r = lin_res(l);
    if (r != R_FINMIENTRAS) { falla(13, l + 1); return -1; }
    l++;
    if (pon_h(bloq, n)) return -1;
    return l;
}

static int p_funcion(int l, nodo_t *prog, int nivel) {
    nodo_t *n;
    int ind = LIND[l];
    int r, np = 0;
    if (nivel > 0) { falla(80, l + 1); return -1; }
    POS = LTINI[l] + 1;
    if (POS >= NTOK || TOKS[POS].tipo != T_ID) { falla(33, l + 1); return -1; }
    n = haz(N_DEF, l + 1);
    if (!n) return -1;
    n->s = TOKS[POS].sval;
    POS++;
    if (tk_es(T_OP, O_PAR)) {
        POS++;
        if (!tk_es(T_OP, O_CPA)) {
            for (;;) {
                if (POS >= NTOK || TOKS[POS].tipo != T_ID) {
                    falla(33, l + 1);
                    return -1;
                }
                {
                    nodo_t *p2 = haz(N_REF, l + 1);
                    if (!p2) return -1;
                    p2->s = TOKS[POS].sval;
                    if (pon_h(n, p2)) return -1;
                    np++;
                    if (np > MAXPAR) { falla(81, l + 1); return -1; }
                }
                POS++;
                if (tk_es(T_OP, O_COM)) {
                    POS++;
                    continue;
                }
                break;
            }
        }
        if (!tk_es(T_OP, O_CPA)) { falla(33, l + 1); return -1; }
        POS++;
    }
    if (exige_fin_linea(l)) return -1;
    l = sig_lin(l);
    l = p_cuerpo(l, ind, n, nivel);
    if (ERR) return -1;
    l = sig_lin(l - 1);
    if (l >= NLIN || LIND[l] != ind) { falla(14, l + 1); return -1; }
    r = lin_res(l);
    if (r != R_FINFUNCION) { falla(14, l + 1); return -1; }
    l++;
    if (pon_h(prog, n)) return -1;
    return l;
}

/* importar a[.b] : modulo .f32 (solo nivel de programa). */
static int p_importar(int l, nodo_t *padre) {
    nodo_t *n;
    char nb[64];
    int q = 0;
    POS = LTINI[l] + 1;
    if (lee_punteado(nb, (int)sizeof(nb), l + 1)) return -1;
    if (exige_fin_linea(l)) return -1;
    n = haz(N_IMP, l + 1);
    if (!n) return -1;
    while (nb[q]) q++;
    n->s = adup(nb, q);
    if (!n->s) return -1;
    if (pon_h(padre, n)) return -1;
    return l + 1;
}

/* para i desde a hasta b [paso s] ... finpara */
static int p_para(int l, nodo_t *bloq, int nivel) {
    nodo_t *n = haz(N_PARA, l + 1);
    nodo_t *e;
    int ind = LIND[l], r;
    char nm[64];
    int q = 0;
    if (!n) return -1;
    POS = LTINI[l] + 1;
    if (POS >= NTOK || TOKS[POS].tipo != T_ID) { falla(33, l + 1); return -1; }
    while (TOKS[POS].sval[q] && q < 63) {
        nm[q] = TOKS[POS].sval[q];
        q++;
    }
    nm[q] = 0;
    n->s = adup(nm, q);
    if (!n->s) return -1;
    POS++;
    if (POS >= NTOK || TOKS[POS].tipo != T_RES ||
        TOKS[POS].ival != R_DESDE) { falla(33, l + 1); return -1; }
    POS++;
    e = p_expr();
    if (ERR || !e) return -1;
    if (pon_h(n, e)) return -1;
    if (POS >= NTOK || TOKS[POS].tipo != T_RES ||
        TOKS[POS].ival != R_HASTA) { falla(33, l + 1); return -1; }
    POS++;
    e = p_expr();
    if (ERR || !e) return -1;
    if (pon_h(n, e)) return -1;
    if (POS < NTOK && TOKS[POS].tipo == T_RES &&
        TOKS[POS].ival == R_PASO) {
        POS++;
        e = p_expr();
        if (ERR || !e) return -1;
    } else {
        /* paso automatico: 0 = segun direccion */
        e = haz(N_LIT, l + 1);
        if (!e) return -1;
        e->n1 = V_INT;
        e->n2 = 0;
    }
    if (pon_h(n, e)) return -1;
    if (exige_fin_linea(l)) return -1;
    l = sig_lin(l);
    l = p_cuerpo(l, ind, n, nivel);
    if (ERR) return -1;
    l = sig_lin(l - 1);
    if (l >= NLIN || LIND[l] != ind) { falla(12, l + 1); return -1; }
    r = lin_res(l);
    if (r != R_FINPARA) { falla(12, l + 1); return -1; }
    l++;
    if (pon_h(bloq, n)) return -1;
    return l;
}

static int p_inst(int l, nodo_t *bloq, int nivel) {
    int r, ind = LIND[l];
    nodo_t *n;
    (void)ind;
    POS = LTINI[l];
    if (POS >= NTOK) { falla(91, l + 1); return -1; }
    if (TOKS[POS].tipo == T_RES) {
        r = TOKS[POS].ival;
        if (r == R_IMPORTAR) {
            /* Solo a nivel de programa (no en bloques/funciones). */
            int q;
            if (nivel > 0) { falla(16, l + 1); return -1; }
            q = p_importar(l, bloq);
            if (ERR || q < 0) return -1;
            return q;
        }
        if (r == R_VARIABLE) {
            nodo_t *e;
            POS++;
            if (POS >= NTOK || TOKS[POS].tipo != T_ID) {
                falla(40, l + 1);
                return -1;
            }
            n = haz(N_VAR, l + 1);
            if (!n) return -1;
            n->s = TOKS[POS].sval;
            POS++;
            if (!tk_es(T_OP, O_ASIG)) { falla(40, l + 1); return -1; }
            POS++;
            e = p_expr();
            if (ERR || !e) return -1;
            if (pon_h(n, e)) return -1;
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, n)) return -1;
            return l + 1;
        }
        if (r == R_ESCRIBIR) {
            POS++;
            n = haz(N_PRINT, l + 1);
            if (!n) return -1;
            if (POS < LTINI[l] + LTN[l]) {
                nodo_t *e = p_expr();
                if (ERR || !e) return -1;
                if (pon_h(n, e)) return -1;
            }
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, n)) return -1;
            return l + 1;
        }
        if (r == R_LEER) {
            POS++;
            if (POS >= NTOK || TOKS[POS].tipo != T_ID) {
                falla(33, l + 1);
                return -1;
            }
            n = haz(N_READ, l + 1);
            if (!n) return -1;
            n->s = TOKS[POS].sval;
            POS++;
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, n)) return -1;
            return l + 1;
        }
        if (r == R_RETORNAR) {
            nodo_t *e;
            POS++;
            n = haz(N_RET, l + 1);
            if (!n) return -1;
            if (POS < LTINI[l] + LTN[l]) {
                e = p_expr();
                if (ERR || !e) return -1;
                if (pon_h(n, e)) return -1;
            }
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, n)) return -1;
            return l + 1;
        }
        if (r == R_SI) return p_si(l, bloq, nivel);
        if (r == R_MIENTRAS) return p_mientras(l, bloq, nivel);
        if (r == R_PARA) return p_para(l, bloq, nivel);
        if (r == R_FUNCION) {
            /* anidada: solo la raiz o el cuerpo directo la
               aceptan (estos los trata p_prog) */
            falla(80, l + 1);
            return -1;
        }
        if (r == R_SINOSI || r == R_SINO || r == R_FINSI ||
            r == R_FINMIENTRAS || r == R_FINFUNCION) {
            falla(60, l + 1);
            return -1;
        }
        if (r == R_FIN) {
            falla(12, l + 1);
            return -1;
        }
        if (r == R_PROGRAMA) {
            falla(16, l + 1);
            return -1;
        }
        if (r == R_IMPORTAR) {
            /* importar solo a nivel de programa (nivel 0); en
               bloques y funciones esta prohibido */
            if (nivel > 0) { falla(16, l + 1); return -1; }
            {
                int q = p_importar(l, bloq);
                if (ERR || q < 0) return -1;
                return q;
            }
        }
        falla(10, l + 1);
        return -1;
    }
    if (TOKS[POS].tipo == T_ID) {
        /* asignacion o llamada, nombre simple o punteado */
        char nb[64];
        int q = 0, p0 = POS;
        nodo_t *nbse;
        if (lee_punteado(nb, (int)sizeof(nb), l + 1)) return -1;
        while (nb[q]) q++;
        nbse = haz(N_REF, l + 1);
        if (!nbse) return -1;
        nbse->s = adup(nb, q);
        if (!nbse->s) return -1;
        if (tk_es(T_OP, O_COR)) {
            /* asignacion indexada: base[indice] = valor */
            nodo_t *ni, *ix, *e;
            POS++;
            ix = p_expr();
            if (ERR || !ix) return -1;
            if (!tk_es(T_OP, O_CCA)) { falla(33, l + 1); return -1; }
            POS++;
            if (!tk_es(T_OP, O_ASIG)) { falla(33, l + 1); return -1; }
            POS++;
            e = p_expr();
            if (ERR || !e) return -1;
            ni = haz(N_ASI, l + 1);
            if (!ni) return -1;
            if (pon_h(ni, nbse) || pon_h(ni, ix) || pon_h(ni, e))
                return -1;
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, ni)) return -1;
            return l + 1;
        }
        if (tk_es(T_OP, O_ASIG)) {
            nodo_t *e;
            n = haz(N_ASIG, l + 1);
            if (!n) return -1;
            n->s = adup(nb, q);
            if (!n->s) return -1;
            POS++;
            e = p_expr();
            if (ERR || !e) return -1;
            if (pon_h(n, e)) return -1;
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, n)) return -1;
            return l + 1;
        }
        if (tk_es(T_OP, O_PAR)) {
            POS = p0;
            n = p_llamada(l);
            if (ERR || !n) return -1;
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, n)) return -1;
            return l + 1;
        }
        falla(33, l + 1);
        return -1;
    }
    falla(10, l + 1);
    return -1;
}

/* programa completo: devuelve raiz N_PROG */
static nodo_t *p_prog(void) {
    nodo_t *prog, *cuerpo;
    int l = 0, visto = 0;
    prog = haz(N_PROG, 1);
    if (!prog) return 0;
    cuerpo = haz(N_BLOQ, 1);
    if (!cuerpo) return 0;
    if (pon_h(prog, cuerpo)) return 0;
    while (l < NLIN) {
        int r;
        if (lin_vacia(l)) { l++; continue; }
        if (LIND[l] != 0) {
            if (!visto) { falla(16, l + 1); return 0; }
            if (lin_res(l) == R_FUNCION) {
                l = p_funcion(l, prog, 0);
                if (ERR) return 0;
                continue;
            }
            l = p_inst(l, cuerpo, 0);
            if (ERR) return 0;
            continue;
        }
        r = lin_res(l);
        if (r == R_PROGRAMA) {
            if (visto) { falla(16, l + 1); return 0; }
            visto = 1;
            if (LTN[l] > 1) { falla(33, l + 1); return 0; }
            l++;
            continue;
        }
        if (r == R_FUNCION) {
            l = p_funcion(l, prog, 0);
            if (ERR) return 0;
            continue;
        }
        if (r == R_IMPORTAR) {
            l = p_importar(l, cuerpo);
            if (ERR || l < 0) return 0;
            continue;
        }
        if (r == R_FIN) {
            int q;
            if (!visto) { falla(15, l + 1); return 0; }
            if (LTN[l] > 1) { falla(33, l + 1); return 0; }
            l++;
            for (q = l; q < NLIN; q++) {
                if (!lin_vacia(q)) { falla(15, q + 1); return 0; }
            }
            return prog;
        }
        /* ejecutable a nivel 0 fuera de programa */
        falla(16, l + 1);
        return 0;
    }
    /* EOF sin fin */
    if (!visto) falla(16, NLIN);
    else falla(15, NLIN);
    return 0;
}

/* ---- mensajes ---- */
static const char *emsg(int c) {
    switch (c) {
    case 1: return "no se pudo abrir o leer el archivo";
    case 2: return "programa demasiado grande";
    case 3: return "demasiadas lineas";
    case 10: return "token invalido";
    case 11: return "usa palabra reservada como nombre";
    case 12: return "falta finsi";
    case 13: return "falta finmientras";
    case 14: return "falta finfuncion";
    case 15: return "falta fin o codigo tras fin";
    case 16: return "codigo fuera de programa";
    case 20: return "el bloque no aumenta indentacion";
    case 21: return "el cierre no alinea con su apertura";
    case 22: return "indentacion invalida (solo multiplos de 4 espacios)";
    case 30: return "cadena sin cerrar";
    case 31: return "entero fuera de 32 bit";
    case 32: return "UTF-8 invalido";
    case 33: return "instruccion mal formada";
    case 34: return "comparacion encadenada";
    case 40: return "variable sin valor inicial";
    case 41: return "variable ya declarada";
    case 42: return "variable inexistente";
    case 43: return "tipos incompatibles";
    case 50: return "division o resto por cero";
    case 51: return "cadena supera 512 B";
    case 52: return "orden no valido con cadenas";
    case 53: return "se exigia booleano";
    case 54: return "leer exige variable de cadena";
    case 55: return "demasiadas variables";
    case 60: return "cierre sin apertura";
    case 61: return "sino mal colocado";
    case 62: return "anidamiento excesivo";
    case 70: return "bucle supera 100000 iteraciones";
    case 80: return "funcion anidada o fuera de nivel";
    case 81: return "parametros duplicados o mas de 8";
    case 82: return "funcion ya definida";
    case 83: return "llamada a funcion inexistente";
    case 84: return "numero de argumentos incorrecto";
    case 85: return "llamadas anidadas mas de 64";
    case 86: return "modulo no encontrado";
    case 87: return "dependencia circular entre modulos";
    case 88: return "ruta de modulo invalida";
    case 100: return "no se pudo abrir el archivo";
    case 101: return "fallo al leer el archivo";
    case 102: return "fallo al escribir el archivo";
    case 103: return "descriptor de archivo invalido";
    case 104: return "modo de apertura invalido";
    case 105: return "demasiados archivos abiertos";
    case 120: return "indice fuera de rango";
    case 121: return "no se puede asignar ahi";
    case 122: return "campo inexistente";
    case 90: return "sin memoria";
    default: return "limite interno del interprete";
    }
}

static void imp_error(void) {
    emit("Error F32-");
    if (ERR < 100) emit("0");
    if (ERR < 10) emit("0");
    emit_num(ERR);
    emit(" en ");
    emit(ARCHIVO);
    if (ERRL > 0) {
        char nb[12];
        emit(" linea ");
        u_itoa(ERRL, nb);
        emit(nb);
    }
    emit(": ");
    emit(emsg(ERR));
    emit("\n");
}

/* ---- tablas de simbolos ---- */
/* Nombres de hasta 63 (identificador 31; con modulo "mod.base"). */
typedef struct {
    char nombre[64];
    int tipo;
} dec_t;

typedef struct {
    char nombre[64];
    val_t v;
    int mod;                   /* modulo dueno (-1 programa) */
} var_t;

/* Busca campo en un REG. Devuelve indice o -1. */
static int reg_halla(val_t r, const char *campo) {
    int k;
    reg_campo_t *cm;
    if (r.tipo != V_REG) return -1;
    cm = (reg_campo_t *)r.pdato;
    for (k = 0; k < r.n; k++) {
        if (!u_strcmp(cm[k].nombre, campo)) return k;
    }
    return -1;
}

static var_t *GLOB = 0;
static int NGLOB = 0;

typedef struct {
    char nombre[64];
    char par[8][32];
    int npar;
    int mod;                   /* modulo dueno (-1 programa) */
    nodo_t *cuerpo;
} fun_t;
static fun_t *FUN = 0;
static int NFUN = 0;

typedef struct {
    var_t loc[MAXLOC];
    int nloc;
} marco_t;
static marco_t *PILA = 0;
static int NDEP = 0;

/* ---- modulos (0.2): importar nombre[.sub] ---- */
#define MAXMOD 16
typedef struct {
    char ruta[128];            /* absoluta resuelta */
    char nombre[64];           /* punteado tal cual */
    int estado;                /* 0 libre, 1 cargando, 2 listo */
    int ejecutado;             /* cuerpo ejecutado (una vez) */
    nodo_t *cuerpo;            /* bloque principal */
} mod_t;
static mod_t *MOD = 0;
static int NMOD = 0;
static char MODSTK[8][128];    /* pila de carga (circulares) */
static int MODSP = 0;
static int MODACT = -1;        /* modulo en ejecucion (-1 programa) */
static int PILAMOD[MAXLLAM];   /* modulo dueno por marco */
static int SEMMOD = -1;        /* modulo en analisis (-1 programa) */
static int RAIZ = 0;           /* ex_bloque raiz: no poda globales */

/* ---- modulos: carga, rutas y precarga (0.2) ---- */

/* Resuelve "a.b.c" a ruta absoluta desde dir_base ("a/b/c.f32").
   Sin ".." (88) ni componentes vacios. 0 ok, -1 mal. */
static int ruta_modulo(const char *dir_base, const char *nombre,
                       char *sal, int max, int lin) {
    int t = 0, i = 0, k;
    while (dir_base[i] && t + 1 < max) {
        sal[t++] = dir_base[i++];
    }
    if (t > 0 && sal[t - 1] != '/' && t + 1 < max) sal[t++] = '/';
    i = 0;
    for (;;) {
        int j = i, n = 0;
        while (nombre[j] && nombre[j] != '.') {
            j++;
            n++;
        }
        if (!n || n > 60) { falla(88, lin); return -1; }
        if (n == 2 && nombre[i] == '.' && nombre[i + 1] == '.') {
            falla(88, lin);
            return -1;
        }
        for (k = 0; k < n; k++) {
            if (t + 1 >= max) { falla(88, lin); return -1; }
            sal[t++] = nombre[i + k];
        }
        if (!nombre[j]) break;
        if (t + 1 >= max) { falla(88, lin); return -1; }
        sal[t++] = '/';
        i = j + 1;
    }
    if (t + 4 >= max) { falla(88, lin); return -1; }
    sal[t++] = '.';
    sal[t++] = 'f';
    sal[t++] = '3';
    sal[t++] = '2';
    sal[t] = 0;
    return 0;
}

/* Directorio de una ruta (hasta la ultima '/'; "" si no hay). */
static void dir_de(const char *ruta, char *sal, int max) {
    int i = 0, ult = -1;
    while (ruta[i]) {
        if (ruta[i] == '/') ult = i;
        i++;
    }
    if (ult < 0) {
        sal[0] = 0;
        return;
    }
    if (ult + 1 >= max) ult = max - 2;
    for (i = 0; i <= ult; i++) sal[i] = ruta[i];
    sal[ult + 1] = 0;
}

static int carga_modulo(const char *ruta_abs, const char *nombre, int lin);

/* Divide "a.b.c" en modulo "a.b" y base "c". Sin punto: mod="". */
static void parte_nom(const char *s, char *mod, char *base) {
    int i = 0, ult = -1, k;
    while (s[i]) {
        if (s[i] == '.') ult = i;
        i++;
    }
    if (ult < 0) {
        mod[0] = 0;
        for (k = 0; s[k] && k < 63; k++) base[k] = s[k];
        base[k] = 0;
        return;
    }
    for (k = 0; k < ult && k < 63; k++) mod[k] = s[k];
    mod[k] = 0;
    i = 0;
    while (s[ult + 1 + i] && i < 63) {
        base[i] = s[ult + 1 + i];
        i++;
    }
    base[i] = 0;
}

/* Indice de modulo por nombre punteado, -1 si no existe. */
static int mod_busca(const char *nombre) {
    int i;
    for (i = 0; i < NMOD; i++) {
        int eq = 1, q = 0;
        while (nombre[q] || MOD[i].nombre[q]) {
            if (nombre[q] != MOD[i].nombre[q]) { eq = 0; break; }
            q++;
            if (q > 64) { eq = 0; break; }
        }
        if (eq) return i;
    }
    return -1;
}

/* Modulo dueno del ambito actual (funcion > modulo > programa). */
static int hogar_mod(void) {
    if (NDEP > 0) return PILAMOD[NDEP - 1];
    return MODACT;
}

/* Busca funcion por (mod, nombre). mod -1 = programa. -1 si no. */
static int fun_busca(int mod, const char *nombre) {
    int i;
    for (i = 0; i < NFUN; i++) {
        if (FUN[i].mod == mod && !u_strcmp(FUN[i].nombre, nombre))
            return i;
    }
    return -1;
}

/* Busca variable global de un modulo. -1 si no esta. */
static int gvar_busca(int mod, const char *nombre) {
    int i;
    for (i = NGLOB - 1; i >= 0; i--) {
        if (GLOB[i].mod == mod && !u_strcmp(GLOB[i].nombre, nombre))
            return i;
    }
    return -1;
}

/* Declaraciones anticipadas para builtins (mk_* se definen despues). */
static val_t mk_int(int v);
static val_t mk_bol(int v);
static val_t mk_str(char *s);
static char *adup(const char *s, int n);

/* ---- archivos y miscelanea (builtins 0.2, §5) ---- */
#define F32_FH 8
static UFILE *FHA[8];
static int FHA_USO[8];

#define BI_ABRIR 1
#define BI_LEER_LINEA 2
#define BI_ESCRIBIR_LINEA 3
#define BI_CERRAR 4
#define BI_EXISTE 5
#define BI_LONGITUD 6
#define BI_BUSCAR 7
#define BI_SUBCADENA 8

static int builtin_id(const char *s) {
    if (!u_strcmp(s, "abrir")) return BI_ABRIR;
    if (!u_strcmp(s, "leer_linea")) return BI_LEER_LINEA;
    if (!u_strcmp(s, "escribir_linea")) return BI_ESCRIBIR_LINEA;
    if (!u_strcmp(s, "cerrar")) return BI_CERRAR;
    if (!u_strcmp(s, "existe")) return BI_EXISTE;
    if (!u_strcmp(s, "longitud")) return BI_LONGITUD;
    if (!u_strcmp(s, "buscar")) return BI_BUSCAR;
    if (!u_strcmp(s, "subcadena")) return BI_SUBCADENA;
    return -1;
}

/* Aridad y tipo de retorno de cada builtin. */
static int builtin_forma(int id, int *mn, int *mx, int *ret) {
    switch (id) {
    case BI_ABRIR: *mn = *mx = 2; *ret = V_INT; return 0;
    case BI_LEER_LINEA: *mn = *mx = 1; *ret = V_STR; return 0;
    case BI_ESCRIBIR_LINEA: *mn = *mx = 2; *ret = V_INT; return 0;
    case BI_CERRAR: *mn = *mx = 1; *ret = V_INT; return 0;
    case BI_EXISTE: *mn = *mx = 1; *ret = V_BOL; return 0;
    case BI_LONGITUD: *mn = *mx = 1; *ret = V_INT; return 0;
    case BI_BUSCAR: *mn = *mx = 2; *ret = V_INT; return 0;
    case BI_SUBCADENA: *mn = 2; *mx = 3; *ret = V_STR; return 0;
    default: return -1;
    }
}

/* Descriptor valido (indice 0..7 en uso). -1 si no. */
static int fh_valido(int h) {
    if (h < 0 || h >= F32_FH || !FHA_USO[h] || !FHA[h]) return -1;
    return 0;
}

static val_t ev_builtin(int id, val_t *av, int na, int lin) {
    val_t r;
    int i;
    r.tipo = 0;
    r.num = 0;
    r.txt = 0;
    r.n = 0;
    r.pdato = 0;
    if (id == BI_ABRIR) {
        const char *md;
        char cm;
        UFILE *f;
        (void)na;
        if (av[0].tipo != V_STR || av[1].tipo != V_STR) {
            falla(43, lin);
            return r;
        }
        md = av[1].txt;
        if (!u_strcmp(md, "lectura")) cm = 'r';
        else if (!u_strcmp(md, "escritura")) cm = 'w';
        else if (!u_strcmp(md, "anadir")) cm = 'a';
        else { falla(104, lin); return r; }
        {
            char mo[2];
            mo[0] = cm;
            mo[1] = 0;
            f = u_fopen(av[0].txt, mo);
        }
        if (!f) { falla(100, lin); return r; }
        for (i = 0; i < F32_FH; i++) {
            if (!FHA_USO[i]) break;
        }
        if (i >= F32_FH) {
            u_fclose(f);
            falla(105, lin);
            return r;
        }
        FHA[i] = f;
        FHA_USO[i] = 1;
        return mk_int(i);
    }
    if (id == BI_CERRAR) {
        int h;
        if (av[0].tipo != V_INT) { falla(43, lin); return r; }
        h = av[0].num;
        if (fh_valido(h)) { falla(103, lin); return r; }
        u_fclose(FHA[h]);
        FHA[h] = 0;
        FHA_USO[h] = 0;
        return mk_int(0);
    }
    if (id == BI_EXISTE) {
        UFILE *f;
        if (av[0].tipo != V_STR) { falla(43, lin); return r; }
        f = u_fopen(av[0].txt, "r");
        if (!f) return mk_bol(0);
        u_fclose(f);
        return mk_bol(1);
    }
    if (id == BI_LEER_LINEA) {
        static char lb[1025];
        int h, n = 0;
        if (av[0].tipo != V_INT) { falla(43, lin); return r; }
        h = av[0].num;
        if (fh_valido(h)) { falla(103, lin); return r; }
        for (;;) {
            unsigned char ch;
            if (n >= 1024) break;
            if (u_fread(&ch, 1, FHA[h]) != 1) break;
            if (ch == '\n') break;
            if (ch == '\r') continue;
            lb[n++] = (char)ch;
        }
        lb[n] = 0;
        {
            char *s = adup(lb, n);
            if (!s) return r;
            return mk_str(s);
        }
    }
    if (id == BI_ESCRIBIR_LINEA) {
        int h, n, w = 0;
        if (av[0].tipo != V_INT || av[1].tipo != V_STR) {
            falla(43, lin);
            return r;
        }
        h = av[0].num;
        if (fh_valido(h)) { falla(103, lin); return r; }
        n = u_strlen(av[1].txt);
        if (n > 0) {
            w = u_fwrite(av[1].txt, n, FHA[h]);
            if (w != n) { falla(102, lin); return r; }
        }
        if (u_fwrite("\n", 1, FHA[h]) != 1) { falla(102, lin); return r; }
        return mk_int(0);
    }
    if (id == BI_LONGITUD) {
        if (av[0].tipo == V_STR) return mk_int(u_strlen(av[0].txt));
        if (av[0].tipo == V_ARR) return mk_int(av[0].n);
        if (av[0].tipo == V_REG) return mk_int(av[0].n);
        falla(43, lin);
        return r;
    }
    if (id == BI_BUSCAR) {
        /* buscar(texto, sub): indice de la primera ocurrencia, -1 si no. */
        const char *t, *s;
        int lt, ls, k, j;
        if (av[0].tipo != V_STR || av[1].tipo != V_STR) {
            falla(43, lin);
            return r;
        }
        t = av[0].txt;
        s = av[1].txt;
        lt = u_strlen(t);
        ls = u_strlen(s);
        if (!ls) return mk_int(0);
        for (k = 0; k + ls <= lt; k++) {
            int ok = 1;
            for (j = 0; j < ls; j++) {
                if (t[k + j] != s[j]) { ok = 0; break; }
            }
            if (ok) return mk_int(k);
        }
        return mk_int(-1);
    }
    if (id == BI_SUBCADENA) {
        /* subcadena(texto, ini [, n]): porcion; fuera de rango = "". */
        const char *t;
        int lt, ini, n, k;
        char *s;
        if (av[0].tipo != V_STR || av[1].tipo != V_INT ||
            (na > 2 && av[2].tipo != V_INT)) {
            falla(43, lin);
            return r;
        }
        t = av[0].txt;
        lt = u_strlen(t);
        ini = av[1].num;
        if (na > 2) n = av[2].num;
        else n = lt - ini;
        if (ini < 0) ini = 0;
        if (n < 0) n = 0;
        if (ini > lt) ini = lt;
        if (ini + n > lt) n = lt - ini;
        if (n > MAXSTR) { falla(51, lin); return r; }
        s = (char *)areserva(n + 1);
        if (!s) return r;
        for (k = 0; k < n; k++) s[k] = t[ini + k];
        s[n] = 0;
        return mk_str(s);
    }
    falla(91, lin);
    return r;
}

static int nombre_ok(const char *s) {
    int i = 0;
    if (!es_idini(s[0])) return 0;
    while (s[i]) {
        if (!es_idc(s[i])) return 0;
        i++;
        if (i >= 32) return 0;
    }
    return i > 0;
}

/* pila de ambitos unica (evita marcos de 9K en pila C).
   sem_bloque guarda/restaura SN: lo declarado dentro de un
   bloque no sale de el (documentado en variables-y-tipos). */
static dec_t *SCOPE = 0;
static int SN = 0;
static int SB[32];
static int SD = 0;

static int busca_dec(const char *s) {
    int i;
    for (i = SN - 1; i >= 0; i--) {
        int q = 0, eq = 1;
        while (s[q] || SCOPE[i].nombre[q]) {
            if (s[q] != SCOPE[i].nombre[q]) { eq = 0; break; }
            q++;
        }
        if (eq) return i;
    }
    return -1;
}

static int mete_dec(const char *s, int tipo, int lin, int cap) {
    int i = 0;
    if (SN >= cap) { falla(55, lin); return -1; }
    while (s[i] && i < 63) {
        SCOPE[SN].nombre[i] = s[i];
        i++;
    }
    SCOPE[SN].nombre[i] = 0;
    SCOPE[SN].tipo = tipo;
    SN++;
    return 0;
}

/* duplicada solo en el bloque actual (permite sombra) */
static int dup_actual(const char *s, int lin) {
    int i, base = SD > 0 ? SB[SD - 1] : 0;
    for (i = base; i < SN; i++) {
        if (!u_strcmp(SCOPE[i].nombre, s)) { falla(41, lin); return -1; }
    }
    return 0;
}

/* ---- inferencia de tipos (estatica) ---- */
#define T_DESCON 0
static int inf_expr(nodo_t *n);

static int inf_llamada(nodo_t *n) {
    int i, f = -1;
    char mod[64], base[64];
    parte_nom(n->s, mod, base);
    if (mod[0]) {
        int md = mod_busca(mod);
        if (md < 0) { falla(86, n->lin); return -1; }
        f = fun_busca(md, base);
        if (f < 0) { falla(83, n->lin); return -1; }
    } else {
        for (i = 0; i < NFUN; i++) {
            if (FUN[i].mod == SEMMOD && !u_strcmp(FUN[i].nombre, base)) {
                f = i;
                break;
            }
        }
        if (f < 0) {
            /* Sin funcion de usuario: prueba builtin (0.2). */
            int mn, mx, rt, bi = builtin_id(base);
            if (bi < 0) { falla(83, n->lin); return -1; }
            builtin_forma(bi, &mn, &mx, &rt);
            if (n->nh < mn || n->nh > mx) { falla(84, n->lin); return -1; }
            for (i = 0; i < n->nh; i++) {
                if (inf_expr(n->h[i]) < 0 && ERR) return -1;
            }
            return rt;
        }
    }
    if (n->nh != FUN[f].npar) { falla(84, n->lin); return -1; }
    for (i = 0; i < n->nh; i++) {
        if (inf_expr(n->h[i]) < 0 && ERR) return -1;
    }
    return T_DESCON;
}

static int inf_expr(nodo_t *n) {
    int a, b, i;
    if (!n) return T_DESCON;
    if (n->tipo == N_LIT) {
        if (n->n1 == V_INT) return V_INT;
        if (n->n1 == V_STR) return V_STR;
        return V_BOL;
    }
    if (n->tipo == N_REF) {
        int q;
        for (q = 0; n->s[q]; q++) {
            if (n->s[q] == '.') break;
        }
        if (n->s[q] == '.') {
            /* Cualificada: modulo.var o registro.campo. Si el
               prefijo es modulo, se valida como tal; si es una
               variable conocida (registro o desconocida), se
               difiere a ejecucion (122 si el campo falta). */
            char mod[64];
            int k = 0;
            while (k < q && k < 63) {
                mod[k] = n->s[k];
                k++;
            }
            mod[k] = 0;
            if (mod_busca(mod) >= 0) return T_DESCON;
            i = busca_dec(mod);
            if (i >= 0) {
                if (SCOPE[i].tipo == V_REG ||
                    SCOPE[i].tipo == T_DESCON)
                    return T_DESCON;
                falla(121, n->lin);
                return -1;
            }
            falla(86, n->lin);
            return -1;
        }
        i = busca_dec(n->s);
        if (i < 0) { falla(42, n->lin); return -1; }
        return SCOPE[i].tipo;
    }
    if (n->tipo == N_ARR) return V_ARR;
    if (n->tipo == N_REG) return V_REG;
    if (n->tipo == N_IDX) {
        int tb, ti;
        tb = inf_expr(n->h[0]);
        if (ERR) return -1;
        ti = inf_expr(n->h[1]);
        if (ERR) return -1;
        if (ti != T_DESCON && ti != V_INT && ti != V_STR) {
            falla(43, n->lin);
            return -1;
        }
        if (tb != T_DESCON && tb != V_ARR && tb != V_STR &&
            tb != V_REG) {
            falla(121, n->lin);
            return -1;
        }
        return T_DESCON;
    }
    if (n->tipo == N_CALL) return inf_llamada(n);
    if (n->tipo == N_UN) {
        a = inf_expr(n->h[0]);
        if (ERR) return -1;
        if (n->n1 == O_MEN) {
            if (a != T_DESCON && a != V_INT) { falla(43, n->lin); return -1; }
            return V_INT;
        }
        if (a != T_DESCON && a != V_BOL) { falla(53, n->lin); return -1; }
        return V_BOL;
    }
    if (n->tipo == N_BIN) {
        a = inf_expr(n->h[0]);
        if (ERR) return -1;
        b = inf_expr(n->h[1]);
        if (ERR) return -1;
        if (n->n1 == R_Y || n->n1 == R_O) {
            if ((a != T_DESCON && a != V_BOL) ||
                (b != T_DESCON && b != V_BOL)) {
                falla(53, n->lin);
                return -1;
            }
            return V_BOL;
        }
        if (n->n1 == O_EQ || n->n1 == O_NE) {
            if (a != T_DESCON && b != T_DESCON && a != b) {
                falla(43, n->lin);
                return -1;
            }
            return V_BOL;
        }
        if (n->n1 == O_LT || n->n1 == O_LE || n->n1 == O_GT ||
            n->n1 == O_GE) {
            /* 0.2: INT o STR (lexicografica); mixtos son 43. */
            if ((a == V_STR && b == V_STR) ||
                (a == V_STR && b == T_DESCON) ||
                (a == T_DESCON && b == V_STR)) {
                return V_BOL;
            }
            if ((a != T_DESCON && a != V_INT) ||
                (b != T_DESCON && b != V_INT)) {
                falla(43, n->lin);
                return -1;
            }
            return V_BOL;
        }
        /* aritmeticos */
        if (n->n1 == O_MAS && ((a == V_STR && (b == V_STR || b == T_DESCON)) ||
                               (b == V_STR && a == T_DESCON)))
            return V_STR;
        if (a != T_DESCON && a != V_INT) { falla(43, n->lin); return -1; }
        if (b != T_DESCON && b != V_INT) { falla(43, n->lin); return -1; }
        return V_INT;
    }
    return T_DESCON;
}

/* ---- validacion semantica ---- */
static int sem_bloque(nodo_t *b, int infun);

static int sem_inst(nodo_t *n, int infun) {
    int i, t;
    if (n->tipo == N_IMP) {
        /* Validado en precarga (existe, sin ciclos); aqui no toca. */
        (void)infun;
        return 0;
    }
    if (n->tipo == N_VAR) {
        if (dup_actual(n->s, n->lin)) return -1;
        t = inf_expr(n->h[0]);
        if (ERR) return -1;
        if (t == T_DESCON) t = V_INT;
        if (mete_dec(n->s, t, n->lin, MAXVAR)) return -1;
        return 0;
    }
    if (n->tipo == N_ASIG) {
        int q;
        for (q = 0; n->s[q]; q++) {
            if (n->s[q] == '.') break;
        }
        if (n->s[q] == '.') {
            /* Destino cualificado: modulo.var o registro.campo.
               Modulo: debe existir (el resto en ejecucion).
               Registro: el prefijo debe ser variable conocida. */
            char mod[64];
            int k = 0;
            while (k < q && k < 63) {
                mod[k] = n->s[k];
                k++;
            }
            mod[k] = 0;
            if (mod_busca(mod) >= 0) {
                t = inf_expr(n->h[0]);
                if (ERR) return -1;
                return 0;
            }
            i = busca_dec(mod);
            if (i < 0) { falla(86, n->lin); return -1; }
            if (SCOPE[i].tipo != V_REG &&
                SCOPE[i].tipo != T_DESCON) {
                falla(121, n->lin);
                return -1;
            }
            t = inf_expr(n->h[0]);
            if (ERR) return -1;
            return 0;
        }
        i = busca_dec(n->s);
        if (i < 0) { falla(42, n->lin); return -1; }
        t = inf_expr(n->h[0]);
        if (ERR) return -1;
        if (t != T_DESCON && t != SCOPE[i].tipo) {
            /* Arreglos/registros: el tipo ya es especifico; si la
               variable era DESCON (p.ej. param), se admite. */
            falla(43, n->lin);
            return -1;
        }
        return 0;
    }
    if (n->tipo == N_ARR) {
        int k;
        for (k = 0; k < n->nh; k++) {
            if (inf_expr(n->h[k]) < 0 && ERR) return -1;
        }
        return V_ARR;
    }
    if (n->tipo == N_REG) {
        int k;
        for (k = 1; k < n->nh; k += 2) {
            if (inf_expr(n->h[k]) < 0 && ERR) return -1;
        }
        return V_REG;
    }
    if (n->tipo == N_IDX) {
        int tb = inf_expr(n->h[0]);
        int ti = inf_expr(n->h[1]);
        if (ERR) return -1;
        if (ti != T_DESCON && ti != V_INT && ti != V_STR) {
            falla(43, n->lin);
            return -1;
        }
        if (tb != T_DESCON && tb != V_ARR && tb != V_STR &&
            tb != V_REG) {
            falla(121, n->lin);
            return -1;
        }
        return T_DESCON;
    }
    if (n->tipo == N_ASI) {
        int tb = inf_expr(n->h[0]);
        int ti = inf_expr(n->h[1]);
        int tv = inf_expr(n->h[2]);
        (void)tv;
        if (ERR) return -1;
        if (ti != T_DESCON && ti != V_INT) { falla(43, n->lin); return -1; }
        if (tb != T_DESCON && tb != V_ARR) { falla(121, n->lin); return -1; }
        return 0;
    }
    if (n->tipo == N_PARA) {
        int t0, t1, t2;
        if (dup_actual(n->s, n->lin)) return -1;
        t0 = inf_expr(n->h[0]);
        if (ERR) return -1;
        t1 = inf_expr(n->h[1]);
        if (ERR) return -1;
        t2 = inf_expr(n->h[2]);
        if (ERR) return -1;
        if ((t0 != T_DESCON && t0 != V_INT) ||
            (t1 != T_DESCON && t1 != V_INT) ||
            (t2 != T_DESCON && t2 != V_INT)) {
            falla(43, n->lin);
            return -1;
        }
        if (mete_dec(n->s, V_INT, n->lin, MAXVAR)) return -1;
        if (sem_bloque(n->h[3], infun)) return -1;
        SN--;
        return 0;
    }
    if (n->tipo == N_PRINT) {
        if (n->nh && inf_expr(n->h[0]) < 0 && ERR) return -1;
        return ERR ? -1 : 0;
    }
    if (n->tipo == N_READ) {
        i = busca_dec(n->s);
        if (i < 0) {
            if (mete_dec(n->s, V_STR, n->lin, MAXVAR)) return -1;
        } else if (SCOPE[i].tipo != V_STR) {
            falla(54, n->lin);
            return -1;
        }
        return 0;
    }
    if (n->tipo == N_RET) {
        if (!infun) { falla(60, n->lin); return -1; }
        if (n->nh && inf_expr(n->h[0]) < 0 && ERR) return -1;
        return ERR ? -1 : 0;
    }
    if (n->tipo == N_CALL) {
        if (inf_llamada(n) < 0 && ERR) return -1;
        return ERR ? -1 : 0;
    }
    if (n->tipo == N_IF) {
        /* hijos: cond,bloque,... [,lit sino, bloque] */
        for (i = 0; i < n->nh; i += 2) {
            if (n->h[i]->tipo == N_LIT) continue;   /* marca sino */
            t = inf_expr(n->h[i]);
            if (ERR) return -1;
            if (t != T_DESCON && t != V_BOL) { falla(53, n->lin); return -1; }
            if (sem_bloque(n->h[i + 1], infun)) return -1;
        }
        return 0;
    }
    if (n->tipo == N_WHILE) {
        t = inf_expr(n->h[0]);
        if (ERR) return -1;
        if (t != T_DESCON && t != V_BOL) { falla(53, n->lin); return -1; }
        if (sem_bloque(n->h[1], infun)) return -1;
        return 0;
    }
    return 0;
}

static int sem_bloque(nodo_t *b, int infun) {
    int i, base = SN;
    if (SD >= 32) { falla(62, b->lin); return -1; }
    SB[SD++] = base;
    for (i = 0; i < b->nh && !ERR; i++) {
        if (sem_inst(b->h[i], infun)) break;
    }
    SD--;
    SN = base;
    return ERR ? -1 : 0;
}

static int semantica(nodo_t *prog) {
    int i, base = NFUN, nf = 0;
    nodo_t *cuerpo = prog->h[0];
    /* 1: recolecta funciones (anexa al registro global; con modulo
       dueno para que dos modulos puedan repetir nombre). */
    for (i = 1; i < prog->nh; i++) {
        nodo_t *d = prog->h[i];
        int k, j;
        if (d->tipo != N_DEF) continue;
        for (k = base; k < base + nf; k++) {
            if (FUN[k].mod == SEMMOD && !u_strcmp(FUN[k].nombre, d->s)) {
                falla(82, d->lin);
                return -1;
            }
        }
        if (NFUN >= MAXFUN) { falla(91, d->lin); return -1; }
        {
            int q = 0;
            while (d->s[q] && q < 63) {
                FUN[NFUN].nombre[q] = d->s[q];
                q++;
            }
            FUN[NFUN].nombre[q] = 0;
        }
        FUN[NFUN].mod = SEMMOD;
        FUN[NFUN].npar = d->nh - 1;   /* ultimo hijo = cuerpo */
        if (FUN[NFUN].npar < 0) FUN[NFUN].npar = 0;
        if (FUN[NFUN].npar > MAXPAR) { falla(81, d->lin); return -1; }
        for (k = 0; k < FUN[NFUN].npar; k++) {
            int q = 0;
            while (d->h[k]->s[q] && q < 31) {
                FUN[NFUN].par[k][q] = d->h[k]->s[q];
                q++;
            }
            FUN[NFUN].par[k][q] = 0;
            for (j = 0; j < k; j++) {
                if (!u_strcmp(FUN[NFUN].par[j], FUN[NFUN].par[k])) {
                    falla(81, d->lin);
                    return -1;
                }
            }
        }
        FUN[NFUN].cuerpo = d->h[d->nh - 1];
        NFUN++;
        nf++;
    }
    /* 2: comprueba cuerpos de funciones DE ESTE programa */
    for (i = base; i < NFUN; i++) {
        int k, sb;
        nodo_t *d = 0, *b = 0;
        int j;
        for (j = 1; j < prog->nh; j++) {
            if (prog->h[j]->tipo == N_DEF &&
                !u_strcmp(prog->h[j]->s, FUN[i].nombre)) {
                d = prog->h[j];
                break;
            }
        }
        if (!d) continue;
        sb = SN;
        for (k = 0; k < FUN[i].npar; k++) {
            if (mete_dec(FUN[i].par[k], T_DESCON, d->lin, MAXLOC))
                return -1;
        }
        /* cuerpo = ultimo hijo */
        b = d->h[d->nh - 1];
        if (sem_bloque(b, 1)) return -1;
        SN = sb;
    }
    /* 3: cuerpo principal */
    if (sem_bloque(cuerpo, 0)) return -1;
    return 0;
}

/* ---- ejecucion ---- */
static int ITER = 0;
static int EXST = 0;   /* 0 normal, 1 retornando */
static val_t EXV;

static var_t *busca_var(const char *s) {
    int i, hm;
    char mod[64], base[64];
    if (NDEP > 0) {
        marco_t *m = &PILA[NDEP - 1];
        for (i = m->nloc - 1; i >= 0; i--) {
            if (!u_strcmp(m->loc[i].nombre, s)) return &m->loc[i];
        }
    }
    hm = hogar_mod();
    parte_nom(s, mod, base);
    if (mod[0]) {
        /* Cualificada: solo el modulo nombrado. */
        int md = mod_busca(mod);
        int gi;
        if (md < 0) return 0;
        gi = gvar_busca(md, base);
        return gi < 0 ? 0 : &GLOB[gi];
    }
    for (i = NGLOB - 1; i >= 0; i--) {
        if (GLOB[i].mod == hm && !u_strcmp(GLOB[i].nombre, s))
            return &GLOB[i];
    }
    return 0;
}

static int pon_var(const char *s, val_t v, int lin, int local) {
    var_t *e = 0;
    int i;
    if (local && NDEP > 0) {
        marco_t *m = &PILA[NDEP - 1];
        for (i = m->nloc - 1; i >= 0; i--) {
            if (!u_strcmp(m->loc[i].nombre, s)) { e = &m->loc[i]; break; }
        }
        if (!e) {
            if (m->nloc >= MAXLOC) { falla(55, lin); return -1; }
            e = &m->loc[m->nloc++];
            i = 0;
            while (s[i] && i < 63) {
                e->nombre[i] = s[i];
                i++;
            }
            e->nombre[i] = 0;
        }
    } else {
        int hm = hogar_mod();
        for (i = NGLOB - 1; i >= 0; i--) {
            if (GLOB[i].mod == hm && !u_strcmp(GLOB[i].nombre, s)) {
                e = &GLOB[i];
                break;
            }
        }
        if (!e) {
            if (NGLOB >= MAXVAR) { falla(55, lin); return -1; }
            e = &GLOB[NGLOB++];
            i = 0;
            while (s[i] && i < 63) {
                e->nombre[i] = s[i];
                i++;
            }
            e->nombre[i] = 0;
            e->mod = hm;
        }
    }
    e->v = v;
    return 0;
}

static val_t mk_int(int v) {
    val_t r;
    r.tipo = V_INT;
    r.num = v;
    r.txt = 0;
    r.n = 0;
    r.pdato = 0;
    return r;
}

static val_t mk_bol(int v) {
    val_t r;
    r.tipo = V_BOL;
    r.num = v ? 1 : 0;
    r.txt = 0;
    r.n = 0;
    r.pdato = 0;
    return r;
}

static val_t mk_str(char *s) {
    val_t r;
    r.tipo = V_STR;
    r.num = 0;
    r.txt = s;
    r.n = 0;
    r.pdato = 0;
    return r;
}

static int a_int(unsigned a, unsigned b, int op, int lin, int *out) {
    unsigned r;
    if (op == O_MAS) r = a + b;
    else if (op == O_MEN) r = a - b;
    else if (op == O_POR) r = a * b;
    else if (op == O_DIV) {
        if (!b) { falla(50, lin); return -1; }
        if (a == 0x80000000u && b == 0xFFFFFFFFu) r = 0x80000000u;
        else r = (unsigned)((int)a / (int)b);
    } else {
        if (!b) { falla(50, lin); return -1; }
        if (a == 0x80000000u && b == 0xFFFFFFFFu) r = 0;
        else r = (unsigned)((int)a % (int)b);
    }
    *out = (int)r;
    return 0;
}

static val_t ev_expr(nodo_t *n);

/* Igualdad profunda para 0.2 (INT/BOL/STR/ARR/REG). */
static int eq_val(val_t a, val_t b) {
    int k;
    if (a.tipo != b.tipo) return 0;
    if (a.tipo == V_INT || a.tipo == V_BOL) return a.num == b.num;
    if (a.tipo == V_STR) return !u_strcmp(a.txt, b.txt);
    if (a.tipo == V_ARR) {
        val_t *va, *vb;
        if (a.n != b.n) return 0;
        va = (val_t *)a.pdato;
        vb = (val_t *)b.pdato;
        for (k = 0; k < a.n; k++) {
            if (!eq_val(va[k], vb[k])) return 0;
        }
        return 1;
    }
    if (a.tipo == V_REG) {
        reg_campo_t *ca = (reg_campo_t *)a.pdato;
        reg_campo_t *cb = (reg_campo_t *)b.pdato;
        int j;
        if (a.n != b.n) return 0;
        for (k = 0; k < a.n; k++) {
            int hall = 0;
            for (j = 0; j < b.n; j++) {
                if (!u_strcmp(ca[k].nombre, cb[j].nombre)) {
                    hall = 1;
                    if (!eq_val(ca[k].v, cb[j].v)) return 0;
                    break;
                }
            }
            if (!hall) return 0;
        }
        return 1;
    }
    return 0;
}

static val_t ev_bin(nodo_t *n) {
    val_t a, b, r;
    int i, j, eq;
    r.tipo = 0;
    r.num = 0;
    r.txt = 0;
    r.n = 0;
    r.pdato = 0;
    a = ev_expr(n->h[0]);
    if (ERR) return r;
    /* cortocircuito */
    if (n->n1 == R_Y) {
        if (a.tipo != V_BOL) { falla(53, n->lin); return r; }
        if (!a.num) return mk_bol(0);
        b = ev_expr(n->h[1]);
        if (ERR) return r;
        if (b.tipo != V_BOL) { falla(53, n->lin); return r; }
        return mk_bol(b.num);
    }
    if (n->n1 == R_O) {
        if (a.tipo != V_BOL) { falla(53, n->lin); return r; }
        if (a.num) return mk_bol(1);
        b = ev_expr(n->h[1]);
        if (ERR) return r;
        if (b.tipo != V_BOL) { falla(53, n->lin); return r; }
        return mk_bol(b.num);
    }
    b = ev_expr(n->h[1]);
    if (ERR) return r;
    if (n->n1 == O_EQ || n->n1 == O_NE) {
        if (a.tipo == V_ARR || a.tipo == V_REG ||
            b.tipo == V_ARR || b.tipo == V_REG) {
            if (a.tipo != b.tipo) { falla(43, n->lin); return r; }
            eq = eq_val(a, b);
        } else if (a.tipo != b.tipo) {
            falla(43, n->lin);
            return r;
        } else if (a.tipo == V_INT) eq = a.num == b.num;
        else if (a.tipo == V_BOL) eq = a.num == b.num;
        else eq = !u_strcmp(a.txt, b.txt);
        if (n->n1 == O_NE) eq = !eq;
        return mk_bol(eq);
    }
    if (n->n1 >= O_LT && n->n1 <= O_GE) {
        int c;
        if (a.tipo == V_STR && b.tipo == V_STR) {
            /* 0.2: comparacion lexicografica (por bytes). */
            int k2 = 0, signo = 0;
            for (;;) {
                unsigned char ca = (unsigned char)a.txt[k2];
                unsigned char cb = (unsigned char)b.txt[k2];
                if (ca != cb) { signo = ca < cb ? -1 : 1; break; }
                if (!ca) { signo = 0; break; }
                k2++;
            }
            if (n->n1 == O_LT) c = signo < 0;
            else if (n->n1 == O_LE) c = signo <= 0;
            else if (n->n1 == O_GT) c = signo > 0;
            else c = signo >= 0;
            return mk_bol(c);
        }
        if (a.tipo != V_INT || b.tipo != V_INT) {
            falla(43, n->lin);
            return r;
        }
        if (n->n1 == O_LT) c = a.num < b.num;
        else if (n->n1 == O_LE) c = a.num <= b.num;
        else if (n->n1 == O_GT) c = a.num > b.num;
        else c = a.num >= b.num;
        return mk_bol(c);
    }
    if (n->n1 == O_MAS && a.tipo == V_STR && b.tipo == V_STR) {
        int la = u_strlen(a.txt), lb = u_strlen(b.txt);
        char *s;
        if (la + lb > MAXSTR) { falla(51, n->lin); return r; }
        s = (char *)areserva(la + lb + 1);
        if (!s) return r;
        for (i = 0; i < la; i++) s[i] = a.txt[i];
        for (j = 0; j < lb; j++) s[la + j] = b.txt[j];
        s[la + lb] = 0;
        return mk_str(s);
    }
    if (a.tipo != V_INT || b.tipo != V_INT) { falla(43, n->lin); return r; }
    {
        int v;
        if (a_int((unsigned)a.num, (unsigned)b.num, n->n1, n->lin, &v))
            return r;
        return mk_int(v);
    }
}

static val_t ev_expr(nodo_t *n) {
    val_t r, v;
    var_t *e;
    int i;
    r.tipo = 0;
    r.num = 0;
    r.txt = 0;
    r.n = 0;
    r.pdato = 0;
    if (!n) return r;
    if (n->tipo == N_LIT) {
        if (n->n1 == V_INT) return mk_int(n->n2);
        if (n->n1 == V_STR) return mk_str(n->s);
        return mk_bol(n->n2);
    }
    if (n->tipo == N_REF) {
        int q;
        for (q = 0; n->s[q]; q++) {
            if (n->s[q] == '.') break;
        }
        if (n->s[q] == '.') {
            /* Punteado: modulo.var o registro.campo (una o varias). */
            char modp[64], base[64];
            int md;
            parte_nom(n->s, modp, base);
            md = mod_busca(modp);
            if (md >= 0) {
                int gi = gvar_busca(md, base);
                if (gi < 0) { falla(42, n->lin); return r; }
                return GLOB[gi].v;
            }
            /* Registro: primer componente es variable, resto campos. */
            {
                char prim[64], resto[64];
                int p = 0, k = 0;
                var_t *ev;
                val_t cur;
                while (n->s[p] && n->s[p] != '.' && k < 63) {
                    prim[k++] = n->s[p++];
                }
                prim[k] = 0;
                k = 0;
                if (n->s[p] == '.') p++;
                while (n->s[p] && k < 63) resto[k++] = n->s[p++];
                resto[k] = 0;
                ev = busca_var(prim);
                if (!ev) { falla(86, n->lin); return r; }
                if (ev->v.tipo != V_REG) { falla(121, n->lin); return r; }
                cur = ev->v;
                for (;;) {
                    char head[64], tail[64];
                    int h = 0, t2 = 0, j = 0, hi;
                    while (resto[j] && resto[j] != '.' && h < 63) {
                        head[h++] = resto[j++];
                    }
                    head[h] = 0;
                    t2 = 0;
                    if (resto[j] == '.') j++;
                    while (resto[j] && t2 < 63) tail[t2++] = resto[j++];
                    tail[t2] = 0;
                    hi = reg_halla(cur, head);
                    if (hi < 0) { falla(122, n->lin); return r; }
                    cur = ((reg_campo_t *)cur.pdato)[hi].v;
                    if (!tail[0]) return cur;
                    if (cur.tipo != V_REG) { falla(121, n->lin); return r; }
                    h = 0;
                    while (tail[h] && h < 63) {
                        resto[h] = tail[h];
                        h++;
                    }
                    resto[h] = 0;
                }
            }
        }
        e = busca_var(n->s);
        if (!e) { falla(42, n->lin); return r; }
        return e->v;
    }
    if (n->tipo == N_ARR) {
        val_t *vec;
        int k;
        if (n->nh > MAXARR) { falla(91, n->lin); return r; }
        vec = (val_t *)areserva(sizeof(val_t) * (n->nh > 0 ? n->nh : 1));
        if (!vec) { falla(90, n->lin); return r; }
        for (k = 0; k < n->nh; k++) {
            vec[k] = ev_expr(n->h[k]);
            if (ERR) return r;
        }
        {
            val_t w;
            w.tipo = V_ARR;
            w.num = 0;
            w.txt = 0;
            w.n = n->nh;
            w.pdato = vec;
            return w;
        }
    }
    if (n->tipo == N_REG) {
        /* h: claveLIT, valor, claveLIT, valor... */
        reg_campo_t *cm;
        int k, nc = n->nh / 2;
        if (nc > MAXCAMPO || (n->nh & 1)) { falla(91, n->lin); return r; }
        cm = (reg_campo_t *)areserva(sizeof(reg_campo_t) *
                                     (nc > 0 ? nc : 1));
        if (!cm) { falla(90, n->lin); return r; }
        for (k = 0; k < nc; k++) {
            int q = 0;
            const char *kn = n->h[k * 2]->s;
            while (kn[q] && q < 63) {
                cm[k].nombre[q] = kn[q];
                q++;
            }
            cm[k].nombre[q] = 0;
            cm[k].v = ev_expr(n->h[k * 2 + 1]);
            if (ERR) return r;
        }
        {
            val_t w;
            w.tipo = V_REG;
            w.num = 0;
            w.txt = 0;
            w.n = nc;
            w.pdato = cm;
            return w;
        }
    }
    if (n->tipo == N_IDX) {
        /* base[indice]: arreglo, cadena o registro (solo lectura). */
        val_t b = ev_expr(n->h[0]);
        val_t q;
        int ix;
        if (ERR) return r;
        /* Registro con clave de cadena: reg["campo"]. */
        if (b.tipo == V_REG) {
            int hi;
            q = ev_expr(n->h[1]);
            if (ERR) return r;
            if (q.tipo != V_STR) { falla(43, n->lin); return r; }
            hi = reg_halla(b, q.txt);
            if (hi < 0) { falla(122, n->lin); return r; }
            return ((reg_campo_t *)b.pdato)[hi].v;
        }
        q = ev_expr(n->h[1]);
        if (ERR) return r;
        if (q.tipo != V_INT) { falla(43, n->lin); return r; }
        ix = q.num;
        if (b.tipo == V_ARR) {
            val_t *vec = (val_t *)b.pdato;
            if (ix < 0 || ix >= b.n) { falla(120, n->lin); return r; }
            return vec[ix];
        }
        if (b.tipo == V_STR) {
            int ln = u_strlen(b.txt);
            if (ix < 0 || ix >= ln) { falla(120, n->lin); return r; }
            {
                char *c2 = (char *)areserva(2);
                val_t w;
                if (!c2) return r;
                c2[0] = b.txt[ix];
                c2[1] = 0;
                w.tipo = V_STR;
                w.num = 0;
                w.txt = c2;
                w.n = 0;
                w.pdato = 0;
                return w;
            }
        }
        falla(121, n->lin);
        return r;
    }
    if (n->tipo == N_UN) {
        v = ev_expr(n->h[0]);
        if (ERR) return r;
        if (n->n1 == O_MEN) {
            unsigned x;
            if (v.tipo != V_INT) { falla(43, n->lin); return r; }
            x = (unsigned)v.num;
            x = 0u - x;
            return mk_int((int)x);
        }
        if (v.tipo != V_BOL) { falla(53, n->lin); return r; }
        return mk_bol(!v.num);
    }
    if (n->tipo == N_BIN) return ev_bin(n);
    if (n->tipo == N_CALL) {
        int f = -1;
        char mod[64], base[64];
        parte_nom(n->s, mod, base);
        if (mod[0]) {
            int md = mod_busca(mod);
            if (md < 0) { falla(86, n->lin); return r; }
            f = fun_busca(md, base);
            if (f < 0) { falla(83, n->lin); return r; }
        } else {
            int hm = hogar_mod();
            for (i = 0; i < NFUN; i++) {
                if (FUN[i].mod == hm && !u_strcmp(FUN[i].nombre, base)) {
                    f = i;
                    break;
                }
            }
            if (f < 0) {
                /* Sin funcion de usuario: builtin (0.2). */
                int mn, mx, rt, k, bi = builtin_id(base);
                val_t args[8];
                if (bi < 0) { falla(83, n->lin); return r; }
                builtin_forma(bi, &mn, &mx, &rt);
                if (n->nh < mn || n->nh > mx) {
                    falla(84, n->lin);
                    return r;
                }
                for (k = 0; k < n->nh; k++) {
                    args[k] = ev_expr(n->h[k]);
                    if (ERR) return r;
                }
                return ev_builtin(bi, args, n->nh, n->lin);
            }
        }
        if (n->nh != FUN[f].npar) { falla(84, n->lin); return r; }
        {
            val_t args[8];
            int k, st;
            for (k = 0; k < n->nh; k++) {
                args[k] = ev_expr(n->h[k]);
                if (ERR) return r;
            }
            if (NDEP >= MAXLLAM) { falla(85, n->lin); return r; }
            PILA[NDEP].nloc = 0;
            PILAMOD[NDEP] = FUN[f].mod;
            NDEP++;
            for (k = 0; k < n->nh; k++) {
                marco_t *m = &PILA[NDEP - 1];
                int q = 0;
                while (FUN[f].par[k][q] && q < 31) {
                    m->loc[k].nombre[q] = FUN[f].par[k][q];
                    q++;
                }
                m->loc[k].nombre[q] = 0;
                m->loc[k].v = args[k];
            }
            PILA[NDEP - 1].nloc = n->nh;
            st = EXST;
            EXST = 0;
            /* ejecuta cuerpo (ex_bloque aisla los locales) */
            {
                val_t ret = mk_int(0);
                if (ex_bloque(FUN[f].cuerpo)) {
                    NDEP--;
                    EXST = st;
                    if (ERR) return r;
                    return ret;
                }
                if (EXST == 1) ret = EXV;
                NDEP--;
                EXST = st;
                if (ERR) return r;
                return ret;
            }
        }
    }
    falla(91, n->lin);
    return r;
}

int ex_inst(nodo_t *n);

static void emit_val(val_t v) {
    char nb[12];
    int k;
    if (v.tipo == V_INT) {
        u_itoa(v.num, nb);
        emit(nb);
    } else if (v.tipo == V_BOL) {
        emit(v.num ? "verdadero" : "falso");
    } else if (v.tipo == V_STR) {
        emit(v.txt);
    } else if (v.tipo == V_ARR) {
        val_t *vec = (val_t *)v.pdato;
        emit("[");
        for (k = 0; k < v.n; k++) {
            if (k) emit(", ");
            if (vec[k].tipo == V_INT) {
                u_itoa(vec[k].num, nb);
                emit(nb);
            } else if (vec[k].tipo == V_BOL) {
                emit(vec[k].num ? "verdadero" : "falso");
            } else if (vec[k].tipo == V_STR) {
                emit(vec[k].txt);
            } else if (vec[k].tipo == V_ARR) {
                emit("[...]");
            } else if (vec[k].tipo == V_REG) {
                emit("[registro]");
            } else {
                emit("?");
            }
        }
        emit("]");
    } else if (v.tipo == V_REG) {
        reg_campo_t *cm = (reg_campo_t *)v.pdato;
        emit("[");
        for (k = 0; k < v.n; k++) {
            if (k) emit(", ");
            emit(cm[k].nombre);
            emit(": ");
            if (cm[k].v.tipo == V_INT) {
                u_itoa(cm[k].v.num, nb);
                emit(nb);
            } else if (cm[k].v.tipo == V_BOL) {
                emit(cm[k].v.num ? "verdadero" : "falso");
            } else if (cm[k].v.tipo == V_STR) {
                emit(cm[k].v.txt);
            } else if (cm[k].v.tipo == V_ARR) {
                emit("[...]");
            } else if (cm[k].v.tipo == V_REG) {
                emit("[registro]");
            } else {
                emit("?");
            }
        }
        emit("]");
    } else {
        emit("?");
    }
}

static int ex_bloque(nodo_t *b) {
    int i, gl = NGLOB, fl = -1, rz = RAIZ;
    RAIZ = 0;
    if (NDEP > 0) fl = PILA[NDEP - 1].nloc;
    for (i = 0; i < b->nh && !ERR && !EXST; i++) {
        if (ex_inst(b->h[i])) break;
    }
    /* La raiz del modulo conserva sus globales (no poda). */
    if (!rz) NGLOB = gl;
    if (NDEP > 0 && fl >= 0) PILA[NDEP - 1].nloc = fl;
    return ERR ? -1 : 0;
}

/* Ejecuta el cuerpo de un modulo conservando sus globales. */
static int ex_bloque_raiz(nodo_t *b) {
    int r;
    RAIZ = 1;
    r = ex_bloque(b);
    RAIZ = 0;
    return r;
}

int ex_inst(nodo_t *n) {
    int i;
    if (!n) return 0;
    if (n->tipo == N_VAR) {
        val_t v = ev_expr(n->h[0]);
        int loc = NDEP > 0 ? 1 : 0;
        if (ERR) return -1;
        /* duplicada en el mismo ambito (modulo incluido) */
        {
            int k, dup = 0, hm = hogar_mod();
            if (loc) {
                marco_t *m = &PILA[NDEP - 1];
                for (k = 0; k < m->nloc; k++) {
                    if (!u_strcmp(m->loc[k].nombre, n->s)) { dup = 1; break; }
                }
            } else {
                for (k = 0; k < NGLOB; k++) {
                    if (GLOB[k].mod == hm &&
                        !u_strcmp(GLOB[k].nombre, n->s)) {
                        dup = 1;
                        break;
                    }
                }
            }
            if (dup) { falla(41, n->lin); return -1; }
        }
        if (pon_var(n->s, v, n->lin, loc)) return -1;
        return 0;
    }
    if (n->tipo == N_ASIG) {
        int q;
        for (q = 0; n->s[q]; q++) {
            if (n->s[q] == '.') break;
        }
        if (n->s[q] == '.') {
            /* modulo.var o registro.campo */
            char modp[64], base[64];
            int md;
            val_t v = ev_expr(n->h[0]);
            if (ERR) return -1;
            parte_nom(n->s, modp, base);
            md = mod_busca(modp);
            if (md >= 0) {
                int gi = gvar_busca(md, base);
                if (gi < 0) { falla(42, n->lin); return -1; }
                if (GLOB[gi].v.tipo != v.tipo) {
                    falla(43, n->lin);
                    return -1;
                }
                GLOB[gi].v = v;
                return 0;
            }
            /* registro.campo (una o varias) */
            {
                char prim[64], resto[64];
                int p = 0, k = 0;
                var_t *ev;
                val_t *curp;
                while (n->s[p] && n->s[p] != '.' && k < 63) {
                    prim[k++] = n->s[p++];
                }
                prim[k] = 0;
                k = 0;
                if (n->s[p] == '.') p++;
                while (n->s[p] && k < 63) resto[k++] = n->s[p++];
                resto[k] = 0;
                ev = busca_var(prim);
                if (!ev) { falla(86, n->lin); return -1; }
                if (ev->v.tipo != V_REG) { falla(121, n->lin); return -1; }
                curp = &ev->v;
                for (;;) {
                    char head[64], tail[64];
                    int h = 0, j = 0, hi;
                    reg_campo_t *cm;
                    while (resto[j] && resto[j] != '.' && h < 63) {
                        head[h++] = resto[j++];
                    }
                    head[h] = 0;
                    {
                        int t2 = 0;
                        if (resto[j] == '.') j++;
                        while (resto[j] && t2 < 63) tail[t2++] = resto[j++];
                        tail[t2] = 0;
                    }
                    if (curp->tipo != V_REG) {
                        falla(121, n->lin);
                        return -1;
                    }
                    cm = (reg_campo_t *)curp->pdato;
                    hi = reg_halla(*curp, head);
                    if (hi < 0) { falla(122, n->lin); return -1; }
                    if (!tail[0]) {
                        cm[hi].v = v;
                        return 0;
                    }
                    curp = &cm[hi].v;
                    h = 0;
                    while (tail[h] && h < 63) {
                        resto[h] = tail[h];
                        h++;
                    }
                    resto[h] = 0;
                }
            }
        }
        {
            val_t v = ev_expr(n->h[0]);
            var_t *e;
            if (ERR) return -1;
            e = busca_var(n->s);
            if (!e) { falla(42, n->lin); return -1; }
            if (e->v.tipo != v.tipo) { falla(43, n->lin); return -1; }
            e->v = v;
            return 0;
        }
    }
    if (n->tipo == N_ASI) {
        /* base[indice] = valor: solo arreglos. */
        val_t b, ixv, v;
        var_t *e = 0;
        val_t *vec;
        int ix, es_ref = 0;
        if (n->h[0]->tipo == N_REF) {
            int q;
            for (q = 0; n->h[0]->s[q]; q++) {
                if (n->h[0]->s[q] == '.') break;
            }
            if (n->h[0]->s[q]) {
                /* a.b[i] = v no soportado: 121 (documentado). */
                falla(121, n->lin);
                return -1;
            }
            e = busca_var(n->h[0]->s);
            es_ref = 1;
        }
        if (es_ref && e) b = e->v;
        else {
            b = ev_expr(n->h[0]);
            if (ERR) return -1;
        }
        ixv = ev_expr(n->h[1]);
        if (ERR) return -1;
        v = ev_expr(n->h[2]);
        if (ERR) return -1;
        if (ixv.tipo != V_INT) { falla(43, n->lin); return -1; }
        if (b.tipo != V_ARR) { falla(121, n->lin); return -1; }
        ix = ixv.num;
        if (ix < 0 || ix >= b.n) { falla(120, n->lin); return -1; }
        vec = (val_t *)b.pdato;
        vec[ix] = v;
        return 0;
    }
    if (n->tipo == N_IMP) {
        int m = mod_busca(n->s);
        if (m < 0) { falla(86, n->lin); return -1; }
        if (!MOD[m].ejecutado) {
            int sv = MODACT;
            MODACT = m;
            if (ex_bloque_raiz(MOD[m].cuerpo)) {
                MODACT = sv;
                return -1;
            }
            MODACT = sv;
            MOD[m].ejecutado = 1;
        }
        return 0;
    }
    if (n->tipo == N_PRINT) {
        if (!n->nh) {
            emit("\n");
            return 0;
        }
        {
            val_t v = ev_expr(n->h[0]);
            if (ERR) return -1;
            emit_val(v);
            emit("\n");
        }
        return 0;
    }
    if (n->tipo == N_READ) {
        char buf[256];
        var_t *e = busca_var(n->s);
        u_lee_linea(buf, 256);
        if (e && e->v.tipo != V_STR) { falla(54, n->lin); return -1; }
        {
            int l = u_strlen(buf);
            char *s = adup(buf, l);
            val_t v;
            if (!s) return -1;
            v = mk_str(s);
            if (pon_var(n->s, v, n->lin, NDEP > 0 ? 1 : 0)) return -1;
        }
        return 0;
    }
    if (n->tipo == N_RET) {
        if (n->nh) {
            EXV = ev_expr(n->h[0]);
            if (ERR) return -1;
        } else {
            EXV = mk_int(0);
        }
        EXST = 1;
        return 0;
    }
    if (n->tipo == N_CALL) {
        (void)ev_expr(n);
        if (ERR) return -1;
        EXST = 0;
        return 0;
    }
    if (n->tipo == N_IF) {
        for (i = 0; i < n->nh; i += 2) {
            if (n->h[i]->tipo == N_LIT) {
                if (ex_bloque(n->h[i + 1])) return -1;
                return 0;
            }
            {
                val_t c = ev_expr(n->h[i]);
                if (ERR) return -1;
                if (c.tipo != V_BOL) { falla(53, n->lin); return -1; }
                if (c.num) {
                    if (ex_bloque(n->h[i + 1])) return -1;
                    return 0;
                }
            }
        }
        return 0;
    }
    if (n->tipo == N_WHILE) {
        ITER = 0;
        for (;;) {
            val_t c = ev_expr(n->h[0]);
            if (ERR) return -1;
            if (c.tipo != V_BOL) { falla(53, n->lin); return -1; }
            if (!c.num) break;
            ITER++;
            if (ITER > MAXITER) { falla(70, n->lin); return -1; }
            if (ex_bloque(n->h[1])) return -1;
            if (EXST) break;
        }
        return 0;
    }
    if (n->tipo == N_PARA) {
        /* para var desde a hasta b [paso s]: h = a, b, s, cuerpo */
        val_t va, vb, vs;
        int ini, fin, paso, gl, fl = -1, habia = 0;
        val_t viejo;
        viejo.tipo = 0;
        viejo.num = 0;
        viejo.txt = 0;
        viejo.n = 0;
        viejo.pdato = 0;
        va = ev_expr(n->h[0]);
        if (ERR) return -1;
        vb = ev_expr(n->h[1]);
        if (ERR) return -1;
        vs = ev_expr(n->h[2]);
        if (ERR) return -1;
        if (va.tipo != V_INT || vb.tipo != V_INT || vs.tipo != V_INT) {
            falla(43, n->lin);
            return -1;
        }
        ini = va.num;
        fin = vb.num;
        paso = vs.num;
        if (!paso) paso = ini <= fin ? 1 : -1;
        if (!paso) { falla(43, n->lin); return -1; }
        /* Guarda la variable si ya existia (sombra temporal). */
        {
            var_t *e0 = busca_var(n->s);
            if (e0) {
                habia = 1;
                viejo = e0->v;
            }
        }
        gl = NGLOB;
        if (NDEP > 0) fl = PILA[NDEP - 1].nloc;
        if (pon_var(n->s, mk_int(ini), n->lin, NDEP > 0 ? 1 : 0))
            return -1;
        ITER = 0;
        for (;;) {
            var_t *e;
            int cur;
            if (paso > 0 ? ini > fin : ini < fin) break;
            e = busca_var(n->s);
            if (!e) { falla(42, n->lin); break; }
            e->v = mk_int(ini);
            ITER++;
            if (ITER > MAXITER) { falla(70, n->lin); break; }
            if (ex_bloque(n->h[3])) break;
            if (EXST) break;
            e = busca_var(n->s);
            cur = e ? e->v.num : ini;
            ini = cur + paso;
        }
        /* Restaura la sombra. */
        if (habia) {
            var_t *e0 = busca_var(n->s);
            if (e0) e0->v = viejo;
        } else {
            NGLOB = gl;
            if (NDEP > 0 && fl >= 0) PILA[NDEP - 1].nloc = fl;
            /* Si pon_var reutilizo una global (no debia: semantica
               exige nombre nuevo), NGLOB no crecio y no hay que
               podar; la restauracion de arriba ya cubre. */
            {
                var_t *e0 = busca_var(n->s);
                /* Si se creo una nueva global, NGLOB>gl y el
                   restauro NGLOB=gl ya la elimino. Si era local,
                   el restauro de nloc ya la elimino. */
                (void)e0;
            }
        }
        return ERR ? -1 : 0;
    }
    return 0;
}

/* ---- modos de prueba ---- */
static const char *tnom(int t) {
    if (t == T_ID) return "ID";
    if (t == T_ENT) return "ENT";
    if (t == T_CAD) return "CAD";
    if (t == T_RES) return "RES";
    if (t == T_OP) return "OP";
    return "?";
}

static void vuelca_toks(void) {
    int i;
    for (i = 0; i < NTOK; i++) {
        char nb[12];
        emit("L");
        u_itoa(TOKS[i].lin, nb);
        emit(nb);
        emit(" ");
        emit(tnom(TOKS[i].tipo));
        emit(" ");
        if (TOKS[i].tipo == T_ENT) {
            u_itoa(TOKS[i].ival, nb);
            emit(nb);
        } else if (TOKS[i].tipo == T_CAD || TOKS[i].tipo == T_ID) {
            emit(TOKS[i].sval);
        } else {
            u_itoa(TOKS[i].ival, nb);
            emit(nb);
        }
        emit("\n");
    }
}

/* ---- init heap (sin estaticos grandes: irian al plano) ---- */
static int f32_init(void) {
    LINI = (int *)u_malloc(MAXLIN * 4);
    LNL = (int *)u_malloc(MAXLIN * 4);
    LIND = (int *)u_malloc(MAXLIN * 4);
    LTINI = (int *)u_malloc(MAXLIN * 4);
    LTN = (int *)u_malloc(MAXLIN * 4);
    TOKS = (tok_t *)u_malloc(MAXTOK * (int)sizeof(tok_t));
    GLOB = (var_t *)u_malloc(MAXVAR * (int)sizeof(var_t));
    FUN = (fun_t *)u_malloc(MAXFUN * (int)sizeof(fun_t));
    PILA = (marco_t *)u_malloc(MAXLLAM * (int)sizeof(marco_t));
    SCOPE = (dec_t *)u_malloc(MAXVAR * (int)sizeof(dec_t));
    MOD = (mod_t *)u_malloc(MAXMOD * (int)sizeof(mod_t));
    if (!LINI || !LNL || !LIND || !LTINI || !LTN || !TOKS || !GLOB ||
        !FUN || !PILA || !SCOPE || !MOD)
        return -1;
    return 0;
}

/* ---- principal ---- */
static void pon_uso(void) {
    emit("uso: f32 programa.f32\n");
    emit("     f32 -t programa.f32 (tokens)\n");
    emit("     f32 -p programa.f32 (comprueba sin ejecutar)\n");
}

/* Libera todo lo reservado por una corrida (tablas + fuente +
   arena) para poder correr varios programas en el mismo proceso
   (t_f32). Idempotente: todo puntero queda a 0. */
static void f32_fin(void) {
    alibera();
    if (SRC) { u_free(SRC); SRC = 0; }
    if (LINI) { u_free(LINI); LINI = 0; }
    if (LNL) { u_free(LNL); LNL = 0; }
    if (LIND) { u_free(LIND); LIND = 0; }
    if (LTINI) { u_free(LTINI); LTINI = 0; }
    if (LTN) { u_free(LTN); LTN = 0; }
    if (TOKS) { u_free(TOKS); TOKS = 0; }
    if (GLOB) { u_free(GLOB); GLOB = 0; }
    if (FUN) { u_free(FUN); FUN = 0; }
    if (PILA) { u_free(PILA); PILA = 0; }
    if (SCOPE) { u_free(SCOPE); SCOPE = 0; }
    if (MOD) { u_free(MOD); MOD = 0; }
    {
        int qi;
        for (qi = 0; qi < F32_FH; qi++) {
            if (FHA_USO[qi] && FHA[qi]) u_fclose(FHA[qi]);
            FHA[qi] = 0;
            FHA_USO[qi] = 0;
        }
    }
    ERR = ERRL = 0;
    SRCL = NLIN = 0;
    NTOK = POS = 0;
    NGLOB = NFUN = 0;
    NDEP = 0;
    SN = SD = 0;
    ITER = 0;
    EXST = 0;
    EXV.tipo = 0;
    EXV.num = 0;
    EXV.txt = 0;
    NMOD = 0;
    MODSP = 0;
    MODACT = -1;
    SEMMOD = -1;
    RAIZ = 0;
    {
        int qi;
        for (qi = 0; qi < MAXLLAM; qi++) PILAMOD[qi] = -1;
    }
    FSAL = 0;
}

/* Ejecuta ruta con modo (0 normal, 1 tokens, 2 comprueba).
   Devuelve como main (0 ok, 1 error programa, 2 mal uso). */
/* Declaraciones anticipadas (el cargador va antes que el parser). */
static nodo_t *p_prog(void);
static int semantica(nodo_t *prog);
static int parte_lineas(void);
static int lex_todo(void);
static void agrupa(void);
static int carga(UFILE *fh);
static int lee_punteado(char *buf, int cap, int lin);
static val_t mk_int(int v);
static val_t mk_bol(int v);
static val_t mk_str(char *s);
static char *adup(const char *s, int n);

/* Carga y analiza un modulo (recursivo). Devuelve indice o -1.
   Registra funciones (con modulo) y deja el cuerpo listo para
   ejecutar una vez. Detecta circulares (87) e inexistentes (86). */
static int carga_modulo(const char *ruta_abs, const char *nombre, int lin) {
    int i, m;
    UFILE *fh;
    nodo_t *prog;
    char arch_save[128];
    /* Primero la pila (circular): un modulo a medio cargar esta
       registrado pero no terminado; si solo miraramos la tabla
       lo dariamos por bueno y el ciclo pasaria inadvertido. */
    for (i = 0; i < MODSP; i++) {
        int k = 0, eq = 1;
        while (ruta_abs[k] || MODSTK[i][k]) {
            if (ruta_abs[k] != MODSTK[i][k]) { eq = 0; break; }
            k++;
            if (k > 120) { eq = 0; break; }
        }
        if (eq) { falla(87, lin); return -1; }
    }
    for (i = 0; i < NMOD; i++) {
        int k = 0, eq = 1;
        while (ruta_abs[k] || MOD[i].ruta[k]) {
            if (ruta_abs[k] != MOD[i].ruta[k]) { eq = 0; break; }
            k++;
            if (k > 120) { eq = 0; break; }
        }
        if (eq) return i;
    }
    if (NMOD >= MAXMOD || MODSP >= 8) { falla(91, lin); return -1; }
    fh = u_fopen(ruta_abs, "r");
    if (!fh) { falla(86, lin); return -1; }
    for (i = 0; ruta_abs[i] && i < 120; i++) MODSTK[MODSP][i] = ruta_abs[i];
    MODSTK[MODSP][i] = 0;
    MODSP++;
    for (i = 0; ARCHIVO[i] && i < 120; i++) arch_save[i] = ARCHIVO[i];
    arch_save[i] = 0;
    for (i = 0; ruta_abs[i] && i < 120; i++) ARCHIVO[i] = ruta_abs[i];
    ARCHIVO[i] = 0;
    u_free(SRC);
    SRC = 0;
    if (carga(fh)) {
        u_fclose(fh);
        MODSP--;
        /* Sin restaurar ARCHIVO: imp_error debe mostrar el
           fichero que fallo, no el que importo. */
        return -1;
    }
    u_fclose(fh);
    if (parte_lineas() || lex_todo()) {
        MODSP--;
        return -1;
    }
    agrupa();
    prog = p_prog();
    if (ERR || !prog) {
        if (!ERR) falla(15, 0);
        MODSP--;
        return -1;
    }
    m = NMOD++;
    {
        int q = 0;
        while (ruta_abs[q] && q < 120) {
            MOD[m].ruta[q] = ruta_abs[q];
            q++;
        }
        MOD[m].ruta[q] = 0;
        q = 0;
        while (nombre[q] && q < 63) {
            MOD[m].nombre[q] = nombre[q];
            q++;
        }
        MOD[m].nombre[q] = 0;
    }
    MOD[m].estado = 1;
    MOD[m].ejecutado = 0;
    MOD[m].cuerpo = 0;
    /* Precarga anidada antes de la semantica propia. */
    for (i = 0; i < prog->h[0]->nh && !ERR; i++) {
        nodo_t *nd = prog->h[0]->h[i];
        if (nd->tipo == N_IMP) {
            char dir[128], ruta2[128];
            dir_de(ruta_abs, dir, (int)sizeof(dir));
            if (ruta_modulo(dir, nd->s, ruta2, (int)sizeof(ruta2),
                            nd->lin)) {
                break;
            }
            if (carga_modulo(ruta2, nd->s, nd->lin) < 0) break;
        }
    }
    if (!ERR) {
        int smsv = SEMMOD;
        SEMMOD = m;
        if (semantica(prog)) {
            SEMMOD = smsv;
            MODSP--;
            return -1;
        }
        SEMMOD = smsv;
        MOD[m].cuerpo = prog->h[0];
        MOD[m].estado = 2;
    }
    MODSP--;
    if (ERR) return -1;
    for (i = 0; arch_save[i]; i++) ARCHIVO[i] = arch_save[i];
    ARCHIVO[i] = 0;
    return m;
}

/* Precarga los importar del cuerpo principal (recursivo). */
static int precarga(nodo_t *cuerpo, const char *dir_base) {
    int i;
    for (i = 0; i < cuerpo->nh && !ERR; i++) {
        nodo_t *n = cuerpo->h[i];
        char ruta[128];
        if (n->tipo != N_IMP) continue;
        if (ruta_modulo(dir_base, n->s, ruta, (int)sizeof(ruta),
                        n->lin))
            return -1;
        if (carga_modulo(ruta, n->s, n->lin) < 0) return -1;
    }
    return ERR ? -1 : 0;
}

static int f32_corre(const char *ruta, int modo) {
    nodo_t *prog;
    {
        int q = 0;
        while (ruta[q] && q < 120) {
            ARCHIVO[q] = ruta[q];
            q++;
        }
        ARCHIVO[q] = 0;
    }
    if (!ruta[0]) return 2;
    /* Estado fresco: limpia restos de una corrida anterior (t_f32
       corre varios programas en el mismo proceso). */
    f32_fin();
    /* Captura para terminal en ventana: se trunca aqui para que la
       terminal siempre lea un fichero fresco (aunque el programa
       falle despues). Si no se puede crear, FSAL=0 y solo consola. */
    FSAL = u_fopen(SAL_TMP, "w");
    /* Abre ANTES de reservar: abrir (syscall 3/9) reinicia el brk
       del heap Ring3 e invalidaria los malloc previos. Todo
       malloc posterior al ultimo fopen es seguro. */
    {
        UFILE *fh = u_fopen(ruta, "r");
        if (!fh) {
            falla(1, 0);
            imp_error();
            alibera();
            cierra_sal();
            return 1;
        }
        if (f32_init()) {
            u_fclose(fh);
            emit("Error F32-090: sin memoria\n");
            cierra_sal();
            return 2;
        }
        if (carga(fh)) {
            u_fclose(fh);
            if (!ERR) falla(1, 0);
            imp_error();
            alibera();
            cierra_sal();
            return ERR == 2 ? 2 : 1;
        }
        u_fclose(fh);
    }
    if (parte_lineas()) {
        imp_error();
        alibera();
        cierra_sal();
        return 1;
    }
    if (lex_todo()) {
        imp_error();
        alibera();
        cierra_sal();
        return 1;
    }
    if (modo == 1) {
        vuelca_toks();
        alibera();
        cierra_sal();
        return ERR ? 1 : 0;
    }
    agrupa();
    prog = p_prog();
    if (ERR || !prog) {
        if (!ERR) falla(15, 0);
        imp_error();
        alibera();
        cierra_sal();
        return 1;
    }
    {
        /* Precarga de modulos antes de la semantica propia, para
           que las llamadas cualificadas se validen (86/83/84). */
        char dir[128];
        dir_de(ruta, dir, (int)sizeof(dir));
        SEMMOD = -1;
        if (precarga(prog->h[0], dir)) {
            imp_error();
            alibera();
            cierra_sal();
            return 1;
        }
    }
    SEMMOD = -1;
    if (semantica(prog)) {
        imp_error();
        alibera();
        cierra_sal();
        return 1;
    }
    if (modo == 2) {
        char nb[12];
        emit("OK: ");
        u_itoa(NFUN, nb);
        emit(nb);
        emit(" funciones, programa valido\n");
        alibera();
        cierra_sal();
        return 0;
    }
    if (ex_bloque(prog->h[0])) {
        imp_error();
        alibera();
        cierra_sal();
        return 1;
    }
    alibera();
    u_free(SRC);
    SRC = 0;
    cierra_sal();
    return 0;
}

#ifndef F32_LIB
int main(void) {
    const char *a = u_args();
    char ruta[256];
    int i = 0, modo = 0;
    while (*a == ' ') a++;
    if (a[0] == '-' && (a[1] == 't' || a[1] == 'p') && a[2] == ' ') {
        modo = a[1] == 't' ? 1 : 2;
        a += 3;
        while (*a == ' ') a++;
    }
    while (*a && i < 255) ruta[i++] = *a++;
    ruta[i] = 0;
    if (!ruta[0]) {
        pon_uso();
        return 2;
    }
    return f32_corre(ruta, modo);
}
#endif

/* FIN */





