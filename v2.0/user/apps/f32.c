#include "ulib.h"
#include "utext.h"

void __main(void) {
}

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
#define ARENAMAX 393216

/* ---- valores ---- */
#define V_INT 1
#define V_STR 2
#define V_BOL 3
typedef struct {
    int tipo;
    int num;
    char *txt;
} val_t;

/* ---- error ---- */
static int ERR = 0, ERRL = 0;
static char ARCHIVO[128];
static void falla(int cod, int lin) {
    if (!ERR) { ERR = cod; ERRL = lin; }
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

static int carga(const char *ruta) {
    UFILE *f = u_fopen(ruta, "r");
    char *tmp;
    int n, tot = 0;
    if (!f) { falla(1, 0); return -1; }
    tmp = (char *)u_malloc(MAXSRC + 1);
    if (!tmp) { u_fclose(f); falla(90, 0); return -1; }
    while (tot <= MAXSRC && (n = u_fread(tmp + tot, MAXSRC + 1 - tot, f)) > 0)
        tot += n;
    u_fclose(f);
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
/* ops: + - * / % = == != < <= > >= ( ) , */
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
        "verdadero", "falso", 0
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
static nodo_t *p_bloque(int ind);

static int tk_es(int t, int r) {
    return POS < NTOK && TOKS[POS].tipo == t &&
           (r < 0 || TOKS[POS].ival == r);
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
    if (TOKS[POS].tipo == T_ID) {
        char *nm = TOKS[POS].sval;
        int l2 = ln;
        POS++;
        if (tk_es(T_OP, O_PAR)) {
            /* llamada */
            nodo_t *n = haz(N_CALL, l2);
            nodo_t *a;
            if (!n) return 0;
            n->s = nm;
            POS++;
            if (tk_es(T_OP, O_CPA)) {
                POS++;
                return n;
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
            return n;
        }
        {
            nodo_t *n = haz(N_REF, l2);
            if (!n) return 0;
            n->s = nm;
            return n;
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
    if (TOKS[POS].tipo == T_RES && TOKS[POS].ival == R_NO) {
        nodo_t *n, *e;
        POS++;
        e = p_un();
        if (ERR || !e) return 0;
        n = haz(N_UN, ln);
        if (!n) return 0;
        n->n1 = R_NO;
        if (pon_h(n, e)) return 0;
        return n;
    }
    return p_prim();
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
    nodo_t *e = p_cmp();
    if (ERR || !e) return 0;
    while (POS < NTOK && TOKS[POS].tipo == T_RES &&
           TOKS[POS].ival == R_Y) {
        int ln = TOKS[POS].lin;
        nodo_t *n, *d;
        POS++;
        d = p_cmp();
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

/* parsea llamada como instruccion: ID ( args ) fin */
static nodo_t *p_llamada(int l) {
    nodo_t *n;
    char *nm = TOKS[POS].sval;
    int ln = TOKS[POS].lin;
    nodo_t *a;
    POS++;             /* ID */
    POS++;             /* ( */
    n = haz(N_CALL, ln);
    if (!n) return 0;
    n->s = nm;
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

static int p_inst(int l, nodo_t *bloq, int nivel) {
    int r, ind = LIND[l];
    nodo_t *n;
    (void)ind;
    POS = LTINI[l];
    if (POS >= NTOK) { falla(91, l + 1); return -1; }
    if (TOKS[POS].tipo == T_RES) {
        r = TOKS[POS].ival;
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
        falla(10, l + 1);
        return -1;
    }
    if (TOKS[POS].tipo == T_ID) {
        /* asignacion o llamada */
        int p2 = POS + 1;
        if (p2 < NTOK && TOKS[p2].tipo == T_OP &&
            TOKS[p2].ival == O_ASIG) {
            nodo_t *e;
            n = haz(N_ASIG, l + 1);
            if (!n) return -1;
            n->s = TOKS[POS].sval;
            POS = p2 + 1;
            e = p_expr();
            if (ERR || !e) return -1;
            if (pon_h(n, e)) return -1;
            if (exige_fin_linea(l)) return -1;
            if (pon_h(bloq, n)) return -1;
            return l + 1;
        }
        if (p2 < NTOK && TOKS[p2].tipo == T_OP &&
            TOKS[p2].ival == O_PAR) {
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
    case 90: return "sin memoria";
    default: return "limite interno del interprete";
    }
}

static void imp_error(void) {
    u_print("Error F32-");
    if (ERR < 100) u_print("0");
    if (ERR < 10) u_print("0");
    u_print_num(ERR);
    u_print(" en ");
    u_print(ARCHIVO);
    if (ERRL > 0) {
        char nb[12];
        u_print(" linea ");
        u_itoa(ERRL, nb);
        u_print(nb);
    }
    u_print(": ");
    u_print(emsg(ERR));
    u_print("\n");
}

/* ---- tablas de simbolos ---- */
typedef struct {
    char nombre[32];
    int tipo;
} dec_t;

typedef struct {
    char nombre[32];
    val_t v;
} var_t;

static var_t *GLOB = 0;
static int NGLOB = 0;

typedef struct {
    char nombre[32];
    char par[8][32];
    int npar;
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
    while (s[i] && i < 31) {
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
    for (i = 0; i < NFUN; i++) {
        if (!u_strcmp(FUN[i].nombre, n->s)) { f = i; break; }
    }
    if (f < 0) { falla(83, n->lin); return -1; }
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
        i = busca_dec(n->s);
        if (i < 0) { falla(42, n->lin); return -1; }
        return SCOPE[i].tipo;
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
            if ((a != T_DESCON && a != V_INT) ||
                (b != T_DESCON && b != V_INT)) {
                if ((a == V_STR && b == V_STR)) { falla(52, n->lin); return -1; }
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
    if (n->tipo == N_VAR) {
        if (dup_actual(n->s, n->lin)) return -1;
        t = inf_expr(n->h[0]);
        if (ERR) return -1;
        if (t == T_DESCON) t = V_INT;
        if (mete_dec(n->s, t, n->lin, MAXVAR)) return -1;
        return 0;
    }
    if (n->tipo == N_ASIG) {
        i = busca_dec(n->s);
        if (i < 0) { falla(42, n->lin); return -1; }
        t = inf_expr(n->h[0]);
        if (ERR) return -1;
        if (t != T_DESCON && t != SCOPE[i].tipo) {
            falla(43, n->lin);
            return -1;
        }
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
    int i, nf = 0;
    nodo_t *cuerpo = prog->h[0];
    /* 1: recolecta funciones */
    for (i = 1; i < prog->nh; i++) {
        nodo_t *d = prog->h[i];
        int k, j;
        for (k = 0; k < nf; k++) {
            if (!u_strcmp(FUN[k].nombre, d->s)) { falla(82, d->lin); return -1; }
        }
        if (nf >= MAXFUN) { falla(91, d->lin); return -1; }
        {
            int q = 0;
            while (d->s[q] && q < 31) {
                FUN[nf].nombre[q] = d->s[q];
                q++;
            }
            FUN[nf].nombre[q] = 0;
        }
        FUN[nf].npar = d->nh;
        if (FUN[nf].npar > MAXPAR) { falla(81, d->lin); return -1; }
        for (k = 0; k < d->nh; k++) {
            int q = 0;
            while (d->h[k]->s[q] && q < 31) {
                FUN[nf].par[k][q] = d->h[k]->s[q];
                q++;
            }
            FUN[nf].par[k][q] = 0;
            for (j = 0; j < k; j++) {
                if (!u_strcmp(FUN[nf].par[j], FUN[nf].par[k])) {
                    falla(81, d->lin);
                    return -1;
                }
            }
        }
        FUN[nf].cuerpo = d->h[d->nh - 1];
        nf++;
    }
    NFUN = nf;
    /* 2: comprueba cuerpos de funciones */
    for (i = 0; i < NFUN; i++) {
        int k, sb;
        nodo_t *d = 0, *b = 0;
        int j;
        for (j = 1; j < prog->nh; j++) {
            if (!u_strcmp(prog->h[j]->s, FUN[i].nombre)) { d = prog->h[j]; break; }
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
    int i;
    if (NDEP > 0) {
        marco_t *m = &PILA[NDEP - 1];
        for (i = m->nloc - 1; i >= 0; i--) {
            if (!u_strcmp(m->loc[i].nombre, s)) return &m->loc[i];
        }
    }
    for (i = NGLOB - 1; i >= 0; i--) {
        if (!u_strcmp(GLOB[i].nombre, s)) return &GLOB[i];
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
            while (s[i] && i < 31) {
                e->nombre[i] = s[i];
                i++;
            }
            e->nombre[i] = 0;
        }
    } else {
        for (i = NGLOB - 1; i >= 0; i--) {
            if (!u_strcmp(GLOB[i].nombre, s)) { e = &GLOB[i]; break; }
        }
        if (!e) {
            if (NGLOB >= MAXVAR) { falla(55, lin); return -1; }
            e = &GLOB[NGLOB++];
            i = 0;
            while (s[i] && i < 31) {
                e->nombre[i] = s[i];
                i++;
            }
            e->nombre[i] = 0;
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
    return r;
}

static val_t mk_bol(int v) {
    val_t r;
    r.tipo = V_BOL;
    r.num = v ? 1 : 0;
    r.txt = 0;
    return r;
}

static val_t mk_str(char *s) {
    val_t r;
    r.tipo = V_STR;
    r.num = 0;
    r.txt = s;
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

static val_t ev_bin(nodo_t *n) {
    val_t a, b, r;
    int i, j, eq;
    r.tipo = 0;
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
        if (a.tipo != b.tipo) { falla(43, n->lin); return r; }
        if (a.tipo == V_INT) eq = a.num == b.num;
        else if (a.tipo == V_BOL) eq = a.num == b.num;
        else eq = !u_strcmp(a.txt, b.txt);
        if (n->n1 == O_NE) eq = !eq;
        return mk_bol(eq);
    }
    if (n->n1 >= O_LT && n->n1 <= O_GE) {
        int c;
        if (a.tipo != V_INT || b.tipo != V_INT) {
            if (a.tipo == V_STR && b.tipo == V_STR) {
                falla(52, n->lin);
                return r;
            }
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
    if (!n) return r;
    if (n->tipo == N_LIT) {
        if (n->n1 == V_INT) return mk_int(n->n2);
        if (n->n1 == V_STR) return mk_str(n->s);
        return mk_bol(n->n2);
    }
    if (n->tipo == N_REF) {
        e = busca_var(n->s);
        if (!e) { falla(42, n->lin); return r; }
        return e->v;
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
        for (i = 0; i < NFUN; i++) {
            if (!u_strcmp(FUN[i].nombre, n->s)) { f = i; break; }
        }
        if (f < 0) { falla(83, n->lin); return r; }
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

static int ex_bloque(nodo_t *b) {
    int i, gl = NGLOB, fl = -1;
    if (NDEP > 0) fl = PILA[NDEP - 1].nloc;
    for (i = 0; i < b->nh && !ERR && !EXST; i++) {
        if (ex_inst(b->h[i])) break;
    }
    NGLOB = gl;
    if (NDEP > 0 && fl >= 0) PILA[NDEP - 1].nloc = fl;
    return ERR ? -1 : 0;
}

int ex_inst(nodo_t *n) {
    int i;
    if (!n) return 0;
    if (n->tipo == N_VAR) {
        val_t v = ev_expr(n->h[0]);
        int loc = NDEP > 0 ? 1 : 0;
        if (ERR) return -1;
        /* duplicada en el mismo ambito */
        {
            int k, dup = 0;
            if (loc) {
                marco_t *m = &PILA[NDEP - 1];
                for (k = 0; k < m->nloc; k++) {
                    if (!u_strcmp(m->loc[k].nombre, n->s)) { dup = 1; break; }
                }
            } else {
                for (k = 0; k < NGLOB; k++) {
                    if (!u_strcmp(GLOB[k].nombre, n->s)) { dup = 1; break; }
                }
            }
            if (dup) { falla(41, n->lin); return -1; }
        }
        if (pon_var(n->s, v, n->lin, loc)) return -1;
        return 0;
    }
    if (n->tipo == N_ASIG) {
        val_t v = ev_expr(n->h[0]);
        var_t *e;
        if (ERR) return -1;
        e = busca_var(n->s);
        if (!e) { falla(42, n->lin); return -1; }
        if (e->v.tipo != v.tipo) { falla(43, n->lin); return -1; }
        e->v = v;
        return 0;
    }
    if (n->tipo == N_PRINT) {
        if (!n->nh) {
            u_print("\n");
            return 0;
        }
        {
            val_t v = ev_expr(n->h[0]);
            char nb[12];
            if (ERR) return -1;
            if (v.tipo == V_INT) {
                u_itoa(v.num, nb);
                u_print(nb);
            } else if (v.tipo == V_BOL) {
                u_print(v.num ? "verdadero" : "falso");
            } else {
                u_print(v.txt);
            }
            u_print("\n");
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
        u_print("L");
        u_itoa(TOKS[i].lin, nb);
        u_print(nb);
        u_print(" ");
        u_print(tnom(TOKS[i].tipo));
        u_print(" ");
        if (TOKS[i].tipo == T_ENT) {
            u_itoa(TOKS[i].ival, nb);
            u_print(nb);
        } else if (TOKS[i].tipo == T_CAD || TOKS[i].tipo == T_ID) {
            u_print(TOKS[i].sval);
        } else {
            u_itoa(TOKS[i].ival, nb);
            u_print(nb);
        }
        u_print("\n");
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
    if (!LINI || !LNL || !LIND || !LTINI || !LTN || !TOKS || !GLOB ||
        !FUN || !PILA || !SCOPE)
        return -1;
    return 0;
}

/* ---- principal ---- */
static void pon_uso(void) {
    u_print("uso: f32 programa.f32\n");
    u_print("     f32 -t programa.f32 (tokens)\n");
    u_print("     f32 -p programa.f32 (comprueba sin ejecutar)\n");
}

int main(void) {
    const char *a = u_args();
    char ruta[256];
    int i = 0, modo = 0;
    nodo_t *prog;
    while (*a == ' ') a++;
    if (a[0] == '-' && (a[1] == 't' || a[1] == 'p') && a[2] == ' ') {
        modo = a[1] == 't' ? 1 : 2;
        a += 3;
        while (*a == ' ') a++;
    }
    while (*a && i < 255) ruta[i++] = *a++;
    ruta[i] = 0;
    {
        int q = 0;
        while (ruta[q] && q < 120) {
            ARCHIVO[q] = ruta[q];
            q++;
        }
        ARCHIVO[q] = 0;
    }
    if (!ruta[0]) {
        pon_uso();
        alibera();
        return 2;
    }
    if (f32_init()) {
        u_print("Error F32-090: sin memoria\n");
        return 2;
    }
    if (carga(ruta)) {
        if (!ERR) falla(1, 0);
        imp_error();
        alibera();
        return ERR == 2 ? 2 : 1;
    }
    if (parte_lineas()) {
        imp_error();
        alibera();
        return 1;
    }
    if (lex_todo()) {
        imp_error();
        alibera();
        return 1;
    }
    if (modo == 1) {
        vuelca_toks();
        alibera();
        return ERR ? 1 : 0;
    }
    agrupa();
    prog = p_prog();
    if (ERR || !prog) {
        if (!ERR) falla(15, 0);
        imp_error();
        alibera();
        return 1;
    }
    if (semantica(prog)) {
        imp_error();
        alibera();
        return 1;
    }
    if (modo == 2) {
        char nb[12];
        u_print("OK: ");
        u_itoa(NFUN, nb);
        u_print(nb);
        u_print(" funciones, programa valido\n");
        alibera();
        return 0;
    }
    if (ex_bloque(prog->h[0])) {
        imp_error();
        alibera();
        return 1;
    }
    alibera();
    u_free(SRC);
    SRC = 0;
    return 0;
}

/* FIN */





