#include "teclado.h"
#include "irq.h"
#include "cpu.h"
#include "libc.h"
#include "libc.h"

int teclado_es = 1;

typedef struct { uint8_t scan, base, mayus; } tecla_t;
/* Misma tabla que FacilOS 16-bit (CP437). */
static const tecla_t tabla[] = {
    {0x10,'q','Q'}, {0x11,'w','W'}, {0x12,'e','E'}, {0x13,'r','R'},
    {0x14,'t','T'}, {0x15,'y','Y'}, {0x16,'u','U'}, {0x17,'i','I'},
    {0x18,'o','O'}, {0x19,'p','P'}, {0x1E,'a','A'}, {0x1F,'s','S'},
    {0x20,'d','D'}, {0x21,'f','F'}, {0x22,'g','G'}, {0x23,'h','H'},
    {0x24,'j','J'}, {0x25,'k','K'}, {0x26,'l','L'}, {0x27,0xA4,0xA5},
    {0x2C,'z','Z'}, {0x2D,'x','X'}, {0x2E,'c','C'}, {0x2F,'v','V'},
    {0x30,'b','B'}, {0x31,'n','N'}, {0x32,'m','M'},
    {0x02,'1','!'}, {0x03,'2','"'}, {0x04,'3',250}, {0x05,'4','$'},
    {0x06,'5','%'}, {0x07,'6','&'}, {0x08,'7','/'}, {0x09,'8','('},
    {0x0A,'9',')'}, {0x0B,'0','='}, {0x29,167,166}, {0x0C,'-','_'},
    /* v1.8: los signos salen en ASCII, no en CP437 (167/168). Los programas
       de usuario comparan con '+' (43) y '*' (42), y la shell solo acepta
       32..126, asi que con CP437 esas dos teclas se perdian. */
    {0x0D,'+','*'}, {0x1B,'+','*'}, {0x33,',',';'}, {0x34,'.',':'},
    {0x35,'/','_'}, {0x39,' ',' '}, {0,0,0}
};

#define TK_N 64
static volatile int tk_cola[TK_N];
/* Indices modulo TK_N: sin el modulo, a partir de 64 teclas se escribe
   fuera del array y se corrompe la BSS vecina. */
static volatile int tk_ini = 0, tk_fin = 0;
static int tk_ext = 0;
static int sh_izq = 0, sh_der = 0, bloqmay = 0;

static void tk_mete(int v) {
    int s = (tk_fin + 1) % TK_N;
    if (s == tk_ini) return;
    tk_cola[tk_fin] = v;
    tk_fin = s;
}

void teclado_pon(int es) { teclado_es = es ? 1 : 0; }

void teclado_irq(void) {
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { tk_ext = 1; return; }
    int suelto = sc & 0x80;
    uint8_t c = sc & 0x7F;
    if (tk_ext) {
        tk_ext = 0;
        if (suelto) return;
        if (c == 0x48) tk_mete(TEC_ARRIBA);
        else if (c == 0x50) tk_mete(TEC_ABAJO);
        else if (c == 0x4B) tk_mete(TEC_IZQ);
        else if (c == 0x4D) tk_mete(TEC_DER);
        return;
    }
    if (c == 0x2A) { sh_izq = !suelto; return; }
    if (c == 0x36) { sh_der = !suelto; return; }
    if (c == 0x3A) { if (!suelto) bloqmay = !bloqmay; return; }
    if (suelto) return;
    if (c == 0x1C) { tk_mete(13); return; }
    if (c == 0x0E) { tk_mete(8); return; }
    if (c == 0x01) { tk_mete(27); return; }
    if (!teclado_es) {
        /* Americano: ASCII directo del BIOS no disponible; tabla base latina */
        for (const tecla_t *t = tabla; t->scan; t++) {
            if (t->scan == c) {
                int may = (sh_izq || sh_der);
                int ch = may ? t->mayus : t->base;
                /* sin enie: mapea 0xA4->n */
                if (ch == 0xA4) ch = may ? 'N' : 'n';
                if (ch == 0xA5) ch = 'N';
                tk_mete(ch);
                return;
            }
        }
        return;
    }
    int may = (sh_izq || sh_der);
    for (const tecla_t *t = tabla; t->scan; t++) {
        if (t->scan != c) continue;
        int ch = may ? t->mayus : t->base;
        if (bloqmay && ch >= 'a' && ch <= 'z') ch -= 32;
        if (bloqmay && ch == 0xA4) ch = 0xA5;
        tk_mete(ch);
        return;
    }
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
        /* v1.9: si seguimos sin tecla, cede la CPU en vez de
           esperar en bucle con la IRQResolution dormida. La IRQ de
           teclado nos devuelve aqui (estado LISTO). */
        {
            extern void proc_bloquea_tecla(void);
            proc_bloquea_tecla();
        }
    }
}
