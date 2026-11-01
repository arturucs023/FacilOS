#include "ps2.h"
#include "proc.h"
#include "pag.h"
#include "cpu.h"
#include "libc.h"
#include "apic.h"

/* Raton PS/2 via i8042/aux (IRQ12). Protocolo estandar: reset, defaults,
 * enable reporting; paquetes de 3 B (botones+signos, dx, dy). */

#define K_EST 0x64
#define K_DAT 0x60

#define COLA_N 64

static volatile raton_t cola[COLA_N];
static volatile int c_ini = 0, c_fin = 0;
static volatile int hay_raton = 0;

static int cur_x = 400, cur_y = 300, cur_b = 0;
static int fase = 0;
static uint8_t pb0 = 0;
static int dx_ac = 0;

static int espera_in(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(K_EST) & 0x02)) return 0;
    }
    return -1;
}

static int espera_out(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(K_EST) & 0x01) return 0;
    }
    return -1;
}

/* Escribe un byte al auxiliar (con prefijo 0xD4). 0 ok. */
static int aux_pon(uint8_t b) {
    if (espera_in()) return -1;
    outb(K_EST, 0xD4);
    if (espera_in()) return -1;
    outb(K_DAT, b);
    return 0;
}

/* Lee respuesta (0xFA esperado) con reintentos. */
static int aux_resp(void) {
    for (int t = 0; t < 3; t++) {
        if (espera_out()) return -1;
        if (inb(K_DAT) == 0xFA) return 0;
    }
    return -1;
}

static void mete(int x, int y, int b) {
    int s = (c_fin + 1) % COLA_N;
    if (s == c_ini) return;             /* llena: se descarta (documentado) */
    cola[c_fin].x = x;
    cola[c_fin].y = y;
    cola[c_fin].botones = b;
    c_fin = s;
    hay_raton = 1;
    /* despierta a quien espere raton (igual patron que teclado) */
    for (int i = 0; i <= MAXPROC; i++) {
        if (proc_tareas[i].usado && proc_tareas[i].estado == EST_BLOQ &&
            proc_tareas[i].espera_raton) {
            proc_tareas[i].estado = EST_LISTO;
            proc_tareas[i].espera_raton = 0;
        }
    }
}

void raton_irq(void) {
    uint8_t e = inb(K_EST);
    uint8_t b;
    if (!(e & 0x01)) return;    /* sin dato: vector espurio/compartido */
    b = inb(K_DAT);
    if (!(e & 0x20)) return;    /* es de teclado: lo lee su IRQ1 */
    if (fase == 0) {
        if (!(b & 0x08)) return;        /* desincronizado: se ignora */
        pb0 = b;
        fase = 1;
        return;
    }
    if (fase == 1) {
        dx_ac = (pb0 & 0x10) ? (int)b - 256 : (int)b;
        fase = 2;
        return;
    }
    {
        int dy = (pb0 & 0x20) ? (int)b - 256 : (int)b;
        cur_x += dx_ac;
        if (cur_x < 0) cur_x = 0;
        if (cur_x > 799) cur_x = 799;
        cur_y -= dy;                    /* PS/2 Y+ es arriba */
        if (cur_y < 0) cur_y = 0;
        if (cur_y > 599) cur_y = 599;
        cur_b = pb0 & 0x07;
        fase = 0;
        mete(cur_x, cur_y, cur_b);
    }
}

int raton_hay(void) {
    int h;
    uint32_t fl = cpu_eflags();
    cli();
    h = (c_ini != c_fin);
    cpu_if_restaura(fl);
    return h;
}

int raton_lee(raton_t *dst) {
    for (;;) {
        uint32_t fl = cpu_eflags();
        cli();
        if (c_ini != c_fin) {
            if (dst) *dst = cola[c_ini];
            c_ini = (c_ini + 1) % COLA_N;
            if (c_ini == c_fin) hay_raton = 0;
            cpu_if_restaura(fl);
            return 0;
        }
        cpu_if_restaura(fl);
        hlt();
        /* recheck tras hlt (igual que kbd_tecla): lo llegado en la
           ventana se consume sin bloquear. */
        fl = cpu_eflags();
        cli();
        if (c_ini != c_fin) {
            if (dst) *dst = cola[c_ini];
            c_ini = (c_ini + 1) % COLA_N;
            if (c_ini == c_fin) hay_raton = 0;
            cpu_if_restaura(fl);
            return 0;
        }
        cpu_if_restaura(fl);
        {
            extern void proc_bloquea_raton(void);
            proc_bloquea_raton();
        }
    }
}


/* Cuerpo con IF ya a 0: las respuestas en 0x60 no pueden ser robadas
   por la IRQ1 (el manejador de teclado las tomaria por teclas). */
static int ps2_init_nolock(void) {
    /* Etapa4: SeaBIOS deja los puertos PS/2 vivos (el teclado funciona sin
       init). Se intenta la secuencia completa, pero lo esencial (handler
       IRQ12 + desmascara) se hace siempre: si el raton esta vivo (BIOS lo
       dejo reportando o acepta el 0xF4), sus IRQ llegan igual. */
    uint8_t ctr;
    while (inb(K_EST) & 0x01) inb(K_DAT);
    if (!espera_in()) {
        outb(K_EST, 0xA8);
        if (!espera_in()) {
            outb(K_EST, 0x20);
            if (!espera_out()) {
                ctr = inb(K_DAT);
                ctr = (uint8_t)((ctr & ~0x20) | 0x02);   /* IRQ12 on, reloj aux on */
                if (!espera_in()) {
                    outb(K_EST, 0x60);
                    if (!espera_in()) outb(K_DAT, ctr);
                }
            }
        }
    }
    /* Secuencia best-effort (fallo silencioso: hay_raton dira si vive). */
    if (!aux_pon(0xFF) && !aux_resp()) {
        for (int t = 0; t < 3; t++) {
            if (espera_out()) break;
            if (inb(K_DAT) == 0xAA) break;
        }
        aux_pon(0xF6);
        aux_resp();
        aux_pon(0xF4);
        aux_resp();
    }
    irq_escucha(12, raton_irq);
    outb(0xA1, 0xEF);                  /* desmascara IRQ12 (i8259) */
    /* Etapa4: la cascada del i8259 no entrega el esclavo en este QEMU;
       la IRQ12 va por IOAPIC (mismo vector 44). Si no hay APIC, queda
       el i8259 por compatibilidad con HW real. */
    apic_irq12 = (apic_ruta_raton() == 0);
    kprintf("ps2: raton IRQ12 via %s\r\n",
            apic_irq12 ? "IOAPIC" : "i8259");
    c_ini = c_fin = 0;
    hay_raton = 0;
    fase = 0;
    return 0;
}
int ps2_init(void) {
    uint32_t fl = cpu_eflags();
    int r;
    cli();
    r = ps2_init_nolock();
    cpu_if_restaura(fl);
    return r;
}



__attribute__((optimize("O0"))) void sc_raton(regs_t *r, int di) {
    uint32_t v[3];
    raton_t e;
    r->eax = (uint32_t)-1;
    if (di >= 0 && !pag_valida_usuario((unsigned)di, r->ebx, 12, 1)) return;
    if (proc_actual && proc_actual->es_usuario) proc_actual->modo = 0;
    if (raton_lee(&e)) return;
    v[0] = (uint32_t)e.x;
    v[1] = (uint32_t)e.y;
    v[2] = (uint32_t)e.botones;
    if (di >= 0) {
        if (pag_escribe_usr((unsigned)di, r->ebx, v, 12)) return;
    } else {
        for (int i = 0; i < 3; i++) ((uint32_t *)r->ebx)[i] = v[i];
    }
    r->eax = 0;
}

__attribute__((optimize("O0"))) void sc_raton_hay(regs_t *r, int di) {
    (void)di;
    r->eax = (uint32_t)raton_hay();
}
