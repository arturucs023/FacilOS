#include "teclado.h"
#include "irq.h"
#include "cpu.h"
#include "libc.h"
#include "libc.h"

/* v2.2: distribuciones ES/CA con teclas muertas y AltGr. El driver
 * emite puntos de codigo Unicode (el pipeline int los transporta) o
 * codigos TEC_*. Muertas en rango privado 0xE000+ (nunca texto).
 * Tablas FacilOS-ES/CA documentadas por scancode en
 * DISENO-TECLADO-v2.2.md (letras/numeros compatibles ES; acentos en
 * 0x1A/0x28 segun distribucion). */

int teclado_es = 1;
static int distrib = 1;   /* 0=US, 1=ES, 2=CA (persiste en cab[15]) */

int distribucion_actual(void) { return distrib; }
void distribucion_pon(int d) {
    if (d < 0 || d > 2) return;
    distrib = d;
    teclado_es = d ? 1 : 0;
}

typedef struct { uint8_t scan, rsv; uint16_t base, mayus, altgr; } tecla_t;
/* Muertas: mayus = muerta alternativa con Shift. */
#define M_AG 0xE000
#define M_GR 0xE001
#define M_CI 0xE002
#define M_DI 0xE003
#define M_TI 0xE004
#define EURO 0x20AC

/* Letras/numeros comunes (base ES; US ignora extras). */
static const tecla_t comunes[] = {
    {0x10,0,'q','Q',0}, {0x11,0,'w','W',0}, {0x12,0,'e','E',0},
    {0x13,0,'r','R',0}, {0x14,0,'t','T',0}, {0x15,0,'y','Y',0},
    {0x16,0,'u','U',0}, {0x17,0,'i','I',0}, {0x18,0,'o','O',0},
    {0x19,0,'p','P',0}, {0x1E,0,'a','A',0}, {0x1F,0,'s','S',0},
    {0x20,0,'d','D',0}, {0x21,0,'f','F',0}, {0x22,0,'g','G',0},
    {0x23,0,'h','H',0}, {0x24,0,'j','J',0}, {0x25,0,'k','K',0},
    {0x26,0,'l','L',0},
    {0x2C,0,'z','Z',0}, {0x2D,0,'x','X',0}, {0x2E,0,'c','C',0},
    {0x2F,0,'v','V',0}, {0x30,0,'b','B',0}, {0x31,0,'n','N',0},
    {0x32,0,'m','M',0},
    {0x02,0,'1','!','|'}, {0x03,0,'2','"','@'}, {0x04,0,'3',0xB7,'#'},
    {0x05,0,'4','$',M_TI}, {0x06,0,'5','%',EURO}, {0x07,0,'6','&',0},
    {0x08,0,'7','/',0}, {0x09,0,'8','(',0}, {0x0A,0,'9',')',0},
    {0x0B,0,'0','=',0}, {0x29,0,0xBA,0xAA,'\\'}, {0x0C,0,'-','_',0},
    {0x0D,0,'+','*',0}, {0x33,0,',',';',0}, {0x34,0,'.',':',0},
    {0x35,0,'/','_',0}, {0x39,0,' ',' ',0}, {0,0,0,0}
};
/* ES: 0x1A tumba/circunflejo, 0x28 agudo/dieresis, 0x1B ¡/¿, 0x2B +/*. */
static const tecla_t es_ext[] = {
    {0x1A,0,M_GR,M_CI,0}, {0x1B,0,0xA1,0xBF,0}, {0x28,0,M_AG,M_DI,0},
    {0x2B,0,'+','*',0}, {0x27,0,0xF1,0xD1,0}, {0,0,0,0}
};
/* CA: c cedilla directa; agudas en 0x1A, graves en 0x28. */
static const tecla_t ca_ext[] = {
    {0x1A,0,M_AG,M_DI,0}, {0x1B,0,0xA1,0xBF,0}, {0x28,0,M_GR,M_CI,0},
    {0x2B,0,'+','*',0}, {0x27,0,0xE7,0xC7,0}, {0,0,0,0}
};
/* US: sin enie (compat v1.x). */
static const tecla_t us_ext[] = {
    {0x1A,0,'[','{',0}, {0x1B,0,']','}',0}, {0x28,0,'\'','"',0},
    {0x2B,0,'\\','|',0}, {0x27,0,';',';',0}, {0,0,0,0}
};

#define TK_N 64
static volatile int tk_cola[TK_N];
static volatile int tk_ini = 0, tk_fin = 0;
static int tk_ext = 0;
static int sh_izq = 0, sh_der = 0, bloqmay = 0, altgr = 0, numlock = 1;
static int muerto = 0;

static void tk_mete(int v) {
    int s = (tk_fin + 1) % TK_N;
    if (s == tk_ini) return;
    tk_cola[tk_fin] = v;
    tk_fin = s;
}

void teclado_pon(int es) { distribucion_pon(es ? 1 : 0); }

/* Inicializacion v2.0 (registro de drivers): deja la cola y los
   modificadores en estado conocido. */
int teclado_init(void) {
    tk_ini = tk_fin = 0;
    tk_ext = 0;
    sh_izq = sh_der = bloqmay = altgr = 0;
    numlock = 1;
    muerto = 0;
    return 0;
}

static int mu_literal(int m) {
    if (m == M_AG) return 0xB4;
    if (m == M_GR) return 0x60;
    if (m == M_CI) return 0x5E;
    if (m == M_DI) return 0xA8;
    if (m == M_TI) return 0x7E;
    return 0;
}

/* Composicion (misma tabla que ut_compone Ring3). */
static int compone(int m, int base) {
    int may = 0;
    int b = base;
    if (b >= 'A' && b <= 'Z') { may = 1; b += 32; }
    else if (b == 0xD1) { may = 1; b = 0xF1; }
    else if (b == 0xC7) { may = 1; b = 0xE7; }
    if (m == M_AG) {
        if (b == 'a') return may ? 0xC1 : 0xE1;
        if (b == 'e') return may ? 0xC9 : 0xE9;
        if (b == 'i') return may ? 0xCD : 0xED;
        if (b == 'o') return may ? 0xD3 : 0xF3;
        if (b == 'u') return may ? 0xDA : 0xFA;
    } else if (m == M_GR) {
        if (b == 'a') return may ? 0xC0 : 0xE0;
        if (b == 'e') return may ? 0xC8 : 0xE8;
        if (b == 'i') return may ? 0xCC : 0xEC;
        if (b == 'o') return may ? 0xD2 : 0xF2;
        if (b == 'u') return may ? 0xD9 : 0xF9;
    } else if (m == M_CI) {
        if (b == 'a') return may ? 0xC2 : 0xE2;
        if (b == 'e') return may ? 0xCA : 0xEA;
        if (b == 'i') return may ? 0xCE : 0xEE;
        if (b == 'o') return may ? 0xD4 : 0xF4;
        if (b == 'u') return may ? 0xDB : 0xFB;
    } else if (m == M_DI) {
        if (b == 'a') return may ? 0xC4 : 0xE4;
        if (b == 'e') return may ? 0xCB : 0xEB;
        if (b == 'i') return may ? 0xCF : 0xEF;
        if (b == 'o') return may ? 0xD6 : 0xF6;
        if (b == 'u') return may ? 0xDC : 0xFC;
    } else if (m == M_TI) {
        if (b == 'a') return may ? 0xC3 : 0xE3;
        if (b == 'n') return may ? 0xD1 : 0xF1;
        if (b == 'o') return may ? 0xD5 : 0xF5;
    }
    return 0;
}

/* Aplica mayusculas (Shift o Caps) a un caracter directo. */
static int aplica_may(int ch, int may) {
    if (!may && !bloqmay) return ch;
    if (may) return ch;
    /* solo Caps: letras */
    if (ch >= 'a' && ch <= 'z') return ch - 32;
    if (ch == 0xF1) return 0xD1;
    if (ch == 0xE7) return 0xC7;
    return ch;
}

static void emite(int ch) {
    if (muerto) {
        int m = muerto;
        muerto = 0;
        if (ch == ' ') {
            tk_mete(mu_literal(m));
            return;
        }
        {
            int c = compone(m, ch);
            if (c) {
                tk_mete(c);
                return;
            }
        }
        tk_mete(mu_literal(m));
    }
    tk_mete(ch);
}

static const tecla_t *tabla_ext(void) {
    if (distrib == 2) return ca_ext;
    if (distrib == 1) return es_ext;
    return us_ext;
}

int kbd_hay(void) {
    int hay;
    cli();
    hay = tk_ini != tk_fin;
    sti();
    return hay;
}

int kbd_tecla(void) {
    for (;;) {
        cli();
        if (tk_ini != tk_fin) {
            int v = tk_cola[tk_ini];
            tk_ini = (tk_ini + 1) % TK_N;
            sti();
            return v;
        }
        sti();
        hlt();
        cli();
        if (tk_ini != tk_fin) {
            int v = tk_cola[tk_ini];
            tk_ini = (tk_ini + 1) % TK_N;
            sti();
            return v;
        }
        sti();
        {
            extern void proc_bloquea_tecla(void);
            proc_bloquea_tecla();
        }
    }
}

void teclado_irq(void) {
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { tk_ext = 1; return; }
    int suelto = sc & 0x80;
    uint8_t c = sc & 0x7F;
    if (tk_ext) {
        tk_ext = 0;
        if (c == 0x38) { altgr = !suelto; return; }
        if (suelto) return;
        muerto = 0;
        if (c == 0x48) tk_mete(TEC_ARRIBA);
        else if (c == 0x50) tk_mete(TEC_ABAJO);
        else if (c == 0x4B) tk_mete(TEC_IZQ);
        else if (c == 0x4D) tk_mete(TEC_DER);
        else if (c == 0x47) tk_mete(TEC_INICIO);
        else if (c == 0x4F) tk_mete(TEC_FIN);
        else if (c == 0x49) tk_mete(TEC_REPAG);
        else if (c == 0x51) tk_mete(TEC_AVPA);
        /* v2.0-etapa5: teclas Windows/menu (E0 5B/5D) para el escritorio. */
        else if (c == 0x5B || c == 0x5D) tk_mete(TEC_MENU);
        return;
    }
    if (c == 0x2A) { sh_izq = !suelto; return; }
    if (c == 0x36) { sh_der = !suelto; return; }
    if (c == 0x3A) { if (!suelto) bloqmay = !bloqmay; return; }
    if (c == 0x45) { if (!suelto) numlock = !numlock; return; }
    if (c == 0x38 && !(sh_izq || sh_der)) {
        /* Alt izquierdo solo: sin accion (AltGr es E0 0x38). */
        return;
    }
    if (suelto) return;
    if (c == 0x1C) { muerto = 0; tk_mete(13); return; }
    if (c == 0x0E) { muerto = 0; tk_mete(8); return; }
    if (c == 0x01) { muerto = 0; tk_mete(27); return; }
    if (c == 0x0F) { muerto = 0; tk_mete(9); return; }
    /* v2.0-etapa5: F1 abre el menu (en QEMU es la via testeable; en HW
       real tambien vale la tecla Menu E0 5B/5D). */
    if (c == 0x3B) { muerto = 0; tk_mete(TEC_MENU); return; }
    /* Supr y numerico */
    if (c == 0x53) {
        if (numlock) { muerto = 0; emite('.'); return; }
        tk_mete(TEC_SUPR);
        return;
    }
    if (numlock) {
        static const char nd[10] = {
            '7', '8', '9', '4', '5', '6', '1', '2', '3', '0'
        };
        static const uint8_t ns[10] = {
            0x47, 0x48, 0x49, 0x4B, 0x4C, 0x4D, 0x4F, 0x50, 0x51, 0x52
        };
        int i;
        for (i = 0; i < 10; i++) {
            if (ns[i] == c) {
                muerto = 0;
                emite(nd[i]);
                return;
            }
        }
        if (c == 0x4A) { muerto = 0; emite('-'); return; }
        if (c == 0x4E) { muerto = 0; emite('+'); return; }
    } else {
        if (c == 0x47) { tk_mete(TEC_INICIO); return; }
        if (c == 0x48) { tk_mete(TEC_ARRIBA); return; }
        if (c == 0x49) { tk_mete(TEC_REPAG); return; }
        if (c == 0x4B) { tk_mete(TEC_IZQ); return; }
        if (c == 0x4D) { tk_mete(TEC_DER); return; }
        if (c == 0x4F) { tk_mete(TEC_FIN); return; }
        if (c == 0x50) { tk_mete(TEC_ABAJO); return; }
        if (c == 0x51) { tk_mete(TEC_AVPA); return; }
        if (c == 0x4A) { muerto = 0; emite('-'); return; }
        if (c == 0x4E) { muerto = 0; emite('+'); return; }
    }
    {
        int may = (sh_izq || sh_der);
        const tecla_t *t;
        for (t = tabla_ext(); t->scan; t++) {
            int ch;
            if (t->scan != c) continue;
            if (altgr && t->altgr) ch = t->altgr;
            else ch = may ? t->mayus : t->base;
            if (ch >= (int)M_AG && ch <= (int)M_TI) {
                muerto = ch;
                return;
            }
            if (altgr && !t->altgr) return;
            emite(aplica_may(ch, may));
            return;
        }
        for (t = comunes; t->scan; t++) {
            int ch;
            if (t->scan != c) continue;
            if (altgr && t->altgr) ch = t->altgr;
            else ch = may ? t->mayus : t->base;
            if (ch >= (int)M_AG && ch <= (int)M_TI) {
                muerto = ch;
                return;
            }
            if (altgr && !t->altgr) return;
            emite(aplica_may(ch, may));
            return;
        }
    }
}
