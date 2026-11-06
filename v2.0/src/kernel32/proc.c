#include "proc.h"
#include "pag.h"
#include "ipc.h"
#include "red_udp.h"
#include "libc.h"
#include "cpu.h"
#include "seg.h"
#include "irq.h"

/* FacilOS32 v1.9: procesos de usuario y planificador Round Robin.
 *
 * El planificador entra desde tres sitios, siempre con IF=0:
 *   - la IRQ del PIT (preemptivo),
 *   - una syscall de usuario que se bloquea (dormir, esperar tecla,
 *     esperar hijo, ceder),
 *   - la shell del nucleo cuando espera una tecla.
 * Nunca se ejecuta "en medio" de una estructura critica porque el unico
 * sitio que muta la tabla de procesos es el propio planificador, con
 * las interrupciones apagadas, y las IRQ del teclado/PIT son la unica
 * fuente de cambios de estado.
 */

proc_t proc_tareas[1 + MAXPROC];
proc_t *proc_actual = 0;

static uint32_t loteria = 0;
static int ultimo_pid = 0;

void proc_inicia(void) {
    libc_memset(proc_tareas, 0, sizeof(proc_tareas));
    proc_tareas[0].usado = 1;
    proc_tareas[0].pid = 0;
    proc_tareas[0].nombre[0] = 's';
    proc_tareas[0].nombre[1] = 'h';
    proc_tareas[0].nombre[2] = 'e';
    proc_tareas[0].nombre[3] = 'l';
    proc_tareas[0].nombre[4] = 'l';
    proc_tareas[0].nombre[5] = 0;
    proc_tareas[0].estado = EST_EJECUTO;
    proc_tareas[0].es_usuario = 0;
    proc_tareas[0].di = 0;
    proc_tareas[0].cr3 = pag_dir_nucleo();
    proc_tareas[0].brk = 0;
    /* La tarea del sistema tambien necesita pila de anillo 0 propia: las
       IRQ del PIT y del teclado entran por esp0 mientras duerme. */
    proc_tareas[0].pila_kern = -1;
    uint32_t cima = proc_toma_pila_kernel();
    if (cima) {
        proc_tareas[0].pila_kern = proc_slot_de_pila(cima);
        proc_tareas[0].esp_k = cima;
    }
    for (int i = 0; i < NFD; i++) proc_tareas[0].fds[i] = -1;
    proc_actual = &proc_tareas[0];
}

proc_t *proc_por_indice(int i) {
    if (i < 0 || i > MAXPROC) return 0;
    return proc_tareas[i].usado ? &proc_tareas[i] : 0;
}

proc_t *proc_por_pid(int pid) {
    for (int i = 0; i <= MAXPROC; i++)
        if (proc_tareas[i].usado && proc_tareas[i].pid == pid)
            return &proc_tareas[i];
    return 0;
}

int proc_cuantos(void) {
    int n = 0;
    for (int i = 0; i <= MAXPROC; i++) if (proc_tareas[i].usado) n++;
    return n;
}

int proc_ultimo_pid(void) { return ultimo_pid; }

int proc_hay_graficos(void) {
    for (int i = 0; i <= MAXPROC; i++)
        if (proc_tareas[i].usado && proc_tareas[i].grafico &&
            proc_tareas[i].estado != EST_FIN)
            return 1;
    return 0;
}

/* ---- creacion y destruccion ------------------------------------------ */

static void anota_marco(proc_t *p, uint32_t f) {
    if (p->nmarcos < MARCOS_MAX) {
        p->marcos[p->nmarcos++] = f;
        if ((uint32_t)p->nmarcos > p->pico_marcos)
            p->pico_marcos = (uint32_t)p->nmarcos;
    }
}

void proc_anota_marco(proc_t *p, uint32_t fis) { anota_marco(p, fis); }

/* Pila de anillo 0 por proceso. Vive en 0xE00000 (1 MB, 32 ranuras de
   32 KB) porque el nucleo la usa por direccion directa y la identidad solo
   cubre sus propias zonas. */
#define PILA_KERN_BASE 0xE00000u
#define PILA_KERN_TAM  0x100000u
#define PILA_KERN_RUNAS 32
#define PILA_KERN_PAGS 8
static uint8_t pilas_uso[PILA_KERN_RUNAS / 8];

uint32_t proc_toma_pila_kernel(void) {
    uint32_t fl = cpu_eflags();
    cli();
    for (int i = 0; i < PILA_KERN_RUNAS; i++) {
        if (pilas_uso[i >> 3] & (1u << (i & 7))) continue;
        pilas_uso[i >> 3] |= (uint8_t)(1u << (i & 7));

        cpu_if_restaura(fl);
        return PILA_KERN_BASE + (uint32_t)(i + 1) * (PILA_KERN_TAM /
                                                    PILA_KERN_RUNAS);
    }
    cpu_if_restaura(fl);
    return 0;
}

int proc_slot_de_pila(uint32_t cima) {
    uint32_t paso = PILA_KERN_TAM / PILA_KERN_RUNAS;
    if (cima <= PILA_KERN_BASE || cima > PILA_KERN_BASE + PILA_KERN_TAM)
        return -1;
    return (int)((cima - PILA_KERN_BASE) / paso) - 1;
}

void proc_devuelve_pila_kernel(int slot) {
    uint32_t fl = cpu_eflags();
    cli();
    if (slot < 0 || slot >= PILA_KERN_RUNAS) { cpu_if_restaura(fl); return; }
    pilas_uso[slot >> 3] &= (uint8_t)~(1u << (slot & 7));
    cpu_if_restaura(fl);
}

int proc_crea(const char *nombre, unsigned di, uint32_t eip, uint32_t esp) {
    uint32_t fl = cpu_eflags();
    cli();
    int slot = -1;
    for (int i = 1; i <= MAXPROC; i++) {
        if (!proc_tareas[i].usado) { slot = i; break; }
    }
    if (slot < 0) {
        for (int i = 1; i <= MAXPROC; i++) {
            if (proc_tareas[i].usado && proc_tareas[i].reciclable) {
                slot = i;
                kprintf("REUSE slot=%d pid=%d\n", i, proc_tareas[i].pid);
                break;
            }
        }
    }
    if (slot < 0) { cpu_if_restaura(fl); return -1; }                 /* limite de procesos (G) */
    proc_t *p = &proc_tareas[slot];
    libc_memset(p, 0, sizeof(*p));
    p->usado = 1;
    p->pid = ++ultimo_pid;
    int k = 0;
    while (nombre && nombre[k] && k < 15) { p->nombre[k] = nombre[k]; k++; }
    p->nombre[k] = 0;
    if (!p->nombre[0]) {
        p->nombre[0] = 'p'; p->nombre[1] = (char)('0' + (p->pid % 10));
        p->nombre[2] = 0;
    }
    /* v2.0-etapa2: nace NUEVO (no seleccionable) y kern_crea_proceso lo
       publica a LISTO cuando esp_k/modo estan asignados. Asi no existe
       ventana con proceso a medio inicializar visible al planificador. */
    p->estado = EST_NUEVO;
    p->es_usuario = 1;    p->di = di;
    p->cr3 = pag_dir(di);
    p->eip = eip;
    p->esp = esp;
    p->eflags = 0x202;
    p->ppid = proc_actual ? proc_actual->pid : 0;
    p->brk = 0x300000;
    p->rc = -1;
    p->pila_kern = -1;
    for (int i = 0; i < NFD; i++) p->fds[i] = -1;
    cpu_if_restaura(fl);
    return p->pid;
}

void proc_libera(proc_t *p) {
    /* Marcos de usuario y de kernel: todos los que apunto el proceso. */
    for (int i = 0; i < p->nmarcos; i++) pag_devuelve_marco(p->marcos[i]);
    p->nmarcos = 0;
    if (p->di) {
        /* Desmapear antes de devolver el directorio al pool. */
        pag_desmapa(p->di, 0x200000, PAG_TAMANO);   /* codigo, pila, heap 1 */
        pag_desmapa(p->di, 0x400000, PAG_MARCO * 128); /* heap 2 */
    }
    p->usado = 0;
    p->estado = EST_NUEVO;
}

/* Recoge los procesos terminados: devuelve sus marcos al pool. Nunca se
   hace desde el propio proceso que muere (su pila de kernel esta en uno
   de esos marcos): lo llama el planificador desde otro contexto. */
static uint32_t fugas_detectadas = 0;
uint32_t proc_fugas(void) { return fugas_detectadas; }

/* Registra una region virtual del proceso (0 ok, -1 tabla llena). */
int proc_vma_add(proc_t *p, uint32_t ini, uint32_t tam, uint32_t tipo) {
    int i;
    if (!p || !tam) return -1;
    if (tipo == VMAR_HEAP) {
        /* El heap crece monotono desde 0x300000 pero con brk sin alinear
           los tramos se solapan: union con la region existente. */
        for (i = 0; i < p->nvmars; i++) {
            if (p->vmars[i].tipo == VMAR_HEAP &&
                ini <= p->vmars[i].ini + p->vmars[i].tam) {
                uint32_t fin = ini + tam;
                uint32_t fin_old = p->vmars[i].ini + p->vmars[i].tam;
                if (fin > fin_old)
                    p->vmars[i].tam = fin - p->vmars[i].ini;
                return 0;
            }
        }
    }
    if (p->nvmars >= VMAR_N) return -1;
    p->vmars[p->nvmars].ini = ini;
    p->vmars[p->nvmars].tam = tam;
    p->vmars[p->nvmars].tipo = tipo;
    p->nvmars++;
    return 0;
}

void proc_fusiona(void) {
    for (int i = 1; i <= MAXPROC; i++) {
        proc_t *p = &proc_tareas[i];
        if (!p->usado || p->estado != EST_FIN) continue;
        if (p == proc_actual) continue;
        if (p->reciclable) continue;         /* ya liberado */
        ipc_limpia_proceso(p);            /* descarta mensajes pendientes */
        red_udp_limpia_proceso((uint32_t)p->pid);  /* libera sus ligas */
        /* Chequeo de destruccion: las paginas de usuario presentes en las
           regiones registradas deben coincidir con los marcos anotados.
           Una diferencia indica fuga (mapeado sin anotar) o residuo. */
        {
            uint32_t presentes = 0;
            for (int v = 0; v < p->nvmars; v++)
                presentes += pag_cuenta_rango(p->di, p->vmars[v].ini,
                                              p->vmars[v].tam);
            if (presentes != (uint32_t)p->nmarcos) {
                fugas_detectadas++;
                kprintf("FUGA pid=%d presentes=%u anotados=%d nvmars=%d\n",
                        p->pid, presentes, p->nmarcos, p->nvmars);
                for (int v = 0; v < p->nvmars; v++)
                    kprintf("  vma%d tipo=%u ini=%x tam=%x pres=%u\n", v,
                            p->vmars[v].tipo, p->vmars[v].ini,
                            p->vmars[v].tam,
                            pag_cuenta_rango(p->di, p->vmars[v].ini,
                                             p->vmars[v].tam));
            }
        }
        if (p->pila_kern >= 0) {
            proc_devuelve_pila_kernel(p->pila_kern);
            p->pila_kern = -1;
        }
        for (int k = 0; k < p->nmarcos; k++) pag_devuelve_marco(p->marcos[k]);
        p->nmarcos = 0;
        if (p->di) {
            /* Las paginas de usuario ya se devolvieron por marcos[] (ahi
               estan codigo, pila y heap: ver anota_rango y syscall 7). No
               se desmapea por rangos: el directorio es privado y al
               liberarlo caen sus dos tablas con todo lo mapeado. Desmapear
               0x200000 con PAG_TAMANO (4 MB) alcanzaba las copias
               supervisor de 0x480000-0x5FFFFF y liberaba marcos
               reservados del nucleo. */
            pag_libera_dir((int)p->di);
            p->di = 0;
        }
        p->reciclable = 1;
    }
}

void proc_despierta_hijos(int pid) {
    proc_t *p = proc_por_pid(pid);
    if (p && (p->estado == EST_BLOQ || p->estado == EST_LISTO))
        p->estado = EST_LISTO;
}

void proc_termina(proc_t *p, int rc) {
    p->rc = rc;
    p->estado = EST_FIN;
    if (p->ppid >= 0) proc_despierta_hijos(p->ppid);
}

/* ---- contexto --------------------------------------------------------- */

void proc_guarda_usuario(proc_t *p, const void *marco) {
    const uint32_t *f = (const uint32_t *)((const uint8_t *)marco +
                                            sizeof(regs_t));
    /* f[0]=EIP, f[1]=CS, f[2]=EFLAGS, f[3]=ESP, f[4]=SS */
    p->edi = ((const regs_t *)marco)->edi;
    p->esi = ((const regs_t *)marco)->esi;
    p->ebp = ((const regs_t *)marco)->ebp;
    p->ebx = ((const regs_t *)marco)->ebx;
    p->edx = ((const regs_t *)marco)->edx;
    p->ecx = ((const regs_t *)marco)->ecx;
    p->eax = ((const regs_t *)marco)->eax;
    p->eip = f[0];
    if ((f[1] & 3) == 3) {          /* venia de anillo 3 */
        p->eflags = f[2];
        p->esp = f[3];
    }
}

/* Al ceder la CPU hay que guardar el ESP exacto, los tres registros que
   la convencion de llamada declara persistentes y el EBP del llamante.
   Sin el EBP, al reanudar en anillo 0 el codigo retomado usaria el EBP
   del proceso que cedia (otra pila) y corromperia sus variables locales.
   Se hace aqui dentro del cambio, antes de ninguna llamada, para que el
   ESP guardado sea el del punto exacto de vuelta. k_ebp es el EBP que
   tenia el llamante de cede_a (esta en la base de su marco, *marco). */
#define GUARDA_GPR(p) do { \
        uint32_t _marco = (uint32_t)__builtin_frame_address(0); \
        uint32_t _esp = _marco + 4; \
        uint32_t _ebx, _esi, _edi; \
        __asm__ volatile ("movl %%ebx, %0\n\t movl %%esi, %1\n\t movl %%edi, %2" \
                          : "=r"(_ebx), "=r"(_esi), "=r"(_edi)); \
        (p)->esp_guard = _esp; \
        (p)->k_ebx = _ebx; \
        (p)->k_esi = _esi; \
        (p)->k_edi = _edi; \
        (p)->k_ebp = *(volatile uint32_t *)_marco; \
    } while (0)

/* Retoma un proceso de usuario: monta el marco de iret en su pila de
   kernel y entra en anillo 3. No retorna. */
static void entra_usuario(proc_t *p) {
    uint32_t *sp = (uint32_t *)p->esp_k;
    *(--sp) = 0x23;                       /* ss  (datos DPL3) */
    *(--sp) = p->esp;                     /* esp */
    *(--sp) = p->eflags | 0x200;          /* eflags con IF=1 */
    *(--sp) = 0x1B;                       /* cs  (codigo DPL3) */
    *(--sp) = p->eip;                     /* eip */
    tss_pon_esp0(p->esp_k);
    {
        extern void salta_usuario(uint32_t sp, uint32_t ebx, uint32_t esi,
                                  uint32_t edi, uint32_t ebp, uint32_t eax,
                                  uint32_t ecx, uint32_t edx);
        salta_usuario((uint32_t)sp, p->ebx, p->esi, p->edi, p->ebp,
                      p->eax, p->ecx, p->edx);
        for (;;) hlt();
    }
}

/* Reanuda en anillo 0: la tarea del sistema (shell) o un proceso que se
   quedo bloqueado dentro de una syscall. En el segundo caso el CR3 debe
   ser el del proceso: si no, al salir del manejador con iret el codigo de
   usuario se ejecutaria con el mapa del nucleo y petaria con #PF.
   El "ret" de retoma_kernel recoge la direccion de vuelta que dejo la
   llamada a cambia. Restaura tambien el EBP del llamante (ver
   GUARDA_GPR): sin el, el codigo retomado direccionaria sus variables
   locales en la pila equivocada. Esta en NASM porque GCC no permite
   tocar EBP desde asm en linea cuando la funcion usa marco de pila. */
__attribute__((optimize("O0"))) static void entra_kernel(proc_t *p) {
    extern void retoma_kernel(uint32_t a, uint32_t b, uint32_t c,
                              uint32_t d, uint32_t e);
    pag_usa(p->es_usuario ? p->di : 0);
    tss_pon_esp0(p->esp_k);
    retoma_kernel(p->k_ebx, p->k_esi, p->k_edi, p->k_ebp, p->esp_guard);
    for (;;) hlt();
}


/* Cede la CPU a p. La barrera de ensamblador que va detras de la llamada
   es imprescindible: sin ella el compilador la convierte en salto de cola
   (cambia no retorna nunca), la direccion de vuelta desaparece de la pila
   y al reanudar el proceso el "ret" salta a donde toque. Al volver, la
   tarea sigue justo despues de la llamada, como si hubiera retornado. */
#define CEDE(p) do { cede_a(p); __asm__ volatile ("" ::: "memory"); } while (0)

static int elige_otra(void);

/* Cambia a p. No retorna si habia alguien a quien ceder. */
__attribute__((optimize("O0"))) static void cede_a(proc_t *p) {
    proc_t *de = proc_actual;
    if (de && de->estado == EST_EJECUTO)
        de->estado = EST_LISTO;
    if (de) {
        GUARDA_GPR(de);
    }
    proc_actual = p;
    p->estado = EST_EJECUTO;
    p->loteria = ++loteria;
    p->cedes++;
    if (p->es_usuario && p->modo) {
        pag_usa(p->di);
        entra_usuario(p);
    } else {
        entra_kernel(p);
    }
}

/* Bloquea la tarea actual y no vuelve hasta que una IRQ la despierta.
   El estado pasa a LISTO en proc_despierta_tecla / proc_tick /
   proc_despierta_hijos. Con IF=0 al entrar y al salir.
   Etapa2: no pisa un LISTO concurrente (un despertar entre el preset del
   llamante y esta funcion debe sobrevivir; el llamante revalida). */
static void bloquea(void) {
    if (proc_actual && proc_actual->estado != EST_LISTO)
        proc_actual->estado = EST_BLOQ;
    for (;;) {
        int i = elige_otra();
        if (i >= 0) {
            CEDE(&proc_tareas[i]);
            return;                       /* solo si no hemos cambiado */
        }
        /* Nadie ejecutable: esperamos a que una IRQ cree trabajo. */
        sti();
        hlt();
        cli();
        if (proc_actual->estado != EST_BLOQ) return;
    }
}

void proc_cede(void) {
    if (proc_actual) proc_actual->estado = EST_LISTO;
    int i = elige_otra();
    if (i < 0) {
        /* Nadie mas: seguimos nosotros (ceder no es bloquease). */
        if (proc_actual) proc_actual->estado = EST_EJECUTO;
        return;
    }
    CEDE(&proc_tareas[i]);
}

void proc_bloquea_tecla(void) {
    /* Etapa2: fija BLOQ bajo cli junto al flag. Si una tecla llega entre
       el flag y el BLOQ, el despertar no veria BLOQ y se perderia hasta la
       siguiente tecla. */
    uint32_t fl = cpu_eflags();
    cli();
    if (proc_actual) {
        proc_actual->espera_tecla = 1;
        proc_actual->modo = 0;   /* se reanuda dentro de la syscall */
        proc_actual->estado = EST_BLOQ;
    }
    cpu_if_restaura(fl);
    bloquea();
    if (proc_actual) proc_actual->espera_tecla = 0;
}

void proc_bloquea_ticks(uint32_t hasta) {
    if (proc_actual) {
        proc_actual->dormir_hasta = hasta;
        proc_actual->modo = 0;
    }
    bloquea();
    if (proc_actual) proc_actual->dormir_hasta = 0;
}

/* Bloqueo generico para esperas con despertar dedicado (IPC): el llamante
   pone modo=0 y su flag de espera antes de entrar. El despertador (enviar)
   es otra syscall: en mononucleo no se intercala aqui (necesita la CPU que
   tiene el que espera), asi que basta el BLOQ normal de bloquea(). */
void proc_bloquea_msg(void) {
    bloquea();
}

/* Raton: igual patron que tecla (despertador async en IRQ12). */
void proc_bloquea_raton(void) {
    uint32_t fl = cpu_eflags();
    cli();
    if (proc_actual) {
        proc_actual->espera_raton = 1;
        proc_actual->modo = 0;
        proc_actual->estado = EST_BLOQ;
    }
    cpu_if_restaura(fl);
    bloquea();
    if (proc_actual) proc_actual->espera_raton = 0;
}

int proc_espera_hijo(int pid) {
    if (proc_actual) proc_actual->modo = 0;
    for (;;) {
        proc_t *hijo = proc_por_pid(pid);
        if (!hijo || hijo->estado == EST_FIN) {
            if (hijo) {
                return hijo->rc;
            }
            return -1;
        }
        bloquea();
    }
}

void proc_despierta_tecla(void) {
    for (int i = 0; i <= MAXPROC; i++) {
        if (proc_tareas[i].usado && proc_tareas[i].estado == EST_BLOQ &&
            proc_tareas[i].espera_tecla) {
            proc_tareas[i].estado = EST_LISTO;
            proc_tareas[i].espera_tecla = 0;
        }
    }
}

void proc_tick(void) {
    uint32_t t = irq_ticks();
    for (int i = 0; i <= MAXPROC; i++) {
        proc_t *p = &proc_tareas[i];
        if (!p->usado || p->estado != EST_BLOQ) continue;
        if (p->dormir_hasta && (int32_t)(t - p->dormir_hasta) >= 0) {
            p->dormir_hasta = 0;
            p->estado = EST_LISTO;
        }
    }
}

void proc_estado_actual(void) {
    if (proc_actual) proc_actual->estado = EST_EJECUTO;
}

/* El proceso actual muere (syscall 0 o excepcion). Sus marcos se devuelven
   mas tarde, en proc_fusiona(), porque ahora mismo esta corriendo sobre
   su pila de kernel. No retorna. */
void proc_muere(int rc) {
    proc_t *p = proc_actual;

    if (!p || !p->es_usuario) for (;;) hlt();
    p->rc = rc;
    p->estado = EST_FIN;
    proc_despierta_hijos(p->ppid);
    for (;;) {
        int i = elige_otra();
        if (i >= 0) {
            CEDE(&proc_tareas[i]);
            return;                 /* solo si no llego a cambiar */
        }
        /* Nadie mas: esperamos a que una IRQ cree trabajo. */
        {
            static uint8_t pila_ociosa[4096] __attribute__((aligned(16)));
            uint32_t cima_ociosa = (uint32_t)pila_ociosa + sizeof(pila_ociosa);
            extern void tss_pon_esp0(uint32_t);
            tss_pon_esp0(cima_ociosa);
            sti();
            hlt();
            cli();
            if (proc_actual) tss_pon_esp0(proc_actual->esp_k);
        }
    }
}

/* Siguiente ejecutable en round robin, sin repetir el actual. */
static int elige_otra(void) {
    int n = 1 + MAXPROC;
    int ini = (int)(proc_actual - proc_tareas);
    for (int k = 1; k <= n; k++) {
        int i = (ini + k) % n;
        if (proc_tareas[i].usado && proc_tareas[i].estado == EST_LISTO)
            return i;
    }
    return -1;
}


