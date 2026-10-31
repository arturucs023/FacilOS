#include "libc.h"
#include "cpu.h"
#include "irq.h"
#include "teclado.h"
#include "fs.h"
#include "shell.h"
#include "seg.h"
#include "exec.h"
#include "cd.h"
#include "cdcat.h"
#include "pag.h"
#include "proc.h"
#include "bootinfo.h"

/* v1.9: el arranque enciende la paginacion y el planificador antes de
   tocar nada mas. Despues crea el menu como PROCESO (espacio de
   direcciones propio) y esta funcion se queda como la tarea 0 del
   sistema: espera a que no quede ninguna aplicacion grafica viva y
   entonces entra en la shell de texto. */

static void info_memoria(void) {
    kprintf("memoria: E820 %s (%d entradas), RAM util %u KB, marcos %u de %u libres\r\n",
            pag_e820_valido() ? "ok" : "alternativa", pag_e820_entradas(),
            pag_ram_total() / 1024, pag_marcos_libres(), pag_marcos_totales());
    kprintf("paginacion: CR0=%x CR3=%x PD=%x ventana=%x\r\n", cpu_cr0(),
            cpu_cr3(), pag_dir_nucleo(), PAG_VENTANA);
    {
        const unsigned char *t = (const unsigned char *)0x5000;
        kprintf("E820: n=%d tabla:", BI->e820n);
        for (int i = 0; i < 8; i++) kprintf(" %d", t[i]);
        kprintf("\r\n");
    }
}

void kmain(void)
{
    uart_init();
    uart_puts("FacilOS32 v1.9 arranca.\r\n");
    mm_init();
    pag_inicia();
    info_memoria();
    proc_inicia();
    idt_init();
    seg_init();
    sti();
    cons_init();
    cdcat_inicia();
    {
        extern int cdcat_cuantos(void);
        extern const char *cdcat_nombre(int i);
        extern int cdcat_tam(int i);
        extern int cdcat_lba(int i);
        kprintf("programas en la ISO: %d\r\n", cdcat_cuantos());
        for (int i = 0; i < cdcat_cuantos(); i++) {
            const unsigned char *h = cd_sector(cdcat_lba(i));
            kprintf("  cat ");
            kprintf(cdcat_nombre(i));
            kprintf(" lba=");
            kprintf("%d", cdcat_lba(i));
            kprintf(" tam=");
            kprintf("%d", cdcat_tam(i));
            kprintf(" magic=");
            kprintf("%c%c%c%c\r\n", h[0], h[1], h[2], h[3]);
        }
    }
    fs_init();

    /* El menu arranca como proceso grafico, igual que en v1.7/v1.8. */
    int pid = kern_crea_proceso("menu", "", 1);
    if (pid < 0) {
        kprintf("no se pudo crear el proceso menu (%d): arranca la shell\r\n",
                pid);
    } else {
        proc_t *m = proc_por_pid(pid);
        if (m) m->grafico = 1;
    }
    /* Tarea 0 (esta funcion): espera a que no haya procesos graficos
       vivos y da la consola de texto. Mientras tanto, los procesos de
       usuario usan la CPU solos. */
    for (;;) {
        if (!proc_hay_graficos()) break;
        proc_bloquea_ticks(irq_ticks() + 5);
    }
    cons_clear();
    shell();
    cons_puts("Fin de sesion. Puedes apagar la maquina.\n");
    for (;;) hlt();
}
