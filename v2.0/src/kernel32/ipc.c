#include "ipc.h"
#include "pag.h"
#include "cpu.h"
#include "libc.h"

/* Buzon IPC: los campos viven en proc_t (a cero por memset al crear).
 * Todas las entradas/salidas validan punteros de usuario igual que el
 * resto de syscalls (ptr_ok/pag_*); el buzon esta en memoria del nucleo. */

void ipc_init_proceso(proc_t *p) {
    if (!p) return;
    p->buz_ini = p->buz_fin = p->buz_n = 0;
    p->espera_msg = 0;
    for (int i = 0; i < IPC_NMSG; i++) p->buzon[i].ocupado = 0;
}

void ipc_limpia_proceso(proc_t *p) {
    if (!p) return;
    ipc_init_proceso(p);
}

/* Valida [ptr, ptr+n) de usuario para lectura. */
static int lee_ok(int di, uint32_t ptr, uint32_t n) {
    if (di < 0) return 1;
    if (n == 0 || n > IPC_MAX) return 0;
    return pag_valida_usuario((unsigned)di, ptr, n, 0);
}

static int esc_ok(int di, uint32_t ptr, uint32_t n) {
    if (di < 0) return 1;
    if (n == 0) return 0;
    return pag_valida_usuario((unsigned)di, ptr, n, 1);
}

/* Lee memoria de usuario a un bufer del nucleo (el llamante corre con su
 * CR3 e IF=0: lectura directa validada, como el resto de syscalls). */
static void lee_usr(int di, uint32_t ptr, uint8_t *dst, uint32_t n) {
    if (di < 0) {
        for (uint32_t i = 0; i < n; i++) dst[i] = ((uint8_t *)ptr)[i];
        return;
    }
    pag_lee_usr((unsigned)di, ptr, dst, n);
}

static void esc_usr(int di, uint32_t ptr, const uint8_t *src, uint32_t n) {
    if (di < 0) {
        for (uint32_t i = 0; i < n; i++) ((uint8_t *)ptr)[i] = src[i];
        return;
    }
    pag_escribe_usr((unsigned)di, ptr, src, n);
}

__attribute__((optimize("O0"))) void sc_ipc_enviar(regs_t *r, int di) {
    int dst_pid = (int)r->ebx;
    uint32_t len = r->ecx;
    r->eax = (uint32_t)-1;
    if (len == 0 || len > IPC_MAX) return;
    if (!lee_ok(di, r->edx, len)) return;
    {
        proc_t *d = proc_por_pid(dst_pid);
        uint8_t tmp[IPC_MAX];
        if (!d || !d->usado || d->estado == EST_FIN) return;
        if (d->buz_n >= IPC_NMSG) { r->eax = (uint32_t)-2; return; }
        lee_usr(di, r->edx, tmp, len);
        d->buzon[d->buz_fin].ocupado = 1;
        d->buzon[d->buz_fin].emisor = proc_actual ? proc_actual->pid : 0;
        d->buzon[d->buz_fin].len = len;
        for (uint32_t i = 0; i < len; i++)
            d->buzon[d->buz_fin].datos[i] = tmp[i];
        d->buz_fin = (d->buz_fin + 1) % IPC_NMSG;
        d->buz_n++;
        if (d->estado == EST_BLOQ && d->espera_msg) {
            d->espera_msg = 0;
            d->estado = EST_LISTO;
        }
        r->eax = 0;
    }
}

__attribute__((optimize("O0"))) void sc_ipc_hay(regs_t *r, int di) {
    (void)di;
    if (!proc_actual) { r->eax = 0; return; }
    r->eax = (uint32_t)proc_actual->buz_n;
}

__attribute__((optimize("O0"))) void sc_ipc_recibir(regs_t *r, int di) {
    uint32_t max = r->ecx;
    r->eax = (uint32_t)-1;
    if (!esc_ok(di, r->ebx, max ? max : 1)) return;
    if (di >= 0 && !esc_ok(di, r->edx, 4)) return;
    if (!proc_actual) return;
    proc_actual->modo = 0;      /* puede bloquearse dentro de la syscall */
    for (;;) {
        uint32_t fl = cpu_eflags();
        int hay;
        cli();
        hay = proc_actual->buz_n > 0;
        if (!hay) proc_actual->espera_msg = 1;
        cpu_if_restaura(fl);
        if (hay) {
            int i = proc_actual->buz_ini;
            uint32_t len = proc_actual->buzon[i].len;
            uint32_t copia = len < max ? len : max;
            int emisor = proc_actual->buzon[i].emisor;
            esc_usr(di, r->ebx, proc_actual->buzon[i].datos, copia);
            if (di >= 0) {
                if (pag_escribe_usr((unsigned)di, r->edx, &emisor, 4)) return;
            } else {
                *(int *)r->edx = emisor;
            }
            proc_actual->buzon[i].ocupado = 0;
            proc_actual->buz_ini = (i + 1) % IPC_NMSG;
            proc_actual->buz_n--;
            proc_actual->espera_msg = 0;
            r->eax = len;
            return;
        }
        proc_bloquea_msg();
    }
}
