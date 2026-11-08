#include "irq.h"
#include "libc.h"
#include "cpu.h"
#include "proc.h"
#include "ktimer.h"
#include "apic.h"
#include "hal.h"
#include "klog.h"

/* F8: IRQ y excepciones. Despacho por HAL con nombre; la IRQ solo
 * descuenta/dispara y pide EOI. La fusion de procesos terminados se
 * difiere a contexto de tarea (nunca kprintf con EOI pendiente). */

void teclado_irq(void);
void proc_cede(void);
void proc_fusion_si_toca(void);
void proc_fusion_pendiente(void);
void proc_tick(void);
void proc_despierta_tecla(void);
void proc_guarda_usuario(proc_t *p, const void *marco);
void proc_termina(proc_t *p, int rc);
extern void shell(void);
extern int shell_anidada;

static void irq_pit(void) {
    hal_tick();
    if (proc_actual) proc_actual->ticks_cpu++;   /* observabilidad RR */
    kt_tick();
}

static void irq_teclado(void) {
    teclado_irq();
    proc_despierta_tecla();
}

void irq_escucha(uint8_t irq, void (*h)(void)) {
    /* Compatibilidad (ps2.c): delega en la HAL con nombre generico. */
    if (!h) {
        hal_irq_libera(irq);
        return;
    }
    hal_irq_registra(irq, h, "legado");
}

void irq_init(void) {
    hal_irq_registra(0, irq_pit, "pit");
    hal_irq_registra(1, irq_teclado, "teclado");
    /* IRQ12 la registra ps2_init ("raton") en su sitio. */
}

uint32_t irq_ticks(void) { return hal_ticks(); }

static const char *nombres_ex[] = {
    "division por cero", "depuracion", "NMI", "breakpoint", "desbordo",
    "fuera de rango", "instruccion invalida", "copro no presente",
    "doble falta", "tarea invalida", "segmento ausente", "pila rota",
    "reservada", "proteccion general", "fallo de pagina", "reservada",
    "x87", "alineacion", "maquina", "SIMD", "virtualizacion"
};

static void irq_eoi(uint8_t n) {
    if (n == 7) {
        /* Espuria del maestro: EOI solo si hay ISR real. */
        outb(0x20, 0x0B);
        if (!(inb(0x20) & 0x80)) return;
        outb(0x20, 0x20);
        return;
    }
    if (n == 15) {
        /* Espuria del esclavo: EOI al maestro, al esclavo solo si ISR. */
        outb(0xA0, 0x0B);
        if (!(inb(0xA0) & 0x80)) {
            outb(0x20, 0x20);
            return;
        }
    }
    hal_irq_fin(n);
}

void irq_c(uint32_t *rp) {
    regs_t *r = (regs_t *)rp;
    if (r->num < 32 || r->num > 47) {
        outb(0x20, 0x20);
        return;
    }
    {
        uint32_t n = r->num - 32;
        hal_irq_fn h = hal_irq_manej((uint8_t)n);
        if (h) h();
        irq_eoi((uint8_t)n);
    }

    /* Preemptivo: el PIT marca el final de la ofreta del proceso. Solo
       se roba la CPU si la IRQ vino de anillo 3 (ahi el marco trae el
       contexto completo de usuario). Dentro de una syscall, incluida la
       terminal anidada, el proceso no se interrumpe: cede cuando pide
       una tecla, duerme o espera a un hijo. */
    proc_t *p = proc_actual;
    proc_tick();
    proc_fusion_pendiente();   /* F8: la fusion corre en tarea, no aqui */
    if (r->num == 32 && p && p->es_usuario) {
        const uint32_t *f = (const uint32_t *)((const uint8_t *)r +
                                                sizeof(regs_t));
        if ((f[1] & 3) == 3) {
            proc_guarda_usuario(p, r);
            p->modo = 1;
            proc_cede();
            if (proc_actual) proc_estado_actual();
        }
    }
}

/* ---- excepciones ------------------------------------------------------ */

static void panic_hex(const char *t, uint32_t v) {
    uart_puts(t);
    uart_puts("=0x");
    for (int i = 7; i >= 0; i--) {
        uint32_t d = (v >> (i * 4)) & 15u;
        char c = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        char s[2] = { c, 0 };
        uart_puts(s);
    }
}

static int en_rango_nucleo(uint32_t a) {
    /* Pila de nucleo y codigo: evita page-fault dentro del panic. */
    if (a >= 0x100000 && a < 0x200000) return 1;
    if (a >= 0xE00000 && a < 0xF00000) return 1;
    return 0;
}

static void panic(const char *que, regs_t *r) {
    const uint32_t *f = (const uint32_t *)((const uint8_t *)r + sizeof(regs_t));
    uint32_t esp;
    __asm__ volatile ("movl %%esp, %0" : "=r"(esp));
    cli();
    uart_puts("\r\nPANICO ");
    uart_puts(que);
    panic_hex(" num", r->num);
    panic_hex(" err", r->err);
    panic_hex(" eip", f[0]);
    panic_hex(" esp", esp);
    panic_hex(" cr2", cpu_cr2());
    {
        proc_t *q = proc_actual;
        if (q && en_rango_nucleo((uint32_t)q)) {
            panic_hex(" pid", (uint32_t)q->pid);
            panic_hex(" cr3", cpu_cr3());
        }
    }
    uart_puts("\r\n");
    if (r->num < 20) {
        uart_puts("  ");
        uart_puts(nombres_ex[r->num]);
        uart_puts("\r\n");
    }
    if (en_rango_nucleo(esp)) {
        uart_puts("  esp:");
        for (int i = 0; i < 8; i++) {
            uint32_t a = esp + (uint32_t)i * 4;
            if (!en_rango_nucleo(a)) break;
            panic_hex(" ", *(volatile uint32_t *)a);
        }
        uart_puts("\r\n");
    }
    {
        uint32_t eb;
        __asm__ volatile ("movl %%ebp, %0" : "=r"(eb));
        if (en_rango_nucleo(eb)) {
            uart_puts("  ebp ret:");
            for (int i = 0; i < 10 && en_rango_nucleo(eb) &&
                 en_rango_nucleo(eb + 4); i++) {
                panic_hex(" ", *(volatile uint32_t *)(eb + 4));
                eb = *(volatile uint32_t *)eb;
            }
            uart_puts("\r\n");
        }
    }
    klog(KLOG_FATAL, "exc", "panico de nucleo");
}

void excepcion_c(regs_t *r) {
    proc_t *p = proc_actual;
    const uint32_t *f = (const uint32_t *)((const uint8_t *)r + sizeof(regs_t));
    uint32_t cs = f[1];
    /* Excepcion en anillo 0: el kernel esta roto, no se sigue. */
    if ((cs & 3) != 3 || !p || !p->es_usuario) {
        panic("nucleo:", r);
        return;
    }
    /* Excepcion de un proceso de usuario: se muere solo ese. */
    p->ultimo_exc = (int)r->num;
    kprintf("EXCEPCION pid=%d di=%u cr3=%x modo=%d esp=%x %s num=%u eip=%x err=%x cr2=%x\n",
            p->pid, p->di, cpu_cr3(), p->modo, p->esp,
            r->num < 20 ? nombres_ex[r->num] : "desconocida", r->num, f[0],
            r->err, (r->num == 14) ? cpu_cr2() : 0);
    proc_muere(-1);      /* no retorna */
}
