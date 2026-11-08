#include "exec.h"
#include "libc.h"
#include "cpu.h"
#include "irq.h"
#include "gfx.h"
#include "fs.h"
#include "vfs.h"
#include "teclado.h"
#include "seg.h"
#include "proc.h"
#include "pag.h"
#include "ipc.h"
#include "ps2.h"
#include "drv_red.h"
#include "red.h"
#include "red_arp.h"
#include "red_ip.h"
#include "red_icmp.h"
#include "red_udp.h"
#include "red_tcp.h"

/* v1.9: descriptores y heap son POR PROCESO (ver proc.h). El bloque del
   proceso con el que estamos se toma de proc_actual, de modo que la
   shell anidada (syscall 26, que corre en el contexto del proceso) ve
   exactamente los mismos ficheros. */
int exec_base_dir = 0;
regs_t *syscall_regs_ptr = 0;

/* v1.6: monton usuario 0x300000-0x480000 (1.5 MB), igual que v1.8 para
   no cambiar los limites que ya usan las aplicaciones. */
#define UHEAP_FIN 0x480000
#define UBRK_INI 0x300000

static int *fds(void) { return proc_actual->fds; }
static uint32_t *fds_pos(void) { return proc_actual->fds_pos; }
static uint32_t *user_brk(void) { return &proc_actual->brk; }
/* NOTA v2.2-f32: aqui hubo un fds_init() que reiniciaba fds[] Y brk
   en cada abrir/crear. Eso rompia dos cosas reales y verificadas:
   (a) el heap Ring3 se solapaba (malloc tras fopen devolvia un
   bloque ya usado: f32 leia SRC==LINI); (b) dos ficheros abiertos
   a la vez compartian fd (copiar producia ficheros vacios con
   "verificacion" vacua). Cada proceso ya nace con fds/brk
   inicializados en proc_crea (tarea 0 al arranque), asi que el
   reinicio por llamada no solo es innecesario sino incorrecto. */

/* ruta (abs o relativa a exec_base_dir) -> dir + nombre base. 0 ok. */
static int parte_sys(const char *ruta, int *dir, char *base) {
    char tmp[128];
    uint32_t i = 0;
    while (i < 127 && ruta[i]) { tmp[i] = ruta[i]; i++; }
    tmp[i] = 0;
    if (!tmp[0]) return -1;
    if (tmp[0] == '/') {
        /* v2.2: rutas absolutas multinivel (\"/T/f.txt\"). Antes solo se
           admitia un componente (\"/T\"); el resto se rechazaba y ningun
           programa Ring3 podia abrir/crear dentro de subcarpetas.
           Extension compatible: lo antes valido resuelve igual; lo antes
           rechazado ahora funciona via fs_resuelve (.., . incluidos). */
        uint32_t m = 0, f, l;
        while (tmp[m]) m++;
        while (m > 1 && tmp[m - 1] == '/') m--;
        tmp[m] = 0;
        if (m <= 1) return -1;
        f = m;
        while (f > 0 && tmp[f - 1] != '/') f--;
        l = m - f;
        if (!l || l > 31) return -1;
        for (uint32_t j = 0; j < l; j++) base[j] = tmp[f + j];
        base[l] = 0;
        if (f <= 1) {
            *dir = 0;
            return 0;
        }
        {
            char nd[128];
            uint32_t n = f - 1;
            if (n > 127) return -1;
            for (uint32_t j = 0; j < n; j++) nd[j] = tmp[j];
            nd[n] = 0;
            *dir = fs_resuelve(nd);
            return *dir < 0 ? -1 : 0;
        }
    }
    uint32_t j = 0;
    while (tmp[j] && tmp[j] != '/' && j < 31) {
        base[j] = tmp[j];
        j++;
    }
    base[j] = 0;
    if (!base[0] || tmp[j]) return -1;
    *dir = exec_base_dir;
    return 0;
}

/* ---------------------------------------------------------------------
   Cargador F32X de v1.9. Cada ejecucion crea un PROCESO con su propio
   directorio de paginas, sus paginas de codigo/datos, su pila de usuario
   (con pagina de guardia) y su pila de anillo 0. El proceso entra en
   anillo 3 y el planificador (proc.c) se encarga de la CPU.

   Devuelve el pid del proceso creado, o un error:
     -100 no existe, -101 no ejecutable, -102 corrupto, -103 error de
     carga, -104 sin memoria para otro proceso, -105 sin paginas.
   --------------------------------------------------------------------- */

typedef struct {
    int idx;        /* indice en el FS, o en el catalogo si es_cd */
    int es_cd;
} origen_t;

static int abre_origen(const char *base, origen_t *o) {
    int idx = vfs_actual->hijo(exec_base_dir, base);
    if (idx >= 0 && vfs_actual->tipo(idx) == 2) {
        o->idx = idx;
        o->es_cd = 0;
        return 0;
    }
    extern int cdcat_buscar(const char *);
    int ci = cdcat_buscar(base);
    if (ci < 0) return -1;
    o->idx = ci;
    o->es_cd = 1;
    return 0;
}

static int lee_origen(origen_t *o, uint32_t off, void *buf, uint32_t n) {
    if (o->es_cd) {
        extern int cdcat_leer(int i, uint32_t off, void *buf, uint32_t n);
        int r = cdcat_leer(o->idx, off, buf, n);
        return r < 0 ? r : (int)n;
    }
    int r = vfs_actual->leer_parcial(o->idx, off, buf, n);
    return r <= 0 ? r : (int)n;
}

static uint32_t origen_tam(origen_t *o) {
    if (o->es_cd) {
        extern int cdcat_tam(int i);
        return (uint32_t)cdcat_tam(o->idx);
    }
    return (uint32_t)vfs_actual->tam(o->idx);
}

/* Pila de anillo 0 del proceso (8 paginas = 32 KB). */
#define PILA_KERN_PAGS 8

static int anota_rango(proc_t *p, unsigned di, uint32_t virt, uint32_t tam) {
    for (uint32_t v = virt; v < virt + tam; v += PAG_MARCO) {
        uint32_t f;
        if (pag_translada(di, v, &f)) return -1;
        proc_anota_marco(p, f & ~(PAG_MARCO - 1));
    }
    return 0;
}

int kern_crea_proceso(const char *ruta, const char *args, int es_grafico) {
    char base[32];
    int dir;
    static uint8_t hdr[20];
    static uint8_t trozo[512];
    uint32_t ver, ent, tam, hc, bss, plano, off, fr;
    if (parte_sys(ruta, &dir, base)) return -100;
    origen_t o;
    if (abre_origen(base, &o)) return -100;
    fr = origen_tam(&o);
    if (fr < 16) return -101;
    if (lee_origen(&o, 0, hdr, 20) != 20) return -103;
    if (hdr[0] != 'F' || hdr[1] != '3' || hdr[2] != '2' || hdr[3] != 'X')
        return -101;
    ver = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
          ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    ent = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) |
          ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
    tam = (uint32_t)hdr[12] | ((uint32_t)hdr[13] << 8) |
          ((uint32_t)hdr[14] << 16) | ((uint32_t)hdr[15] << 24);
    hc = 16;
    bss = 0;
    if (ver == 2 || ver == 3) {
        bss = (uint32_t)hdr[16] | ((uint32_t)hdr[17] << 8) |
              ((uint32_t)hdr[18] << 16) | ((uint32_t)hdr[19] << 24);
        hc = 20;
    } else if (ver != 1) {
        return -102;
    }
    plano = tam < hc ? tam : tam - hc;
    /* F8: el segundo tope (hc+plano+bss) es redundante con el primero
     * (plano+bss), pero se conserva como guarda de cabecera. */
    if (tam != fr || ent >= plano || bss > 0x40000 ||
        plano + bss > 0xC0000 || hc + plano + bss > 0xE0000)
        return -102;

    int di = pag_nuevo_dir();
    if (di < 0) return -104;
    uint32_t usr = PAG_PRESENTE | PAG_ESCRITURA | PAG_USUARIO;
    /* codigo + datos + BSS (las paginas nacen a cero) */
    if (pag_mapa_nuevas(di, 0x200000, plano + bss, usr)) {
        pag_libera_dir(di);
        return -105;
    }
    /* pila de usuario: 0x2E0000-0x2EFFFF. Debajo (0x2D0000) queda sin
       mapear a proposito: es la pagina de guardia. */
    if (pag_mapa_nuevas(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO, usr)) {
        pag_desmapa(di, 0x200000, plano + bss);
        pag_libera_dir(di);
        return -105;
    }
    /* argumentos del programa en 0x2F0000 (u_args). El esp inicial
     * apunta al inicio: el `call _main` de start.asm apila hacia abajo
     * (0x2EFFFFC) y los args quedan intactos encima. Apuntar al final
     * los pisaba con la direccion de retorno (dhcp leia basura). */
    uint32_t esp_ini = 0x2F0000;
    if (args) {
        uint32_t k = 0;
        while (k < 255 && args[k]) k++;
        if (pag_escribe_usr(di, 0x2F0000, args, k + 1)) {
            pag_desmapa(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
            pag_desmapa(di, 0x200000, plano + bss);
            pag_libera_dir(di);
            return -103;
        }
    }
    /* copiar el plano */
    off = hc;
    while (off < tam) {
        uint32_t n = tam - off > 512 ? 512 : tam - off;
        if (lee_origen(&o, off, trozo, (int)n) != (int)n) {
            pag_desmapa(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
            pag_desmapa(di, 0x200000, plano + bss);
            pag_libera_dir(di);
            return -103;
        }
        if (pag_escribe_usr(di, 0x200000 + (off - hc), trozo, n)) {
            pag_desmapa(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
            pag_desmapa(di, 0x200000, plano + bss);
            pag_libera_dir(di);
            return -105;
        }
        off += n;
    }
    /* pila de anillo 0: zona reservada 0xE00000, mapeada por identidad */
    uint32_t cima = proc_toma_pila_kernel();
    if (!cima) {
        pag_desmapa(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
        pag_desmapa(di, 0x200000, plano + bss);
        pag_libera_dir(di);
        return -105;
    }
    int pid = proc_crea(base, di, 0x200000 + ent, esp_ini);
    if (pid < 0) {
        proc_devuelve_pila_kernel(proc_slot_de_pila(cima));
        pag_desmapa(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
        pag_desmapa(di, 0x200000, plano + bss);
        pag_libera_dir(di);
        return -104;
    }
    proc_t *p = proc_por_pid(pid);
    p->esp_k = cima;
    p->pila_kern = proc_slot_de_pila(cima);
    p->modo = 1;
    p->cod_pags = (plano + bss + PAG_MARCO - 1) / PAG_MARCO;
    if (es_grafico) p->grafico = 1;
    /* los marcos que se han pedido hasta ahora quedan anotados para
       poder devolverlos al terminar el proceso.
       F8: si la anotacion o la VMA fallan, se aborta (transaccional):
       antes se ignoraba y la fusion delataba FUGA con marcos huerfanos. */
    if (anota_rango(p, di, 0x200000, plano + bss) ||
        anota_rango(p, di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO)) {
        proc_devuelve_pila_kernel(proc_slot_de_pila(cima));
        pag_desmapa(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
        pag_desmapa(di, 0x200000, plano + bss);
        pag_libera_dir(di);
        return -105;
    }
    /* regiones virtuales del proceso (v2.0) */
    if (proc_vma_add(p, 0x200000, plano + bss, VMAR_CODIGO) ||
        proc_vma_add(p, 0x2E0000, PILA_USR_PAGS * PAG_MARCO, VMAR_PILA)) {
        proc_devuelve_pila_kernel(proc_slot_de_pila(cima));
        pag_desmapa(di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
        pag_desmapa(di, 0x200000, plano + bss);
        pag_libera_dir(di);
        return -105;
    }
    /* Publicacion atomica: hasta aqui nadie podia verlo (NUEVO). */
    {
        uint32_t fl = cpu_eflags();
        cli();
        p->estado = EST_LISTO;
        cpu_if_restaura(fl);
    }
    return pid;
}

/* Lanzar y esperar: la semantica que necesitan el menu (syscall 25) y la
   shell. Crea el proceso y bloquea al padre hasta que termine. */
int kern_ejecutar2(int dir, const char *base, const char *args, int fresco) {
    char ruta[128];
    uint32_t k = 0, j = 0;
    (void)dir;
    (void)fresco;
    while (base && base[j] && k < 120) ruta[k++] = base[j++];
    ruta[k] = 0;
    int pid = kern_crea_proceso(ruta, args, 0);
    if (pid < 0) return pid;
    if (!proc_actual) return pid;          /* arranque: no espera */
    return proc_espera_hijo(pid);
}

int kern_ejecutar(int dir, const char *base, const char *args) {
    return kern_ejecutar2(dir, base, args, 1);
}

/* Terminal anidada (syscall 26). */
int shell_anidada = 0;

/* Propietario del framebuffer (politica grafica de v1.9, ver LEEME). */
static int gfx_pid = -1;

/* Devuelve el indice de directorio del proceso que llama, o -1 si la
   llamada viene del nucleo (shell del sistema). */
static int di_actual(void) {
    if (proc_actual && proc_actual->es_usuario) return (int)proc_actual->di;
    return -1;
}

/* Valida un puntero de usuario antes de tocarlo. Las llamadas hechas
   desde el nucleo (shell) pasan di<0 y no se validan. */
static int ptr_ok(int di, uint32_t p, uint32_t n, int esc) {
    if (di < 0) return 1;
    return pag_valida_usuario((unsigned)di, p, n, esc);
}

/* Pide al proceso que se haga grafico y comprueba que es el dueno del
   dorso: si otro proceso esta dibujando, se le cede (politica de v1.9).
   v2.2-post: el cambio de dueno YA NO limpia el dorso. Servidor y
   clientes comparten un unico dorso de composicion (todos repintan lo
   suyo: fondo/cromo/contenido) y las apps modales limpian explicito;
   limpiar aqui borraba el cromo del servidor en cada DRAW del cliente
   (ventanas sin marco). */
static int gfx_toma(int di) {
    if (di < 0) return 1;
    int pid = proc_actual->pid;
    if (proc_actual->grafico == 0) proc_actual->grafico = 1;
    if (gfx_pid != pid) {
        gfx_pid = pid;
    }
    return 1;
}

static int gfx_es_dueno(int di) {
    if (di < 0) return 1;
    return gfx_pid == proc_actual->pid;
}

/* Marco de syscall del proceso actual tras reanudar: el global
   syscall_regs_ptr lo puede haber cambiado otro proceso mientras este
   cedia la CPU, asi que se usa el guardado por proceso (que es el
   puntero r de entrada, en su propia pila de kernel, intacta). */
static regs_t *regs_actual(regs_t *r) {
    if (proc_actual && proc_actual->sc_regs)
        return (regs_t *)proc_actual->sc_regs;
    return r ? r : syscall_regs_ptr;
}

/* ---- syscalls v2.0: tabla central de despacho --------------------
 * Cada handler conserva intacta la semantica del case original.
 * Todos llevan optimize("O0"): ver nota en syscall_c (Fase 6).
 * 34-35 reservadas para IPC (etapa 1, sin implementar). */
__attribute__((optimize("O0"))) static void sc_00(regs_t *r, int di)
{
        /* salir: termina el proceso y el planificador sigue con otro. */
        proc_muere((int)r->ebx);
        return;
}

__attribute__((optimize("O0"))) static void sc_01(regs_t *r, int di)
 {
        uint32_t len = r->ecx;
        if (len > 2048) len = 2048;
        if (!ptr_ok(di, r->ebx, len, 0)) { r->eax = (uint32_t)-1; return; }
        const char *p = (const char *)r->ebx;
        for (uint32_t i = 0; i < len && p[i]; i++) cons_putc(p[i]);
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_02(regs_t *r, int di)
{
        r->eax = irq_ticks();
        return;
}

__attribute__((optimize("O0"))) static void sc_03(regs_t *r, int di)
 {
        /* v2.2-f32: sin fds_init (ver nota arriba): los fds y el brk
           viven mientras vive el proceso. */
        const char *pa = (const char *)r->ebx;
        char ruta[128];
        uint32_t i = 0;
        if (!ptr_ok(di, r->ebx, 1, 0)) { r->eax = (uint32_t)-1; return; }
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        int dir;
        char base[32];
        r->eax = (uint32_t)-1;
        if (!parte_sys(ruta, &dir, base)) {
            int idx = vfs_actual->hijo(dir, base);
            if (idx >= 0 && vfs_actual->tipo(idx) == 2) {
                for (int f = 0; f < NFD; f++) {
                    if (fds()[f] < 0) {
                        fds()[f] = idx;
                        fds_pos()[f] = 0;
                        r->eax = (uint32_t)f;
                        break;
                    }
                }
            }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_04(regs_t *r, int di)
 {
        /* v1.6: lectura por offset con trozos de 512 (ficheros grandes). */
        int fd = (int)r->ebx;
        uint8_t *dst = (uint8_t *)r->ecx;
        uint32_t n = r->edx;
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ecx, n ? n : 1, 1)) return;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            if (!n) { r->eax = 0; return; }
            static uint8_t tmp[512];
            uint16_t t = vfs_actual->tam(fds()[fd]);
            uint32_t p = fds_pos()[fd];
            if (p >= t) { r->eax = 0; return; }
            if (n > (uint32_t)(t - p)) n = t - p;
            uint32_t hechas = 0;
            while (hechas < n) {
                uint32_t trozo = n - hechas;
                if (trozo > 512) trozo = 512;
                int le = vfs_actual->leer_parcial(fds()[fd], p + hechas, tmp, trozo);
                if (le <= 0) { if (!hechas) r->eax = (uint32_t)le; break; }
                for (int i = 0; i < le; i++) dst[hechas + i] = tmp[i];
                hechas += (uint32_t)le;
                if ((uint32_t)le < trozo) break;
            }
            if (hechas) { fds_pos()[fd] = p + hechas; r->eax = hechas; }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_05(regs_t *r, int di)
 {
        /* v1.6: escribe en la posicion actual (offset), extiende.
           En pos 0 con fichero viejo NO trunca (usar 24). */
        int fd = (int)r->ebx;
        const uint8_t *src = (const uint8_t *)r->ecx;
        uint32_t n = r->edx;
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ecx, n ? n : 1, 0)) return;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            static uint8_t tmp[512];
            uint32_t p = fds_pos()[fd];
            uint32_t hechas = 0;
            int mal = 0;
            while (hechas < n) {
                uint32_t trozo = n - hechas;
                if (trozo > 512) trozo = 512;
                for (uint32_t i = 0; i < trozo; i++) tmp[i] = src[hechas + i];
                if (vfs_actual->escribir_parcial(fds()[fd], p + hechas, tmp, trozo)) {
                    mal = 1;
                    break;
                }
                hechas += trozo;
            }
            if (!mal && !vfs_actual->sincroniza()) {
                fds_pos()[fd] = p + hechas;
                r->eax = hechas;
            }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_06(regs_t *r, int di)
 {
        int fd = (int)r->ebx;
        r->eax = (uint32_t)-1;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            fds()[fd] = -1;
            r->eax = 0;
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_07(regs_t *r, int di)
 {   /* memoria(n): sbrk con paginas del proceso.
     * F8: atomico. Antes mapeaba/anotaba a medias y aun asi avanzaba
     * brk (fuga + FUGA espuria); con n==0 mapeaba una pagina gratis.
     * La cota es el rango [UBRK_INI, UHEAP_FIN): la presencia se crea
     * aqui, no se exige antes (pag_valida_usuario seria circular). */
        uint32_t n = r->ebx;
        uint32_t *b = user_brk();
        r->eax = (uint32_t)-1;
        if (di < 0) return;                    /* la shell usa kmalloc */
        if (n == 0) { r->eax = *b; return; }
        if (n > UHEAP_FIN - *b) return;
        {
            uint32_t viejo = *b;
            uint32_t nuevo = viejo + n;
            uint32_t ini = viejo & ~(PAG_MARCO - 1);
            int nmarcos0 = proc_actual->nmarcos;
            /* Solo las paginas aun no presentes necesitan marco nuevo;
             * el rollback solo deshace esas (las previas se conservan). */
            uint32_t nuevas[384];
            int nn = 0;
            for (uint32_t v = ini; v < nuevo; v += PAG_MARCO) {
                if (pag_presente((unsigned)di, v)) continue;
                if (nn < 384) nuevas[nn++] = v;
                else { proc_actual->nmarcos = nmarcos0; return; }
            }
            for (int i = 0; i < nn; i++) {
                uint32_t f;
                uint32_t v = nuevas[i];
                if (pag_mapa_nuevas((unsigned)di, v, PAG_MARCO,
                                    PAG_PRESENTE | PAG_ESCRITURA |
                                    PAG_USUARIO)) {
                    for (int j = 0; j < i; j++)
                        pag_desmapa((unsigned)di, nuevas[j], PAG_MARCO);
                    proc_actual->nmarcos = nmarcos0;
                    return;
                }
                if (pag_translada((unsigned)di, v, &f)) {
                    for (int j = 0; j <= i; j++)
                        pag_desmapa((unsigned)di, nuevas[j], PAG_MARCO);
                    proc_actual->nmarcos = nmarcos0;
                    return;
                }
                if (proc_anota_marco_w(proc_actual, f & ~(PAG_MARCO - 1))) {
                    for (int j = 0; j <= i; j++)
                        pag_desmapa((unsigned)di, nuevas[j], PAG_MARCO);
                    proc_actual->nmarcos = nmarcos0;
                    return;
                }
            }
            if (proc_vma_add(proc_actual, ini, nuevo - ini, VMAR_HEAP)) {
                for (int j = 0; j < nn; j++)
                    pag_desmapa((unsigned)di, nuevas[j], PAG_MARCO);
                proc_actual->nmarcos = nmarcos0;
                return;
            }
            *b = nuevo;
            r->eax = viejo;
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_08(regs_t *r, int di)
 {   /* leer_tecla(): bloquea el proceso, no la CPU entera */
        extern int kbd_tecla(void);
        if (di < 0) {
            r->eax = (uint32_t)kbd_tecla();
            return;
        }
        if (!kbd_hay()) {
            proc_actual->modo = 0;      /* reanuda en anillo 0 */
            proc_bloquea_tecla();
            r = regs_actual(r);
        }
        r->eax = (uint32_t)kbd_tecla();
        return;
    }


__attribute__((optimize("O0"))) static void sc_09(regs_t *r, int di)
 {
        /* v2.2-f32: sin fds_init (ver nota arriba). */
        const char *pa = (const char *)r->ebx;
        char ruta[128];
        uint32_t i = 0;
        int dir;
        char base[32];
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ebx, 1, 0)) return;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        if (!parte_sys(ruta, &dir, base)) {
            int idx = vfs_actual->hijo(dir, base);
            if (idx < 0) idx = vfs_actual->crear(2, dir, base);
            if (idx >= 0) {
                for (int f = 0; f < NFD; f++) {
                    if (fds()[f] < 0) {
                        fds()[f] = idx;
                        fds_pos()[f] = 0;
                        if (vfs_actual->sincroniza()) { fds()[f] = -1; break; }
                        r->eax = (uint32_t)f;
                        break;
                    }
                }
            }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_10(regs_t *r, int di)
{
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; return; }
        g_pixel((int)r->ebx, (int)r->ecx, r->edx);
        r->eax = 0;
        return;
}

__attribute__((optimize("O0"))) static void sc_11(regs_t *r, int di)
{
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; return; }
        g_linea((int)r->ebx, (int)r->ecx, (int)r->edx, (int)r->esi,
                r->edi);
        r->eax = 0;
        return;
}

__attribute__((optimize("O0"))) static void sc_12(regs_t *r, int di)
{
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; return; }
        g_rect((int)r->ebx, (int)r->ecx, (int)r->edx, (int)r->esi,
               r->edi);
        r->eax = 0;
        return;
}

__attribute__((optimize("O0"))) static void sc_13(regs_t *r, int di)
 {
        const char *p = (const char *)r->edx;
        uint32_t len = r->esi;
        if (len > 256) len = 256;
        if (!ptr_ok(di, r->edx, len ? len : 1, 0)) { r->eax = (uint32_t)-1; return; }
        gfx_toma(di);
        char tmp[257];
        uint32_t i = 0;
        while (i < len && p[i]) { tmp[i] = p[i]; i++; }
        tmp[i] = 0;
        g_texto((int)r->ebx, (int)r->ecx, tmp, r->edi);
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_14(regs_t *r, int di)
{
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; return; }
        g_limpiar(r->ebx);
        r->eax = 0;
        return;
}

__attribute__((optimize("O0"))) static void sc_15(regs_t *r, int di)
 {
        /* buscar: fd, off (con signo), base 0=ini 1=act 2=fin -> pos. */
        int fd = (int)r->ebx;
        int32_t d = (int32_t)r->ecx;
        uint32_t base = r->edx;
        r->eax = (uint32_t)-1;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0 && base <= 2) {
            int32_t t = (int32_t)vfs_actual->tam(fds()[fd]);
            int32_t p = (int32_t)fds_pos()[fd];
            int32_t np = base == 0 ? d : (base == 1 ? p + d : t + d);
            if (np >= 0 && np <= t) {
                fds_pos()[fd] = (uint32_t)np;
                r->eax = (uint32_t)np;
            }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_16(regs_t *r, int di)
 {
        /* borrar: ruta (fichero o carpeta vacia). */
        const char *pa = (const char *)r->ebx;
        char ruta[128];
        uint32_t i = 0;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        int dir;
        char base[32];
        r->eax = (uint32_t)-1;
        if (!parte_sys(ruta, &dir, base)) {
            int idx = vfs_actual->hijo(dir, base);
            if (idx > 0) {
                /* Propaga el codigo real (-2 carpeta no vacia): antes
                   se aplanaba a -1 y ningun programa podia distinguirlo
                   (mismo patron que renombrar). */
                int rr = vfs_actual->borrar(idx);
                if (!rr && !vfs_actual->sincroniza()) r->eax = 0;
                else r->eax = (uint32_t)rr;
            }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_17(regs_t *r, int di)
 {   /* dormir(ms): bloquea hasta el tick; no hay bucle activo */
        uint32_t ms = r->ebx;
        if (ms > 600000) ms = 600000;
        r->eax = 0;
        if (di < 0) {
            uint32_t fin = irq_ticks() + (ms + 9) / 10 + 1;
            while ((int32_t)(irq_ticks() - fin) < 0) hlt();
            return;
        }
        uint32_t hasta = irq_ticks() + (ms + 9) / 10 + 1;
        proc_actual->modo = 0;
        proc_bloquea_ticks(hasta);
        r = regs_actual(r);
        return;
    }


__attribute__((optimize("O0"))) static void sc_18(regs_t *r, int di)
{
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; return; }
        g_destino((int)r->ebx ? 1 : 0);
        r->eax = 0;
        return;
}

__attribute__((optimize("O0"))) static void sc_19(regs_t *r, int di)
{
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; return; }
        g_presentar((int)r->ebx, (int)r->ecx, (int)r->edx,
                    (int)r->esi);
        r->eax = 0;
        return;
}

__attribute__((optimize("O0"))) static void sc_20(regs_t *r, int di)
 {
        const uint32_t *sp = (const uint32_t *)r->edx;
        uint32_t w, h;
        r->eax = (uint32_t)-1;
        if (!gfx_es_dueno(di)) return;
        if (sp && ptr_ok(di, r->edx, 8, 0) && (w = sp[0]) >= 1 && w <= 512 &&
            (h = sp[1]) >= 1 && h <= 512 && w * h <= 262144 &&
            ptr_ok(di, r->edx + 8, w * h * 4, 0)) {
            g_sprite((int)r->ebx, (int)r->ecx, (int)w, (int)h, &sp[2]);
            r->eax = 0;
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_21(regs_t *r, int di)
 {
        const uint32_t *sp = (const uint32_t *)r->edx;
        uint32_t w, h;
        r->eax = (uint32_t)-1;
        if (!gfx_es_dueno(di)) return;
        if (sp && ptr_ok(di, r->edx, 8, 0) && (w = sp[0]) >= 1 && w <= 512 &&
            (h = sp[1]) >= 1 && h <= 512 && w * h <= 262144 &&
            ptr_ok(di, r->edx + 8, w * h * 4, 0)) {
            g_sprite_t((int)r->ebx, (int)r->ecx, (int)w, (int)h, &sp[2],
                       r->esi);
            r->eax = 0;
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_22(regs_t *r, int di)
 {
        const int *op = (const int *)r->ebx;
        r->eax = (uint32_t)-1;
        if (!gfx_es_dueno(di)) return;
        if (op && ptr_ok(di, r->ebx, 24, 0)) {
            g_blit(op[0], op[1], op[2], op[3], op[4], op[5]);
            r->eax = 0;
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_23(regs_t *r, int di)
{
        r->eax = g_leer_pixel((int)r->ebx, (int)r->ecx);
        return;
}

__attribute__((optimize("O0"))) static void sc_24(regs_t *r, int di)
 {
        int fd = (int)r->ebx;
        uint32_t tam = r->ecx;
        r->eax = (uint32_t)-1;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            if (!vfs_actual->truncar(fds()[fd], tam) && !vfs_actual->sincroniza()) {
                if (fds_pos()[fd] > tam) fds_pos()[fd] = tam;
                r->eax = 0;
            }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_25(regs_t *r, int di)
 {
        /* ejecutar hijo preservando al padre: guarda imagen+brk+fds,
           lanza sin reset, restaura y devuelve rc. Sin anidar. */
        const char *pa = (const char *)r->ebx;
        const char *aa = (const char *)r->ecx;
        char ruta[128], args[256];
        uint32_t i = 0, k = 0;
        int rc;
        if (!pa) { r->eax = (uint32_t)-1; return; }
        if (!ptr_ok(di, r->ebx, 128, 0)) return;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        if (aa) {
            if (!ptr_ok(di, r->ecx, 1, 0)) return;
            while (k < 255 && aa[k]) { args[k] = aa[k]; k++; }
        }
        args[k] = 0;
        r->eax = (uint32_t)-1;
        if (di >= 0) proc_actual->modo = 0;   /* espera dentro de la syscall */
        rc = kern_ejecutar(0, ruta, args);
        r = regs_actual(r);
        r->eax = (uint32_t)rc;
        return;
    }


__attribute__((optimize("O0"))) static void sc_26(regs_t *r, int di)
 {
        extern void shell(void);
        shell_anidada++;
        shell();
        shell_anidada--;
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_27(regs_t *r, int di)
 {
        extern int kbd_hay(void);
        r->eax = (uint32_t)kbd_hay();
        return;
    }

    

__attribute__((optimize("O0"))) static void sc_28(regs_t *r, int di)
 {   /* crear(nombre, args) -> pid (>0) o error */
        const char *pa = (const char *)r->ebx;
        const char *aa = (const char *)r->ecx;
        char ruta[128], args[256];
        uint32_t i = 0, k = 0;
        r->eax = (uint32_t)-1;
        if (!pa || !ptr_ok(di, r->ebx, 32, 0)) return;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        if (aa && ptr_ok(di, r->ecx, 1, 0)) {
            while (k < 255 && aa[k]) { args[k] = aa[k]; k++; }
        }
        args[k] = 0;
        r->eax = (uint32_t)kern_crea_proceso(ruta, args, 0);
        return;
    }


__attribute__((optimize("O0"))) static void sc_29(regs_t *r, int di)
{
    /* mi_pid() */
        r->eax = (uint32_t)(proc_actual ? proc_actual->pid : 0);
        return;
}

__attribute__((optimize("O0"))) static void sc_30(regs_t *r, int di)
 {   /* listar(buf, max) -> numero de tareas escritas */
        /* Cada entrada: pid, estado, rc, grafico, marcos, nombre[16] */
        uint32_t max = r->ecx;
        if (max == 0) { r->eax = 0; return; }
        if (max > 1 + MAXPROC) max = 1 + MAXPROC;
        uint32_t n = 0;
        r->eax = 0;
        if (!ptr_ok(di, r->ebx, 36 * max, 1)) return;
        for (int i = 0; i <= MAXPROC && n < max; i++) {
            proc_t *p = &proc_tareas[i];
            if (!p->usado) continue;
            if (pag_escribe_usr((unsigned)di, r->ebx + n * 36, &p->pid, 4)) break;
            if (pag_escribe_usr((unsigned)di, r->ebx + n * 36 + 4, &p->estado, 4))
                break;
            if (pag_escribe_usr((unsigned)di, r->ebx + n * 36 + 8, &p->rc, 4))
                break;
            uint32_t g = (uint32_t)p->grafico, m = (uint32_t)p->nmarcos;
            if (pag_escribe_usr((unsigned)di, r->ebx + n * 36 + 12, &g, 4)) break;
            if (pag_escribe_usr((unsigned)di, r->ebx + n * 36 + 16, &m, 4)) break;
            if (pag_escribe_usr((unsigned)di, r->ebx + n * 36 + 20, p->nombre, 16))
                break;
            n++;
        }
        r->eax = n;
        return;
    }


__attribute__((optimize("O0"))) static void sc_31(regs_t *r, int di)
 {   /* esperar(pid) -> rc del hijo */
        int pid = (int)r->ebx;
        r->eax = (uint32_t)-1;
        if (pid <= 0) return;
        if (proc_actual && !proc_actual->es_usuario) {
            /* desde el nucleo no se bloquea: se consulta el estado */
            proc_t *h = proc_por_pid(pid);
            if (h && h->estado == EST_FIN) r->eax = (uint32_t)h->rc;
            return;
        }
        int _rc = proc_espera_hijo(pid);
        r = regs_actual(r);
        r->eax = (uint32_t)_rc;
        return;
    }


__attribute__((optimize("O0"))) static void sc_32(regs_t *r, int di)
{
    /* ceder() */
        if (proc_actual && proc_actual->es_usuario) {
            proc_actual->modo = 0;
            proc_cede();
            r->eax = 0;
        } else {
            r->eax = 0;
        }
        return;
}

__attribute__((optimize("O0"))) static void sc_33(regs_t *r, int di)
 {   /* info_memoria(buf) -> 0; buf: 5 x uint32 */
        uint32_t v[5];
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->ebx, 20, 1)) return;
        v[0] = pag_marcos_totales();
        v[1] = pag_marcos_libres();
        v[2] = pag_marcos_usados();
        v[3] = pag_ram_total();
        v[4] = pag_e820_valido() ? (uint32_t)pag_e820_entradas() : 0;
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ebx, v, 20)) return;
        } else {
            for (int i = 0; i < 5; i++) ((uint32_t *)r->ebx)[i] = v[i];
        }
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_36(regs_t *r, int di)
 {   /* info_memoria2(buf) -> 0; buf: 8 x uint32 (v2.0).
                    34-35 reservadas para IPC (etapa 1, sin implementar). */
        uint32_t v[8];
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->ebx, 32, 1)) return;
        v[0] = pag_marcos_libres();
        v[1] = pag_marcos_usados();
        v[2] = pag_usados_usuario();
        v[3] = pag_pico_usuario();
        v[4] = pag_doble_free();
        v[5] = proc_fugas() + pag_verifica();
        v[6] = pag_pd_usados();
        v[7] = pag_pt_usadas();
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ebx, v, 32)) return;
        } else {
            for (int i = 0; i < 8; i++) ((uint32_t *)r->ebx)[i] = v[i];
        }
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_37(regs_t *r, int di)
 {   /* proc_stat(pid, buf) -> 0; buf: 5 x uint32 (v2.0):
                    estado, ticks_cpu, cedes, pico_marcos, nmarcos. */
        uint32_t v[5];
        proc_t *q;
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->ecx, 20, 1)) return;
        q = proc_por_pid((int)r->ebx);
        if (!q) return;
        v[0] = (uint32_t)q->estado;
        v[1] = q->ticks_cpu;
        v[2] = q->cedes;
        v[3] = q->pico_marcos;
        v[4] = (uint32_t)q->nmarcos;
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ecx, v, 20)) return;
        } else {
            for (int i = 0; i < 5; i++) ((uint32_t *)r->ecx)[i] = v[i];
        }
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_41(regs_t *r, int di)
 {   /* dir_hijo(dir, n, buf48) -> tipo (1 carp, 2 arch, 3 CD) o -1.
       dir<0 = actual. buf: u32 tipo, char nombre[33], u32 tam, i32 idx
       (idx fs o -1 si es CD). Itera como la shell (k=1..63); en raiz
       sigue el catalogo ISO. */
        extern int cdcat_cuantos(void);
        extern const char *cdcat_nombre(int i);
        extern int cdcat_tam(int i);
        int dir = (int)r->ebx, n = (int)r->ecx;
        uint8_t d[48];
        int cont = 0;
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->edx, 48, 1)) return;
        if (dir < 0) dir = fs_actual();
        if (dir < 0 || dir >= FS_N) return;
        if (vfs_actual->tipo(dir) != 1 && dir != 0) return;
        for (int k = 1; k < FS_N; k++) {
            const char *nm;
            uint32_t t, tm;
            if (!vfs_actual->tipo(k) || vfs_actual->padre(k) != dir)
                continue;
            if (cont != n) { cont++; continue; }
            nm = vfs_actual->nombre(k);
            t = (uint32_t)vfs_actual->tipo(k);
            tm = (uint32_t)vfs_actual->tam(k);
            d[0] = (uint8_t)t; d[1] = (uint8_t)(t >> 8);
            d[2] = (uint8_t)(t >> 16); d[3] = (uint8_t)(t >> 24);
            for (int j = 0; j < 33; j++) d[4 + j] = (uint8_t)nm[j];
            d[37] = d[38] = d[39] = 0;
            d[40] = (uint8_t)tm; d[41] = (uint8_t)(tm >> 8);
            d[42] = (uint8_t)(tm >> 16); d[43] = (uint8_t)(tm >> 24);
            d[44] = (uint8_t)k; d[45] = (uint8_t)(k >> 8);
            d[46] = (uint8_t)(k >> 16); d[47] = (uint8_t)(k >> 24);
            if (di >= 0) {
                if (pag_escribe_usr((unsigned)di, r->edx, d, 48)) return;
            } else {
                for (int j = 0; j < 48; j++)
                    ((uint8_t *)r->edx)[j] = d[j];
            }
            r->eax = t;
            return;
        }
        if (dir == 0) {
            int nc = cdcat_cuantos();
            for (int k = 0; k < nc; k++) {
                const char *nm;
                uint32_t tm;
                if (cont != n) { cont++; continue; }
                nm = cdcat_nombre(k);
                tm = (uint32_t)cdcat_tam(k);
                if (!nm) { cont++; continue; }
                d[0] = 3; d[1] = d[2] = d[3] = 0;
                for (int j = 0; j < 33; j++)
                    d[4 + j] = (j < 16 && nm[j]) ? (uint8_t)nm[j] : 0;
                d[37] = d[38] = d[39] = 0;
                d[40] = (uint8_t)tm; d[41] = (uint8_t)(tm >> 8);
                d[42] = (uint8_t)(tm >> 16); d[43] = (uint8_t)(tm >> 24);
                d[44] = d[45] = d[46] = 0xFF; d[47] = 0xFF;
                if (di >= 0) {
                    if (pag_escribe_usr((unsigned)di, r->edx, d, 48))
                        return;
                } else {
                    for (int j = 0; j < 48; j++)
                        ((uint8_t *)r->edx)[j] = d[j];
                }
                r->eax = 3;
                return;
            }
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_42(regs_t *r, int di)
 {   /* dir_crear(ruta) -> 0 ok, -1 mal/existe. Crea carpeta + sincroniza. */
        const char *pa = (const char *)r->ebx;
        char ruta[128];
        uint32_t i = 0;
        int dir;
        char base[32];
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ebx, 1, 0)) return;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        if (!parte_sys(ruta, &dir, base)) {
            if (vfs_actual->hijo(dir, base) >= 0) return;
            if (vfs_actual->crear(1, dir, base) >= 0 &&
                !vfs_actual->sincroniza())
                r->eax = 0;
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_43(regs_t *r, int di)
 {   /* renombrar(ruta, nuevo) -> 0 ok, -1 mal, -2 choca. Misma carpeta. */
        const char *pa = (const char *)r->ebx;
        const char *na = (const char *)r->ecx;
        char ruta[128], nuevo[33];
        uint32_t i = 0, k = 0;
        int dir, idx, rr;
        char base[32];
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ebx, 1, 0)) return;
        if (!na || !ptr_ok(di, r->ecx, 1, 0)) return;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        while (k < 32 && na[k]) { nuevo[k] = na[k]; k++; }
        nuevo[k] = 0;
        if (!nuevo[0] || na[k]) return;
        if (!parte_sys(ruta, &dir, base)) {
            idx = vfs_actual->hijo(dir, base);
            if (idx <= 0) return;
            rr = vfs_actual->renombrar(idx, nuevo);
            if (!rr && !vfs_actual->sincroniza()) r->eax = 0;
            else r->eax = (uint32_t)rr;
        }
        return;
    }


__attribute__((optimize("O0"))) static void sc_44(regs_t *r, int di)
 {   /* reloj_cmos(buf) -> 0; buf: 6 x u32 [Y,M,D,h,m,s] (BCD del CMOS). */
        uint32_t v[6];
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->ebx, 24, 1)) return;
        for (int i = 0; i < 100000; i++) {
            outb(0x70, 0x8A);
            if (!(inb(0x71) & 0x80)) break;
        }
        {
            /* CMOS: 9=Y, 8=M, 7=D, 4=h, 2=m, 0=s (como cmd_fecha). */
            const uint8_t regs[6] = { 9, 8, 7, 4, 2, 0 };
            for (int j = 0; j < 6; j++) {
                uint8_t x;
                outb(0x70, (uint8_t)(0x80 | regs[j]));
                x = inb(0x71);
                v[j] = (uint32_t)(((x >> 4) * 10) + (x & 15));
            }
        }
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ebx, v, 24)) return;
        } else {
            for (int i = 0; i < 6; i++) ((uint32_t *)r->ebx)[i] = v[i];
        }
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_45(regs_t *r, int di)
 {
        /* video_cmos(op, val): byte de video en CMOS 0x40 (NVRAM).
           op=0 leer -> devuelve el byte; op=1 escribir val (0-2) -> 0.
           Sin esto no hay donde guardar el modo de video para el
           arranque (la ISO es de solo lectura y en frio no hay disco).
           Justificacion v2.1-p1: imprescindible para resolucion
           persistente; ABI 0-44 intacta. */
        uint32_t op = r->ebx;
        (void)di;
        if (op == 0) {
            outb(0x70, (uint8_t)(0x80 | 0x40));   /* F8: NMI inhibida */
            r->eax = (uint32_t)inb(0x71);
            return;
        }
        if (op == 1 && r->ecx <= 3) {
            outb(0x70, (uint8_t)(0x80 | 0x40));   /* F8: NMI inhibida */
            outb(0x71, (uint8_t)r->ecx);
            r->eax = 0;
            return;
        }
        r->eax = (uint32_t)-1;
        return;
    }


__attribute__((optimize("O0"))) static void sc_46(regs_t *r, int di)
 {
        /* teclado_distribucion(op, id): 0=US, 1=ES, 2=CA.
           op=0 leer -> id actual; op=1 establecer (0-2) + persistir en
           cabecera (mismo camino que `teclado` de la shell).
           Justificacion v2.2: la distribucion vive en el driver (Ring0)
           y persiste en disco; Ring3 no puede hacerlo. ABI 0-45 intacta. */
        uint32_t op = r->ebx;
        (void)di;
        if (op == 0) {
            extern int distribucion_actual(void);
            r->eax = (uint32_t)distribucion_actual();
            return;
        }
        if (op == 1 && r->ecx <= 2) {
            extern void distribucion_pon(int d);
            extern int fs_guardar(void);
            distribucion_pon((int)r->ecx);
            fs_guardar();
            r->eax = 0;
            return;
        }
        r->eax = (uint32_t)-1;
        return;
    }


__attribute__((optimize("O0"))) static void sc_47(regs_t *r, int di)
 {
        /* video_modo(w, h): cambia la resolucion en vivo (ver gfx.h).
           0 ok, -1 sin soporte, -2 sin memoria para componer. */
        (void)di;
        r->eax = (uint32_t)pantalla_modo((int)r->ebx, (int)r->ecx);
        return;
    }


__attribute__((optimize("O0"))) static void sc_48(regs_t *r, int di)
 {
        /* video_info(buf) -> 0; buf: 4 x u32 [W, H, paso, prof]. */
        uint32_t v[4];
        int w = 0, h = 0, paso = 0, prof = 0;
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->ebx, 16, 1)) return;
        pantalla_dime(&w, &h, &paso, &prof);
        v[0] = (uint32_t)w;
        v[1] = (uint32_t)h;
        v[2] = (uint32_t)paso;
        v[3] = (uint32_t)prof;
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ebx, v, 16)) return;
        } else {
            for (int i = 0; i < 4; i++) ((uint32_t *)r->ebx)[i] = v[i];
        }
        r->eax = 0;
        return;
    }


__attribute__((optimize("O0"))) static void sc_49(regs_t *r, int di)
 {
        /* v2.2-editor: modificadores de teclado (bit0 Shift, bit1 Ctrl,
           bit2 AltGr, bit3 BloqMay). Solo lectura; ABI 0-48 intacta. */
        extern int teclado_mod(void);
        (void)di;
        r->eax = (uint32_t)teclado_mod();
        return;
}


__attribute__((optimize("O0"))) static void sc_50(regs_t *r, int di)
 {
        /* v2.2-f32: apagar (boton FacilOS). Flush de disco y poweroff
           ACPI de QEMU (0x604/0xB004); sin ACPI queda halt. */
        extern int blk_flush(void);
        (void)di;
        cons_puts("apagando...\n");
        blk_flush();
        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        r->eax = 0;
        for (;;) { cli(); hlt(); }
}


__attribute__((optimize("O0"))) static void sc_51(regs_t *r, int di)
 {
        /* v2.2-f32: reiniciar (boton FacilOS). Flush y pulso al
           controlador de teclado (0x64:FE); alternativa 0x92. */
        extern int blk_flush(void);
        unsigned char b;
        (void)di;
        cons_puts("reiniciando...\n");
        blk_flush();
        outb(0x64, 0xFE);
        b = inb(0x92);
        outb(0x92, (uint8_t)(b | 1));
        r->eax = 0;
        for (;;) { cli(); hlt(); }
}


__attribute__((optimize("O0"))) static void sc_52(regs_t *r, int di)
 {
        /* v2.3-red F1: ficha del adaptador (32 B en ebx). 0 ok,
           -1 sin adaptador. */
        red_if_t f;
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ebx, 32, 1)) return;
        if (red_info(&f)) return;
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ebx, &f, 32))
                return;
        } else {
            uint8_t *d = (uint8_t *)r->ebx;
            uint8_t *s = (uint8_t *)&f;
            for (int i = 0; i < 32; i++) d[i] = s[i];
        }
        r->eax = 0;
        return;
 }


__attribute__((optimize("O0"))) static void sc_53(regs_t *r, int di)
 {
        /* v2.3-red F1: transmite trama Ethernet completa (ebx=tam).
           Devuelve tam o -1 sin adaptador, -2 tam invalido,
           -3 espera agotada. */
        static uint8_t tmp[1518];
        const uint8_t *src = (const uint8_t *)r->ecx;
        uint32_t n = r->ebx;
        int k;
        r->eax = (uint32_t)-1;
        if (n < 14 || n > 1514) { r->eax = (uint32_t)-2; return; }
        if (!ptr_ok(di, r->ecx, n, 0)) return;
        if (di >= 0) {
            /* Copia por partes (evita rachas largas con IF=0). */
            uint32_t h = 0;
            while (h < n) {
                uint32_t t = n - h;
                uint8_t parte[64];
                if (t > 64) t = 64;
                if (pag_lee_usr((unsigned)di, r->ecx + h, parte, t))
                    return;
                for (uint32_t i = 0; i < t; i++) tmp[h + i] = parte[i];
                h += t;
            }
        } else {
            for (uint32_t i = 0; i < n; i++) tmp[i] = src[i];
        }
        k = red_tx(tmp, (int)n);
        r->eax = (uint32_t)k;
        return;
 }


__attribute__((optimize("O0"))) static void sc_54(regs_t *r, int di)
 {
        /* v2.3-red F4: recibe trama (ebx=max, ecx=espera ms, edx=buf).
           Devuelve tam>0, 0 si agota la espera, -1 sin adaptador,
           -2 parametros invalidos. Espera acotada a 10 s.
           Desde F4 bombea primero (el nucleo procesa ARP/ICMP/UDP)
           y sirve del deposito tap: los lectores crudos ya no roban
           tramas a la pila ni entre si. Mismo contrato que F1. */
        static uint8_t tmp[1518];
        uint32_t max = r->ebx, esp = r->ecx;
        uint32_t fin;
        int k;
        r->eax = (uint32_t)-1;
        if ((int32_t)max <= 0 || max > 1518) {
            r->eax = (uint32_t)-2;
            return;
        }
        if (!ptr_ok(di, r->edx, max, 1)) return;
        if (esp > 10000) esp = 10000;
        if (!red_hay()) { r->eax = (uint32_t)-1; return; }
        fin = irq_ticks() + esp / 10u + 1u;
        for (;;) {
            red_bombea();
            k = red_saca_crudo(tmp, 1518);
            if (k < 0) { r->eax = (uint32_t)k; return; }
            if (k > 0) {
                if ((uint32_t)k > max) k = (int)max;
                if (di >= 0) {
                    if (pag_escribe_usr((unsigned)di, r->edx, tmp,
                                        (uint32_t)k))
                        return;
                } else {
                    uint8_t *d = (uint8_t *)r->edx;
                    for (int i = 0; i < k; i++) d[i] = tmp[i];
                }
                r->eax = (uint32_t)k;
                return;
            }
            if ((int32_t)(irq_ticks() - fin) >= 0) {
                r->eax = 0;
                return;
            }
            if (di < 0) { r->eax = 0; return; }
            /* Espera por rodajas (~20 ms): bloquea el proceso, no
               la CPU (hlt colgaria: IF=0 en syscall). */
            proc_actual->modo = 0;
            proc_bloquea_ticks(irq_ticks() + 2u);
            r = regs_actual(r);
        }
 }


__attribute__((optimize("O0"))) static void sc_55(regs_t *r, int di)
 {
        /* v2.3-red F2: configuracion L3 (sin adaptador tambien vale).
           op 0=leer 16B {ip,mask,gw,dns}; 1=ip 2=mask 3=gw 4=dns.
           Fijar IP no nula emite anuncio gratuito. */
        int op = (int)r->ebx;
        r->eax = (uint32_t)-2;
        if (op == 0) {
            red_cfg_t c;
            if (!ptr_ok(di, r->ecx, 16, 1)) return;
            red_cfg_leer(&c);
            if (di >= 0) {
                if (pag_escribe_usr((unsigned)di, r->ecx, &c, 16))
                    return;
            } else {
                uint8_t *d = (uint8_t *)r->ecx;
                uint8_t *s = (uint8_t *)&c;
                for (int i = 0; i < 16; i++) d[i] = s[i];
            }
            r->eax = 0;
            return;
        }
        if (op >= 1 && op <= 4) {
            r->eax = (uint32_t)red_cfg_fijar(op, r->ecx);
            return;
        }
        return;
 }


__attribute__((optimize("O0"))) static void sc_56(regs_t *r, int di)
 {
        /* v2.3-red F2: consulta de red.
           tipo 0 (ARP): ptr12B {ip, mac[6], espera_ms}; 0 ok con MAC,
           1 espera agotada, -1 sin adaptador, -2 args.
           tipo 2 (sonda IP de diagnostico): ptr8B {ip,ttl,proto,
           espera}; 0 enviada, resto error de red_ip_enviar.
           tipo 1 (DNS) en F4: -2 no implementado. */
        int tipo = (int)r->ebx;
        r->eax = (uint32_t)-2;
        if (tipo == 0) {
            red_consulta_t q;
            int k;
            if (!ptr_ok(di, r->ecx, 12, 1)) return;
            if (di >= 0) {
                if (pag_lee_usr((unsigned)di, r->ecx, &q, 12)) return;
            } else {
                uint8_t *s = (uint8_t *)r->ecx;
                uint8_t *d = (uint8_t *)&q;
                for (int i = 0; i < 12; i++) d[i] = s[i];
            }
            if (!q.ip) return;
            if (q.espera > 10000) q.espera = 10000;
            k = red_arp_resolver(q.ip, (int)q.espera, q.mac);
            if (k < 0) { r->eax = (uint32_t)k; return; }
            r = regs_actual(r);
            if (di >= 0) {
                if (pag_escribe_usr((unsigned)di, r->ecx + 4, q.mac, 6))
                    return;
            } else {
                uint8_t *d = (uint8_t *)r->ecx + 4;
                for (int i = 0; i < 6; i++) d[i] = q.mac[i];
            }
            r->eax = (uint32_t)k;
            return;
        }
        if (tipo == 2) {
            red_sonda_t s;
            static const uint8_t carga[8] = {
                'F', '3', '2', 'R', 'E', 'D', 'v', '1'
            };
            int k;
            if (!ptr_ok(di, r->ecx, 8, 0)) return;
            if (di >= 0) {
                if (pag_lee_usr((unsigned)di, r->ecx, &s, 8)) return;
            } else {
                uint8_t *p = (uint8_t *)r->ecx;
                uint8_t *d = (uint8_t *)&s;
                for (int i = 0; i < 8; i++) d[i] = p[i];
            }
            if (!s.ip || !s.proto) return;
            k = red_ip_enviar(s.ip, s.proto, carga, 8,
                              s.ttl ? s.ttl : 64);
            r = regs_actual(r);
            r->eax = (uint32_t)k;
            return;
        }
        return;
 }


__attribute__((optimize("O0"))) static void sc_57(regs_t *r, int di)
 {
        /* v2.3-red F3: eco ICMP (comprovar_conexion). Entrada 12 B
           {ip,n,espera,ttl}: N ecos (1..10) con espera acotada cada
           uno. Sale 6 B por eco {seq,rtt_ms,ok,ttl}. Devuelve bytes
           o -1 sin adaptador, -2 args, -3 sin IP; -4 ARP agotado y
           -5 fallo Tx se propagan del envio (la terminal los muestra
           como fallo de comprobacion, no como falta de adaptador). */
        struct {
            uint32_t ip;
            uint16_t n;
            uint16_t espera;
            uint8_t ttl;
            uint8_t res;
            uint16_t res2;
        } pe;
        uint16_t ident;
        int i, n;
        r->eax = (uint32_t)-2;
        if (!ptr_ok(di, r->ebx, 12, 0)) return;
        if (di >= 0) {
            if (pag_lee_usr((unsigned)di, r->ebx, (void *)&pe, 12))
                return;
        } else {
            uint8_t *s = (uint8_t *)r->ebx;
            uint8_t *d = (uint8_t *)&pe;
            for (int k = 0; k < 12; k++) d[k] = s[k];
        }
        if (!pe.ip || !pe.n || pe.n > 10) return;
        if (pe.espera > 10000) pe.espera = 10000;
        if (pe.espera < 100) pe.espera = 100;
        if (!red_hay()) { r->eax = (uint32_t)-1; return; }
        if (!red_cfg_ip()) { r->eax = (uint32_t)-3; return; }
        if (!ptr_ok(di, r->ecx, pe.n * 6u, 1)) return;
        ident = (uint16_t)((uint32_t)proc_actual->pid * 7919u +
                          irq_ticks());
        n = 0;
        for (i = 1; i <= (int)pe.n; i++) {
            uint8_t sal[6];
            uint16_t rtt = 0;
            uint8_t ttl = 0;
            int k = red_icmp_ping(pe.ip, ident, (uint16_t)i,
                                  pe.ttl ? pe.ttl : 64,
                                  pe.espera, &rtt, &ttl);
            r = regs_actual(r);
            if (k < 0) { r->eax = (uint32_t)k; return; }
            sal[0] = (uint8_t)((i >> 8) & 0xFFu);
            sal[1] = (uint8_t)(i & 0xFFu);
            sal[2] = (uint8_t)((rtt >> 8) & 0xFFu);
            sal[3] = (uint8_t)(rtt & 0xFFu);
            sal[4] = (uint8_t)(k ? 1 : 0);
            sal[5] = ttl;
            if (di >= 0) {
                if (pag_escribe_usr((unsigned)di,
                                    r->ecx + (uint32_t)n, sal, 6))
                    return;
            } else {
                uint8_t *d = (uint8_t *)r->ecx + n;
                for (int j = 0; j < 6; j++) d[j] = sal[j];
            }
            n += 6;
        }
        r->eax = (uint32_t)n;
        return;
 }


__attribute__((optimize("O0"))) static void sc_58(regs_t *r, int di)
 {
        /* v2.3-red F4: liga puerto UDP (ebx=puerto, 0=efimero).
           Devuelve el puerto o negativo (-1 sin adaptador, -2 args,
           -6 en uso por otro proceso o tabla llena con pendientes).
           Idempotente para el propio proceso. */
        uint32_t p = r->ebx;
        int k;
        r->eax = (uint32_t)-2;
        (void)di;
        if (p > 65535) return;
        if (!red_hay()) { r->eax = (uint32_t)-1; return; }
        if (!proc_actual) return;
        k = red_udp_ligar((uint16_t)p, (uint32_t)proc_actual->pid);
        r->eax = (uint32_t)k;
        return;
 }


__attribute__((optimize("O0"))) static void sc_59(regs_t *r, int di)
 {
        /* v2.3-red F4: envia datagrama UDP. Entrada 8 B {ip,dpuerto,
           spuerto} en ebx, datos en ecx, len en edx (0..1472).
           Devuelve len o negativo (-1 sin adaptador, -2 args, -3 sin
           ruta, -4 ARP agotado, -5 fallo Tx). sport 0 = sin ligar. */
        struct {
            uint32_t ip;
            uint16_t dport;
            uint16_t sport;
        } e;
        static uint8_t tmp[UDP_TX_MAX];
        uint32_t len = r->edx;
        int k;
        r->eax = (uint32_t)-2;
        if (!ptr_ok(di, r->ebx, 8, 0)) return;
        if (di >= 0) {
            if (pag_lee_usr((unsigned)di, r->ebx, (void *)&e, 8))
                return;
        } else {
            uint8_t *s = (uint8_t *)r->ebx;
            uint8_t *d = (uint8_t *)&e;
            for (int i = 0; i < 8; i++) d[i] = s[i];
        }
        if (!e.ip || !e.dport || len > UDP_TX_MAX) return;
        if (len && !ptr_ok(di, r->ecx, len, 0)) return;
        if (!red_hay()) { r->eax = (uint32_t)-1; return; }
        if (len) {
            if (di >= 0) {
                uint32_t h = 0;
                while (h < len) {
                    uint32_t t = len - h;
                    uint8_t parte[64];
                    if (t > 64) t = 64;
                    if (pag_lee_usr((unsigned)di, r->ecx + h, parte, t))
                        return;
                    for (uint32_t i = 0; i < t; i++)
                        tmp[h + i] = parte[i];
                    h += t;
                }
            } else {
                uint8_t *s = (uint8_t *)r->ecx;
                for (uint32_t i = 0; i < len; i++) tmp[i] = s[i];
            }
        }
        k = red_udp_enviar(e.ip, e.dport, e.sport, tmp, (int)len);
        r->eax = (uint32_t)k;
        return;
 }


__attribute__((optimize("O0"))) static void sc_60(regs_t *r, int di)
 {
        /* v2.3-red F4: recibe datagrama UDP. Entrada 4 B
           {puerto,espera} en ebx; salida en ecx {src:4,sport:2,
           len:2}+datos, max en edx (>=8). Devuelve bytes (>0),
           0 si agota la espera, -1 sin adaptador, -2 args o puerto
           sin ligar, -6 liga de otro proceso. Espera acotada. */
        struct {
            uint16_t puerto;
            uint16_t espera;
        } pe;
        static uint8_t tmp[8 + UDP_COLA_DATOS];
        uint32_t max = r->edx, fin;
        uint32_t pid;
        int k;
        r->eax = (uint32_t)-2;
        if (!ptr_ok(di, r->ebx, 4, 0)) return;
        if (di >= 0) {
            if (pag_lee_usr((unsigned)di, r->ebx, (void *)&pe, 4))
                return;
        } else {
            uint8_t *s = (uint8_t *)r->ebx;
            uint8_t *d = (uint8_t *)&pe;
            for (int i = 0; i < 4; i++) d[i] = s[i];
        }
        if (!pe.puerto || max < 8) return;
        if (!ptr_ok(di, r->ecx, max, 1)) return;
        if (pe.espera > 10000) pe.espera = 10000;
        if (!red_hay()) { r->eax = (uint32_t)-1; return; }
        if (!proc_actual) return;
        pid = (uint32_t)proc_actual->pid;
        fin = irq_ticks() + pe.espera / 10u + 1u;
        for (;;) {
            int copia;
            red_bombea();
            k = red_udp_sacar(pe.puerto, pid, 0, 0, tmp,
                              (int)sizeof(tmp));
            if (k < 0) { r->eax = (uint32_t)k; return; }
            if (k > 0) {
                copia = k > (int)max ? (int)max : k;
                r = regs_actual(r);
                if (di >= 0) {
                    if (pag_escribe_usr((unsigned)di, r->ecx, tmp,
                                        (uint32_t)copia))
                        return;
                } else {
                    uint8_t *d = (uint8_t *)r->ecx;
                    for (int i = 0; i < copia; i++) d[i] = tmp[i];
                }
                r->eax = (uint32_t)copia;
                return;
            }
            if ((int32_t)(irq_ticks() - fin) >= 0) {
                r->eax = 0;
                return;
            }
            proc_actual->modo = 0;
            proc_bloquea_ticks(irq_ticks() + 2u);
            r = regs_actual(r);
        }
 }


__attribute__((optimize("O0"))) static void sc_61(regs_t *r, int di)
 {
        /* v2.3-red F5: TCP (op en ebx, multiplexada como 55/56).
           0 abrir {ip:4,puerto:2,local:2} -> id | neg (acotado).
           1 escuchar {puerto:2} -> id | neg.
           2 aceptar {id:4,espera:2} -> hijo, 0 espera, neg.
           3 enviar {id:4}+buf+len -> len | neg (<=536).
           4 recibir {id:4,espera:2}+buf+max -> B, 0 espera,
              -12 fin remoto, neg.
           5 cerrar {id:4} -> 0 | neg (ordendo, acotado).
           6 abortar {id:4} -> 0 (RST + libera).
           7 estado {id:4} -> estado 1..6 | neg.
           Negativos: -1 sin adaptador, -2 args, -3 sin ruta,
           -4 ARP, -5 Tx, -6 en uso/tabla, -7 conexion, -8 estado,
           -9 abortada. Toda espera acotada. */
        int op = (int)r->ebx;
        uint32_t pid;
        static uint8_t tmp[64];
        r->eax = (uint32_t)-2;
        if (op < 0 || op > 7) return;
        if (!red_hay()) { r->eax = (uint32_t)-1; return; }
        if (!proc_actual) return;
        pid = (uint32_t)proc_actual->pid;
        if (op == 0) {
            struct {
                uint32_t ip;
                uint16_t puerto;
                uint16_t local;
            } a;
            int k;
            if (!ptr_ok(di, r->ecx, 8, 0)) return;
            if (di >= 0) {
                if (pag_lee_usr((unsigned)di, r->ecx, (void *)&a, 8))
                    return;
            } else {
                uint8_t *s = (uint8_t *)r->ecx;
                uint8_t *d = (uint8_t *)&a;
                for (int i = 0; i < 8; i++) d[i] = s[i];
            }
            k = red_tcp_abrir(a.ip, a.puerto, a.local, pid);
            r = regs_actual(r);
            r->eax = (uint32_t)k;
            return;
        }
        if (op == 1) {
            uint8_t b[2];
            int k;
            if (!ptr_ok(di, r->ecx, 2, 0)) return;
            if (di >= 0) {
                if (pag_lee_usr((unsigned)di, r->ecx, b, 2)) return;
            } else {
                uint8_t *s = (uint8_t *)r->ecx;
                b[0] = s[0];
                b[1] = s[1];
            }
            k = red_tcp_escuchar((uint16_t)(b[0] | ((uint16_t)b[1] << 8)),
                                 pid);
            r = regs_actual(r);
            r->eax = (uint32_t)k;
            return;
        }
        if (op == 2 || op == 4 || op == 5 || op == 6 || op == 7) {
            struct {
                uint32_t id;
                uint16_t espera;
                uint16_t res;
            } a;
            int k;
            if (!ptr_ok(di, r->ecx, 8, 0)) return;
            if (di >= 0) {
                if (pag_lee_usr((unsigned)di, r->ecx, (void *)&a, 8))
                    return;
            } else {
                uint8_t *s = (uint8_t *)r->ecx;
                uint8_t *d = (uint8_t *)&a;
                for (int i = 0; i < 8; i++) d[i] = s[i];
            }
            if (op == 2) {
                k = red_tcp_aceptar((int)a.id, pid, a.espera);
            } else if (op == 5) {
                k = red_tcp_cerrar((int)a.id, pid);
            } else if (op == 6) {
                k = red_tcp_abortar((int)a.id, pid);
            } else if (op == 7) {
                k = red_tcp_estado((int)a.id, pid);
            } else {
                uint32_t max = r->esi;
                uint32_t n = 0;
                if (max < 1 || max > 2048) return;
                if (!ptr_ok(di, r->edx, max, 1)) return;
                if (a.espera > 10000) a.espera = 10000;
                while (n < max) {
                    uint32_t t = max - n;
                    if (t > 64) t = 64;
                    k = red_tcp_recibir((int)a.id, pid, tmp, t,
                                        n ? 0 : a.espera);
                    r = regs_actual(r);
                    if (k < 0) {
                        r->eax = (uint32_t)(n ? (int)n : k);
                        return;
                    }
                    if (k == 0) break;
                    if (di >= 0) {
                        if (pag_escribe_usr((unsigned)di,
                                            r->edx + n, tmp,
                                            (uint32_t)k))
                            return;
                    } else {
                        uint8_t *d = (uint8_t *)r->edx + n;
                        for (int i = 0; i < k; i++) d[i] = tmp[i];
                    }
                    n += (uint32_t)k;
                    if (k < (int)t) break;
                    r = regs_actual(r);
                }
                r->eax = n;
                return;
            }
            r = regs_actual(r);
            r->eax = (uint32_t)k;
            return;
        }
        /* op 3: enviar (id en ecx[0..3], datos en edx, len en esi). */
        {
            uint8_t b[4];
            uint32_t len = r->esi, h = 0;
            int k;
            if (!ptr_ok(di, r->ecx, 4, 0)) return;
            if (di >= 0) {
                if (pag_lee_usr((unsigned)di, r->ecx, b, 4)) return;
            } else {
                uint8_t *s = (uint8_t *)r->ecx;
                for (int i = 0; i < 4; i++) b[i] = s[i];
            }
            if (!len || len > TCP_MSS) return;
            if (!ptr_ok(di, r->edx, len, 0)) return;
            k = (int)(((uint32_t)b[0]) | ((uint32_t)b[1] << 8) |
                      ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
            /* Envio en un tramo (<=536): via nucleo con espera. */
            {
                static uint8_t sbuf[TCP_MSS];
                if (di >= 0) {
                    if (pag_lee_usr((unsigned)di, r->edx, sbuf, len))
                        return;
                } else {
                    uint8_t *s = (uint8_t *)r->edx;
                    for (uint32_t i = 0; i < len; i++)
                        sbuf[i] = s[i];
                }
                k = red_tcp_enviar(k, pid, sbuf, (int)len);
                r = regs_actual(r);
                r->eax = (uint32_t)k;
                return;
            }
        }
 }


__attribute__((optimize("O0"))) static void sc_63(regs_t *r, int di)
 {
        /* v2.3-red F2: tabla de rutas (conectada + defecto derivadas
           de la configuracion). op 0: vuelca hasta max bytes (12 por
           ruta); devuelve bytes o -2. op 1+ en F6. */
        int op = (int)r->ebx;
        uint32_t max = r->edx;
        r->eax = (uint32_t)-2;
        if (op != 0) return;
        if (max > 96) max = 96;
        if (!max) { r->eax = 0; return; }
        if (!ptr_ok(di, r->ecx, max, 1)) return;
        {
            static uint8_t rutas[96];
            int n = red_rutas(rutas, (int)max);
            if (n < 0) { r->eax = (uint32_t)n; return; }
            if (di >= 0) {
                if (pag_escribe_usr((unsigned)di, r->ecx, rutas,
                                    (uint32_t)n))
                    return;
            } else {
                uint8_t *d = (uint8_t *)r->ecx;
                for (int i = 0; i < n; i++) d[i] = rutas[i];
            }
            r = regs_actual(r);
            r->eax = (uint32_t)n;
        }
        return;
 }


typedef void (*sc_fn_t)(regs_t *r, int di);
#define SC_N 64
static sc_fn_t const sc_tabla[SC_N] = {
    [0] = sc_00,    [1] = sc_01,
    [2] = sc_02,
    [3] = sc_03,
    [4] = sc_04,
    [5] = sc_05,
    [6] = sc_06,
    [7] = sc_07,
    [8] = sc_08,
    [9] = sc_09,
    [10] = sc_10,
    [11] = sc_11,
    [12] = sc_12,
    [13] = sc_13,
    [14] = sc_14,
    [15] = sc_15,
    [16] = sc_16,
    [17] = sc_17,
    [18] = sc_18,
    [19] = sc_19,
    [20] = sc_20,
    [21] = sc_21,
    [22] = sc_22,
    [23] = sc_23,
    [24] = sc_24,
    [25] = sc_25,
    [26] = sc_26,
    [27] = sc_27,
    [28] = sc_28,
    [29] = sc_29,
    [30] = sc_30,
    [31] = sc_31,
    [32] = sc_32,
    [33] = sc_33,
    [34] = sc_ipc_enviar,
    [35] = sc_ipc_recibir,
    [36] = sc_36,
    [37] = sc_37,
    [38] = sc_raton,
    [39] = sc_raton_hay,
    [40] = sc_ipc_hay,
    [41] = sc_41,
    [42] = sc_42,
    [43] = sc_43,
    [44] = sc_44,
    [45] = sc_45,
    [46] = sc_46,
    [47] = sc_47,
    [48] = sc_48,
    [49] = sc_49,
    [50] = sc_50,
    [51] = sc_51,
    [52] = sc_52,
    [53] = sc_53,
    [54] = sc_54,
    [55] = sc_55,
    [56] = sc_56,
    [57] = sc_57,
    /* 58-60 F4: UDP (ligar/enviar/recibir) */
    [58] = sc_58,
    [59] = sc_59,
    [60] = sc_60,
    [61] = sc_61,   /* F5: TCP multiplexado (op 0..7) */
    /* 62 reservada */
    [63] = sc_63,
};
static const char *const sc_nombre[SC_N] = {
    [0] = "salir",
    [1] = "imprimir",
    [2] = "ticks",
    [3] = "abrir",
    [4] = "leer",
    [5] = "escribir",
    [6] = "cerrar",
    [7] = "memoria",
    [8] = "leer_tecla",
    [9] = "crear",
    [10] = "pixel",
    [11] = "linea",
    [12] = "rectangulo",
    [13] = "texto",
    [14] = "limpiar_pantalla",
    [15] = "buscar",
    [16] = "borrar",
    [17] = "dormir",
    [18] = "destino",
    [19] = "presentar",
    [20] = "sprite",
    [21] = "sprite_t",
    [22] = "blit",
    [23] = "leer_pixel",
    [24] = "truncar",
    [25] = "ejecutar",
    [26] = "terminal",
    [27] = "hay_tecla",
    [28] = "crear_proceso",
    [29] = "mi_pid",
    [30] = "listar",
    [31] = "esperar",
    [32] = "ceder",
    [33] = "info_memoria",
    [34] = "ipc_enviar",
    [35] = "ipc_recibir",
    [36] = "info_memoria2",
    [37] = "proc_stat",
    [38] = "leer_raton",
    [39] = "hay_raton",
    [40] = "ipc_hay",
    [41] = "dir_hijo",
    [42] = "dir_crear",
    [43] = "renombrar",
    [44] = "reloj",
    [45] = "video_cmos",
    [46] = "teclado_distribucion",
    [47] = "video_modo",
    [48] = "video_info",
    [49] = "teclado_mod",
    [50] = "apagar",
    [51] = "reiniciar",
    [52] = "red_interfaz",
    [53] = "red_trama_enviar",
    [54] = "red_trama_recibir",
    [55] = "red_configurar",
    [56] = "red_consultar",
    [57] = "red_comprobar",
    [58] = "red_udp_ligar",
    [59] = "red_udp_enviar",
    [60] = "red_udp_recibir",
    [61] = "red_tcp",
    [63] = "red_rutas",
};
static uint32_t sc_count[SC_N];

__attribute__((optimize("O0"))) void syscall_c(regs_t *r) {
    syscall_regs_ptr = r;
    /* El marco se guarda por proceso: mientras este cede la CPU, otro
       proceso puede entrar en syscall_c y cambiar el global. Al reanudar
       hay que usar el propio (ver regs_actual). */
    if (proc_actual) proc_actual->sc_regs = r;
    int di = di_actual();
    uint32_t n = r->eax;
    if (n >= SC_N || !sc_tabla[n]) { r->eax = (uint32_t)-1; return; }
    sc_count[n]++;
    sc_tabla[n](r, di);
}

void syscall_init(void) {
}
