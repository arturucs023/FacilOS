#include "irq.h"
#include "libc.h"
#include "cpu.h"
#include "proc.h"

/* IRQ y excepciones de FacilOS32 v1.9. */

static volatile uint32_t ticks = 0;

void teclado_irq(void);
void proc_cede(void);
void proc_fusiona(void);
void proc_tick(void);
void proc_despierta_tecla(void);
void proc_guarda_usuario(proc_t *p, const void *marco);
void proc_termina(proc_t *p, int rc);
extern void shell(void);
extern int shell_anidada;

static void (*manej[16])(void) = { 0 };

void irq_escucha(uint8_t irq, void (*h)(void)) {
    if (irq < 16) manej[irq] = h;
}

uint32_t irq_ticks(void) { return ticks; }

static const char *nombres_ex[] = {
    "division por cero", "depuracion", "NMI", "breakpoint", "desbordo",
    "fuera de rango", "instruccion invalida", "copro no presente",
    "doble falta", "tarea invalida", "segmento ausente", "pila rota",
    "reservada", "proteccion general", "fallo de pagina", "reservada",
    "x87", "alineacion", "maquina", "SIMD", "virtualizacion"
};

void irq_c(uint32_t *rp) {
    regs_t *r = (regs_t *)rp;
    if (r->num < 32 || r->num > 47) {
        outb(0x20, 0x20);
        return;
    }
    uint32_t n = r->num - 32;
    if (n == 0) {
        ticks++;
        if (proc_actual) proc_actual->ticks_cpu++;   /* observabilidad RR */
    } else if (n == 1) {
        teclado_irq();
        proc_despierta_tecla();
    }
    if (manej[n]) manej[n]();
    if (n >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);

    /* Preemptivo: el PIT marca el final de la ofreta del proceso. Solo
       se roba la CPU si la IRQ vino de anillo 3 (ahi el marco trae el
       contexto completo de usuario). Dentro de una syscall, incluida la
       terminal anidada, el proceso no se interrumpe: cede cuando pide
       una tecla, duerme o espera a un hijo. */
    proc_t *p = proc_actual;
    proc_tick();
    proc_fusiona();
    if (n == 0 && p && p->es_usuario) {
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

static void panic(const char *que, regs_t *r) {
    const uint32_t *f = (const uint32_t *)((const uint8_t *)r + sizeof(regs_t));
    uint32_t esp;
    __asm__ volatile ("movl %%esp, %0" : "=r"(esp));
    cli();
    uart_puts("\r\nPANICO ");
    uart_puts(que);
    uart_puts(" num=");
    kprintf("%u err=%x cs=%x eip=%x esp=%x cr2=%x", r->num, r->err, f[1],
            f[0], esp, cpu_cr2());
    proc_t *q = proc_actual;
    if (q) {
        kprintf(" tarea pid=%d di=%u modo=%d esp_guard=%x esp_k=%x estado=%d",
                q->pid, q->di, q->modo, q->esp_guard, q->esp_k, q->estado);
    }
    uart_puts("\r\n");
    if (r->num < 20) {
        uart_puts("  ");
        uart_puts(nombres_ex[r->num]);
        uart_puts("\r\n");
    }
    {
        uint32_t eb;
        __asm__ volatile ("movl %%ebp, %0" : "=r"(eb));
        kprintf("  esp:");
        for (int i = 0; i < 8; i++)
            kprintf(" %x", *(volatile uint32_t *)esp + i);
        kprintf("\r\n  ebp=%x ret:", eb);
        for (int i = 0; i < 10 && eb > 0x8000 && eb < 0xA0000; i++) {
            kprintf(" %x", *(volatile uint32_t *)(eb + 4));
            eb = *(volatile uint32_t *)eb;
        }
        kprintf("\r\n");
    }
    for (;;) hlt();
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
