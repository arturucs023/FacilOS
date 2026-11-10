#include "teclado.h"
#include "irq.h"
#include "cpu.h"
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
static int ctrl_izq = 0, ctrl_der = 0;
static int muerto = 0;

static void tk_mete(int v) {
    int s = (tk_fin + 1) % TK_N;
    if (s == tk_ini) return;
    tk_cola[tk_fin] = v;
    tk_fin = s;
}

/* Especial con modificadores del momento (0x200 May, 0x400 Ctrl):
   la app lo decodifica sin carreras (ver TEC_MAY/TEC_CTRL). */
static void tk_mete_mod(int base) {
    int m = 0;
    if (sh_izq || sh_der) m |= 0x200;
    if (ctrl_izq || ctrl_der) m |= 0x400;
    tk_mete(base | m);
}

void teclado_pon(int es) { distribucion_pon(es ? 1 : 0); }

/* Inicializacion v2.0 (registro de drivers): deja la cola y los
   modificadores en estado conocido. */
int teclado_init(void) {
    tk_ini = tk_fin = 0;
    tk_ext = 0;
    sh_izq = sh_der = bloqmay = altgr = 0;
    ctrl_izq = ctrl_der = 0;
    numlock = 1;
    muerto = 0;
    return 0;
}

/* v2.2-editor: modificadores para Ring3 (syscall 49). bit0 Shift,
   bit1 Ctrl, bit2 AltGr, bit3 BloqMay. Sin esto no hay Shift+flechas
   ni Ctrl+A/C/X/V/Z/Y (el driver descarta Ctrl y no lo exponia). */
int teclado_mod(void) {
    int m = 0;
    if (sh_izq || sh_der) m |= 1;
    if (ctrl_izq || ctrl_der) m |= 2;
    if (altgr) m |= 4;
    if (bloqmay) m |= 8;
    return m;
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
    /* v2.2-editor: Ctrl+letra se codifica en la propia tecla
       (0xE100+idx, 0xE200+idx con May) para que Ring3 no dependa
       de consultar el modificador tras el evento (carrera en QEMU:
       el Ctrl ya se solto al procesar KEY y se insertaba la letra).
       Sin esto no hay atajos Ctrl fiables. */
    if (ctrl_izq || ctrl_der) {
        int idx = -1;
        if (ch >= 'a' && ch <= 'z') idx = ch - 'a';
        else if (ch >= 'A' && ch <= 'Z') idx = ch - 'A';
        if (idx >= 0) {
            muerto = 0;
            tk_mete((sh_izq || sh_der) ? (0xE200 + idx)
                                      : (0xE100 + idx));
            return;
        }
    }
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

/* v2.4-USB: HID-boot (posicional) -> scancode set-1 (posicional ES).
   0 = ignorada. Letras/digitos coinciden; simbolos por posicion. */
static const uint8_t hid_a_scan[128] = {
    [0x04] = 0x1E, [0x05] = 0x30, [0x06] = 0x2E, [0x07] = 0x20,
    [0x08] = 0x12, [0x09] = 0x21, [0x0A] = 0x22, [0x0B] = 0x23,
    [0x0C] = 0x17, [0x0D] = 0x24, [0x0E] = 0x25, [0x0F] = 0x26,
    [0x10] = 0x32, [0x11] = 0x31, [0x12] = 0x18, [0x13] = 0x19,
    [0x14] = 0x10, [0x15] = 0x13, [0x16] = 0x1F, [0x17] = 0x14,
    [0x18] = 0x16, [0x19] = 0x2F, [0x1A] = 0x11, [0x1B] = 0x2D,
    [0x1C] = 0x15, [0x1D] = 0x2C,
    [0x1E] = 0x02, [0x1F] = 0x03, [0x20] = 0x04, [0x21] = 0x05,
    [0x22] = 0x06, [0x23] = 0x07, [0x24] = 0x08, [0x25] = 0x09,
    [0x26] = 0x0A, [0x27] = 0x0B,
    [0x28] = 0x1C, [0x29] = 0x01, [0x2A] = 0x0E, [0x2B] = 0x0F,
    [0x2C] = 0x39, [0x2D] = 0x0C, [0x2E] = 0x0D, [0x2F] = 0x1A,
    [0x30] = 0x1B, [0x31] = 0x2B, [0x33] = 0x27, [0x34] = 0x28,
    [0x35] = 0x29, [0x36] = 0x33, [0x37] = 0x34, [0x38] = 0x35,
};

void proc_despierta_tecla(void);

/* Inyecta tecla USB como PS/2: actualiza modificadores desde el byte
   HID (LCtrl/LShift/LAlt/LGui/RCtrl/RShift/RAlt/RGui), traduce el uso
   con la misma tabla de distribucion y despierta a los lectores. */
void teclado_usb_tecla(uint8_t mod, uint8_t uso, int baja) {
    uint8_t c;
    int may;
    const tecla_t *t;
    sh_izq = (mod & 0x02) ? 1 : 0;
    sh_der = (mod & 0x20) ? 1 : 0;
    ctrl_izq = (mod & 0x01) ? 1 : 0;
    ctrl_der = (mod & 0x10) ? 1 : 0;
    altgr = (mod & 0x40) ? 1 : 0;   /* RAlt como AltGr (ES) */
    if (!baja) return;
    if (uso == 0x39) { bloqmay = !bloqmay; return; }      /* CapsLock */
    if (uso == 0x3A) { muerto = 0; tk_mete(TEC_MENU);      /* F1 = menu */
        proc_despierta_tecla(); return; }
    if (uso == 0x52) { tk_mete_mod(TEC_ARRIBA);            /* flechas */
        proc_despierta_tecla(); return; }
    if (uso == 0x51) { tk_mete_mod(TEC_ABAJO);
        proc_despierta_tecla(); return; }
    if (uso == 0x50) { tk_mete_mod(TEC_IZQ);
        proc_despierta_tecla(); return; }
    if (uso == 0x4F) { tk_mete_mod(TEC_DER);
        proc_despierta_tecla(); return; }
    if (uso == 0x4A) { tk_mete_mod(TEC_INICIO);            /* edicion */
        proc_despierta_tecla(); return; }
    if (uso == 0x4D) { tk_mete_mod(TEC_FIN);
        proc_despierta_tecla(); return; }
    if (uso == 0x4B) { tk_mete_mod(TEC_REPAG);
        proc_despierta_tecla(); return; }
    if (uso == 0x4E) { tk_mete_mod(TEC_AVPA);
        proc_despierta_tecla(); return; }
    if (uso == 0x4C) { tk_mete_mod(TEC_SUPR);
        proc_despierta_tecla(); return; }
    if (uso >= 128) return;
    c = hid_a_scan[uso];
    if (!c) return;
    if (c == 0x1C) { muerto = 0; tk_mete(13);
        proc_despierta_tecla(); return; }
    if (c == 0x0E) { muerto = 0;                          /* retroceso */
        tk_mete((ctrl_izq || ctrl_der) ? 0x7F : 8);
        proc_despierta_tecla(); return; }
    if (c == 0x01) { muerto = 0; tk_mete(27);
        proc_despierta_tecla(); return; }
    if (c == 0x0F) { muerto = 0;                          /* Tab */
        if (ctrl_izq || ctrl_der)
            tk_mete((sh_izq || sh_der) ? TEC_CTRL_MAY_TAB : TEC_CTRL_TAB);
        else if (sh_izq || sh_der)
            tk_mete(TEC_MAY_TAB);
        else
            tk_mete(9);
        proc_despierta_tecla(); return; }
    may = (sh_izq || sh_der);
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
        proc_despierta_tecla();
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
        proc_despierta_tecla();
        return;
    }
}

int kbd_hay(void) {
    uint32_t fl = cpu_eflags();
    int hay;
    cli();
    hay = tk_ini != tk_fin;
    cpu_if_restaura(fl);
    return hay;
}

int kbd_tecla(void) {
    for (;;) {
        uint32_t fl = cpu_eflags();
        cli();
        if (tk_ini != tk_fin) {
            int v = tk_cola[tk_ini];
            tk_ini = (tk_ini + 1) % TK_N;
            cpu_if_restaura(fl);
            return v;
        }
        /* F8: la espera con hlt exige IF=1 para despertar con la IRQ.
         * Si se entro con IF=0 (tarea 0 tras bloquea), restaurar solo
         * no basta: se habilita explicito como hace bloquea/ktimer. */
        sti();
        hlt();
        cli();
        fl = cpu_eflags();
        cli();
        if (tk_ini != tk_fin) {
            int v = tk_cola[tk_ini];
            tk_ini = (tk_ini + 1) % TK_N;
            cpu_if_restaura(fl);
            return v;
        }
        cpu_if_restaura(fl);
        {
            extern void proc_bloquea_tecla(void);
            proc_bloquea_tecla();
        }
    }
}

void teclado_irq(void) {
    extern volatile int ps2_sondeo;
    if (ps2_sondeo) return;   /* F8: sondeo PS/2 en curso, no consumir 0x60 */
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { tk_ext = 1; return; }
    int suelto = sc & 0x80;
    uint8_t c = sc & 0x7F;
    if (tk_ext) {
        tk_ext = 0;
        if (c == 0x38) { altgr = !suelto; return; }
        if (c == 0x1D) { ctrl_der = !suelto; return; }
        if (suelto) return;
        muerto = 0;
        if (c == 0x48) tk_mete_mod(TEC_ARRIBA);
        else if (c == 0x50) tk_mete_mod(TEC_ABAJO);
        else if (c == 0x4B) tk_mete_mod(TEC_IZQ);
        else if (c == 0x4D) tk_mete_mod(TEC_DER);
        else if (c == 0x47) tk_mete_mod(TEC_INICIO);
        else if (c == 0x4F) tk_mete_mod(TEC_FIN);
        else if (c == 0x49) tk_mete_mod(TEC_REPAG);
        else if (c == 0x51) tk_mete_mod(TEC_AVPA);
        /* v2.0-etapa5: teclas Windows/menu (E0 5B/5D) para el escritorio. */
        else if (c == 0x5B || c == 0x5D) tk_mete(TEC_MENU);
        return;
    }
    if (c == 0x2A) { sh_izq = !suelto; return; }
    if (c == 0x36) { sh_der = !suelto; return; }
    if (c == 0x1D) { ctrl_izq = !suelto; return; }   /* Ctrl izq: solo mod */
    if (c == 0x3A) { if (!suelto) bloqmay = !bloqmay; return; }
    if (c == 0x45) { if (!suelto) numlock = !numlock; return; }
    if (c == 0x38 && !(sh_izq || sh_der)) {
        /* Alt izquierdo solo: sin accion (AltGr es E0 0x38). */
        return;
    }
    if (suelto) return;
    if (c == 0x1C) { muerto = 0; tk_mete(13); return; }
    if (c == 0x0E) {
        muerto = 0;
        /* Ctrl+Retroceso = 0x7F (borrado de palabra en el editor) */
        tk_mete((ctrl_izq || ctrl_der) ? 0x7F : 8);
        return;
    }
    if (c == 0x01) { muerto = 0; tk_mete(27); return; }
    if (c == 0x0F) {
        muerto = 0;
        /* Ctrl+Tab = cambia de pestana (0xE120, 0xE220 con May);
           May+Tab = reduce indentacion (0xE121, F32 Lang v0.1);
           el Tab normal (9) no lleva modificador codificado. */
        if (ctrl_izq || ctrl_der)
            tk_mete((sh_izq || sh_der) ? TEC_CTRL_MAY_TAB : TEC_CTRL_TAB);
        else if (sh_izq || sh_der)
            tk_mete(TEC_MAY_TAB);
        else
            tk_mete(9);
        return;
    }
    /* v2.0-etapa5: F1 abre el menu (en QEMU es la via testeable; en HW
       real tambien vale la tecla Menu E0 5B/5D). */
    if (c == 0x3B) { muerto = 0; tk_mete(TEC_MENU); return; }
    /* Supr y numerico */
    if (c == 0x53) {
        if (numlock) { muerto = 0; emite('.'); return; }
        tk_mete_mod(TEC_SUPR);
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
        if (c == 0x47) { tk_mete_mod(TEC_INICIO); return; }
        if (c == 0x48) { tk_mete_mod(TEC_ARRIBA); return; }
        if (c == 0x49) { tk_mete_mod(TEC_REPAG); return; }
        if (c == 0x4B) { tk_mete_mod(TEC_IZQ); return; }
        if (c == 0x4D) { tk_mete_mod(TEC_DER); return; }
        if (c == 0x4F) { tk_mete_mod(TEC_FIN); return; }
        if (c == 0x50) { tk_mete_mod(TEC_ABAJO); return; }
        if (c == 0x51) { tk_mete_mod(TEC_AVPA); return; }
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
