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
#include "ktimer.h"
#include "drv.h"
#include "blk.h"
#include "ps2.h"
#include "teclado.h"
#include "gfx.h"
#include "fs.h"

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
    uart_puts("FacilOS32 v2.0 arranca.\r\n");
    mm_init();
    {
        uint32_t ku, kp, kd, kc;
        int aut = mm_autoprueba();
        kmalloc_info(&ku, &kp, &kd, &kc);
        kprintf("kheap: autoprueba=%d usados=%u pico=%u dobles=%u corr=%u\r\n",
                aut, ku, kp, kd, kc);
    }
    pag_inicia();
    info_memoria();
    proc_inicia();
    idt_init();
    seg_init();
    sti();
    {
        int kta = kt_autoprueba();
        kprintf("ktimer: autoprueba=%d\r\n", kta);
    }
    /* v2.0-etapa3: deteccion de disco ANTES de montar (ramfs la necesita). */
    blk_init();
    /* v2.0: drivers con contrato (pantalla, teclado, pit, ramfs). */
    drv_registra("pantalla", pantalla_init);
    drv_registra("teclado", teclado_init);
    drv_registra("raton", ps2_init);
    drv_registra("pit", pit_drv_init);
    drv_registra("ramfs", ramfs_init);
    drv_init_todos();
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
    /* v2.0: el FS RAM lo inicia su driver (drv_init_todos); shell()
       tambien lo pide (idempotente). */
    /* v2.1-p1: autocuracion de video: si caimos a texto con un modo
       persistente pedido (CMOS 0x40 != 0), se restablece el defecto para
       el proximo arranque (el modo pedido no existe en este video). */
    if (!gfx_activo()) {
        uint8_t cv;
        outb(0x70, 0x40);
        cv = inb(0x71);
        if (cv > 2) {
            outb(0x70, 0x40);
            outb(0x71, 0);
            kprintf("video: modo %u no soportado, restablecido 800x600\r\n",
                    (unsigned)cv);
        }
    }
    /* v2.1-P0: el escritorio renovado arranca directo; la shell queda
       como recuperacion al salir del servidor (o si este falla). */
    /* v2.1-p1: el servidor recibe las dimensiones reales (modos VESA). */
    {
        char sargs[24];
        int o = 0, ww = (int)gfx_w(), hh = (int)gfx_h(), t;
        int vals[2];
        if (ww < 640 || ww > 2048) ww = 800;
        if (hh < 400 || hh > 2048) hh = 600;
        vals[0] = ww;
        vals[1] = hh;
        for (t = 0; t < 2; t++) {
            char rv[12];
            int ri = 0, v = vals[t], j;
            if (t) sargs[o++] = ' ';
            if (!v) sargs[o++] = '0';
            else {
                while (v && ri < 11) {
                    rv[ri++] = (char)('0' + v % 10);
                    v /= 10;
                }
                for (j = ri - 1; j >= 0 && o < 22; j--)
                    sargs[o++] = rv[j];
            }
        }
        sargs[o] = 0;
        {
            int pid = kern_crea_proceso("servidor", sargs, 1);
            if (pid < 0) {
                kprintf("no se pudo crear el proceso servidor (%d): arranca la shell\r\n",
                        pid);
            } else {
                proc_t *m = proc_por_pid(pid);
                if (m) m->grafico = 1;
            }
        }
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
