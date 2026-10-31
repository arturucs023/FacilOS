#include "exec.h"
#include "libc.h"
#include "cpu.h"
#include "irq.h"
#include "gfx.h"
#include "fs.h"
#include "teclado.h"
#include "seg.h"
#include "proc.h"
#include "pag.h"

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
static void fds_init(void) {
    for (int i = 0; i < NFD; i++) {
        proc_actual->fds[i] = -1;
        proc_actual->fds_pos[i] = 0;
    }
    proc_actual->brk = UBRK_INI;
}

/* ruta (abs o relativa a exec_base_dir) -> dir + nombre base. 0 ok. */
static int parte_sys(const char *ruta, int *dir, char *base) {
    char tmp[128];
    uint32_t i = 0;
    while (i < 127 && ruta[i]) { tmp[i] = ruta[i]; i++; }
    tmp[i] = 0;
    if (!tmp[0]) return -1;
    if (tmp[0] == '/') {
        uint32_t k = 0, j = 0;
        while (tmp[k] == '/') k++;
        while (tmp[k] && tmp[k] != '/' && j < 31) {
            base[j++] = tmp[k++];
        }
        base[j] = 0;
        if (!base[0] || tmp[k]) return -1;
        char nd[128];
        uint32_t m = 0;
        while (tmp[m]) m++;
        uint32_t f = m;
        while (f > 0 && tmp[f - 1] != '/') f--;
        if (f <= 1) { nd[0] = '/'; nd[1] = 0; }
        else {
            uint32_t l = 0;
            for (l = 0; l < f - 1 && l < 127; l++) nd[l] = tmp[l];
            nd[l] = 0;
        }
        *dir = fs_resuelve(nd);
        return *dir < 0 ? -1 : 0;
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
    int idx = fs_hijo(exec_base_dir, base);
    if (idx >= 0 && fs_tipo(idx) == 2) {
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
    int r = fs_leer_parcial(o->idx, off, buf, n);
    return r <= 0 ? r : (int)n;
}

static uint32_t origen_tam(origen_t *o) {
    if (o->es_cd) {
        extern int cdcat_tam(int i);
        return (uint32_t)cdcat_tam(o->idx);
    }
    return (uint32_t)fs_tam(o->idx);
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
    /* argumentos del programa en la cima de la pila */
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
    int pid = proc_crea(base, di, 0x200000 + ent, 0x2F0000);
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
       poder devolverlos al terminar el proceso */
    anota_rango(p, di, 0x200000, plano + bss);
    anota_rango(p, di, 0x2E0000, PILA_USR_PAGS * PAG_MARCO);
    /* regiones virtuales del proceso (v2.0) */
    proc_vma_add(p, 0x200000, plano + bss, VMAR_CODIGO);
    proc_vma_add(p, 0x2E0000, PILA_USR_PAGS * PAG_MARCO, VMAR_PILA);
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
   dorso: si otro proceso esta dibujando, se le cede (politica de v1.9). */
static int gfx_toma(int di) {
    if (di < 0) return 1;
    int pid = proc_actual->pid;
    if (proc_actual->grafico == 0) proc_actual->grafico = 1;
    if (gfx_pid != pid) {
        if (gfx_pid >= 0) {
            extern void g_dorso_limpia(void);
            g_dorso_limpia();
        }
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

__attribute__((optimize("O0"))) void syscall_c(regs_t *r) {
    syscall_regs_ptr = r;
    /* El marco se guarda por proceso: mientras este cede la CPU, otro
       proceso puede entrar en syscall_c y cambiar el global. Al reanudar
       hay que usar el propio (ver regs_actual). */
    if (proc_actual) proc_actual->sc_regs = r;
    int di = di_actual();
    switch (r->eax) {
    case 0:
        /* salir: termina el proceso y el planificador sigue con otro. */
        proc_muere((int)r->ebx);
        break;
    case 1: {
        uint32_t len = r->ecx;
        if (len > 2048) len = 2048;
        if (!ptr_ok(di, r->ebx, len, 0)) { r->eax = (uint32_t)-1; break; }
        const char *p = (const char *)r->ebx;
        for (uint32_t i = 0; i < len && p[i]; i++) cons_putc(p[i]);
        r->eax = 0;
        break;
    }
    case 2:
        r->eax = irq_ticks();
        break;
    case 3: {
        fds_init();
        const char *pa = (const char *)r->ebx;
        char ruta[128];
        uint32_t i = 0;
        if (!ptr_ok(di, r->ebx, 1, 0)) { r->eax = (uint32_t)-1; break; }
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        int dir;
        char base[32];
        r->eax = (uint32_t)-1;
        if (!parte_sys(ruta, &dir, base)) {
            int idx = fs_hijo(dir, base);
            if (idx >= 0 && fs_tipo(idx) == 2) {
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
        break;
    }
    case 4: {
        /* v1.6: lectura por offset con trozos de 512 (ficheros grandes). */
        int fd = (int)r->ebx;
        uint8_t *dst = (uint8_t *)r->ecx;
        uint32_t n = r->edx;
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ecx, n ? n : 1, 1)) break;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            if (!n) { r->eax = 0; break; }
            static uint8_t tmp[512];
            uint16_t t = fs_tam(fds()[fd]);
            uint32_t p = fds_pos()[fd];
            if (p >= t) { r->eax = 0; break; }
            if (n > (uint32_t)(t - p)) n = t - p;
            uint32_t hechas = 0;
            while (hechas < n) {
                uint32_t trozo = n - hechas;
                if (trozo > 512) trozo = 512;
                int le = fs_leer_parcial(fds()[fd], p + hechas, tmp, trozo);
                if (le <= 0) { if (!hechas) r->eax = (uint32_t)le; break; }
                for (int i = 0; i < le; i++) dst[hechas + i] = tmp[i];
                hechas += (uint32_t)le;
                if ((uint32_t)le < trozo) break;
            }
            if (hechas) { fds_pos()[fd] = p + hechas; r->eax = hechas; }
        }
        break;
    }
    case 5: {
        /* v1.6: escribe en la posicion actual (offset), extiende.
           En pos 0 con fichero viejo NO trunca (usar 24). */
        int fd = (int)r->ebx;
        const uint8_t *src = (const uint8_t *)r->ecx;
        uint32_t n = r->edx;
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ecx, n ? n : 1, 0)) break;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            static uint8_t tmp[512];
            uint32_t p = fds_pos()[fd];
            uint32_t hechas = 0;
            int mal = 0;
            while (hechas < n) {
                uint32_t trozo = n - hechas;
                if (trozo > 512) trozo = 512;
                for (uint32_t i = 0; i < trozo; i++) tmp[i] = src[hechas + i];
                if (fs_escribir_parcial(fds()[fd], p + hechas, tmp, trozo)) {
                    mal = 1;
                    break;
                }
                hechas += trozo;
            }
            if (!mal && !fs_guardar()) {
                fds_pos()[fd] = p + hechas;
                r->eax = hechas;
            }
        }
        break;
    }
    case 6: {
        int fd = (int)r->ebx;
        r->eax = (uint32_t)-1;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            fds()[fd] = -1;
            r->eax = 0;
        }
        break;
    }
    case 7: {   /* memoria(n): sbrk con paginas del proceso */
        uint32_t n = r->ebx;
        uint32_t *b = user_brk();
        r->eax = (uint32_t)-1;
        if (di < 0) break;                    /* la shell usa kmalloc */
        if (n > UHEAP_FIN - *b) break;
        uint32_t viejo = *b;
        uint32_t nuevo = *b + n;
        /* mapea las paginas nuevas que todavia no existan */
        for (uint32_t v = viejo & ~(PAG_MARCO - 1); v < nuevo; v += PAG_MARCO) {
            if (pag_presente((unsigned)di, v)) continue;
            if (pag_mapa_nuevas((unsigned)di, v, PAG_MARCO,
                                PAG_PRESENTE | PAG_ESCRITURA | PAG_USUARIO))
                break;
            uint32_t f;
            if (!pag_translada((unsigned)di, v, &f))
                proc_anota_marco(proc_actual, f & ~(PAG_MARCO - 1));
        }
        if (!pag_valida_usuario((unsigned)di, nuevo, 1, 1)) break;
        *b = nuevo;
        proc_vma_add(proc_actual, viejo & ~(PAG_MARCO - 1),
                     nuevo - (viejo & ~(PAG_MARCO - 1)), VMAR_HEAP);
        r->eax = viejo;
        break;
    }
    case 8: {   /* leer_tecla(): bloquea el proceso, no la CPU entera */
        extern int kbd_tecla(void);
        if (di < 0) {
            r->eax = (uint32_t)kbd_tecla();
            break;
        }
        if (!kbd_hay()) {
            proc_actual->modo = 0;      /* reanuda en anillo 0 */
            proc_bloquea_tecla();
            r = regs_actual(r);
        }
        r->eax = (uint32_t)kbd_tecla();
        break;
    }
    case 9: {
        fds_init();
        const char *pa = (const char *)r->ebx;
        char ruta[128];
        uint32_t i = 0;
        int dir;
        char base[32];
        r->eax = (uint32_t)-1;
        if (!ptr_ok(di, r->ebx, 1, 0)) break;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        if (!parte_sys(ruta, &dir, base)) {
            int idx = fs_hijo(dir, base);
            if (idx < 0) idx = fs_crear(2, dir, base);
            if (idx >= 0) {
                for (int f = 0; f < NFD; f++) {
                    if (fds()[f] < 0) {
                        fds()[f] = idx;
                        fds_pos()[f] = 0;
                        if (fs_guardar()) { fds()[f] = -1; break; }
                        r->eax = (uint32_t)f;
                        break;
                    }
                }
            }
        }
        break;
    }
    case 10:
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; break; }
        g_pixel((int)r->ebx, (int)r->ecx, (uint8_t)r->edx);
        r->eax = 0;
        break;
    case 11:
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; break; }
        g_linea((int)r->ebx, (int)r->ecx, (int)r->edx, (int)r->esi,
                (uint8_t)r->edi);
        r->eax = 0;
        break;
    case 12:
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; break; }
        g_rect((int)r->ebx, (int)r->ecx, (int)r->edx, (int)r->esi,
               (uint8_t)r->edi);
        r->eax = 0;
        break;
    case 13: {
        const char *p = (const char *)r->edx;
        uint32_t len = r->esi;
        if (len > 256) len = 256;
        if (!ptr_ok(di, r->edx, len ? len : 1, 0)) { r->eax = (uint32_t)-1; break; }
        gfx_toma(di);
        char tmp[257];
        uint32_t i = 0;
        while (i < len && p[i]) { tmp[i] = p[i]; i++; }
        tmp[i] = 0;
        g_texto((int)r->ebx, (int)r->ecx, tmp, (uint8_t)r->edi);
        r->eax = 0;
        break;
    }
    case 14:
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; break; }
        g_limpiar((uint8_t)r->ebx);
        r->eax = 0;
        break;
    case 15: {
        /* buscar: fd, off (con signo), base 0=ini 1=act 2=fin -> pos. */
        int fd = (int)r->ebx;
        int32_t d = (int32_t)r->ecx;
        uint32_t base = r->edx;
        r->eax = (uint32_t)-1;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0 && base <= 2) {
            int32_t t = (int32_t)fs_tam(fds()[fd]);
            int32_t p = (int32_t)fds_pos()[fd];
            int32_t np = base == 0 ? d : (base == 1 ? p + d : t + d);
            if (np >= 0 && np <= t) {
                fds_pos()[fd] = (uint32_t)np;
                r->eax = (uint32_t)np;
            }
        }
        break;
    }
    case 16: {
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
            int idx = fs_hijo(dir, base);
            if (idx > 0 && !fs_borrar(idx) && !fs_guardar()) r->eax = 0;
        }
        break;
    }
    case 17: {   /* dormir(ms): bloquea hasta el tick; no hay bucle activo */
        uint32_t ms = r->ebx;
        if (ms > 600000) ms = 600000;
        r->eax = 0;
        if (di < 0) {
            uint32_t fin = irq_ticks() + (ms + 9) / 10 + 1;
            while ((int32_t)(irq_ticks() - fin) < 0) hlt();
            break;
        }
        uint32_t hasta = irq_ticks() + (ms + 9) / 10 + 1;
        proc_actual->modo = 0;
        proc_bloquea_ticks(hasta);
        r = regs_actual(r);
        break;
    }
    case 18:
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; break; }
        g_destino((int)r->ebx ? 1 : 0);
        r->eax = 0;
        break;
    case 19:
        if (!gfx_es_dueno(di)) { r->eax = (uint32_t)-1; break; }
        g_presentar((int)r->ebx, (int)r->ecx, (int)r->edx,
                    (int)r->esi);
        r->eax = 0;
        break;
    case 20: {
        const uint32_t *sp = (const uint32_t *)r->edx;
        uint32_t w, h;
        r->eax = (uint32_t)-1;
        if (!gfx_es_dueno(di)) break;
        if (sp && ptr_ok(di, r->edx, 8, 0) && (w = sp[0]) >= 1 && w <= 512 &&
            (h = sp[1]) >= 1 && h <= 512 && w * h <= 262144 &&
            ptr_ok(di, r->edx + 8, w * h * 4, 0)) {
            g_sprite((int)r->ebx, (int)r->ecx, (int)w, (int)h, &sp[2]);
            r->eax = 0;
        }
        break;
    }
    case 21: {
        const uint32_t *sp = (const uint32_t *)r->edx;
        uint32_t w, h;
        r->eax = (uint32_t)-1;
        if (!gfx_es_dueno(di)) break;
        if (sp && ptr_ok(di, r->edx, 8, 0) && (w = sp[0]) >= 1 && w <= 512 &&
            (h = sp[1]) >= 1 && h <= 512 && w * h <= 262144 &&
            ptr_ok(di, r->edx + 8, w * h * 4, 0)) {
            g_sprite_t((int)r->ebx, (int)r->ecx, (int)w, (int)h, &sp[2],
                       r->esi);
            r->eax = 0;
        }
        break;
    }
    case 22: {
        const int *op = (const int *)r->ebx;
        r->eax = (uint32_t)-1;
        if (!gfx_es_dueno(di)) break;
        if (op && ptr_ok(di, r->ebx, 24, 0)) {
            g_blit(op[0], op[1], op[2], op[3], op[4], op[5]);
            r->eax = 0;
        }
        break;
    }
    case 23:
        r->eax = g_leer_pixel((int)r->ebx, (int)r->ecx);
        break;
    case 24: {
        int fd = (int)r->ebx;
        uint32_t tam = r->ecx;
        r->eax = (uint32_t)-1;
        if (fd >= 0 && fd < NFD && fds()[fd] >= 0) {
            if (!fs_truncar(fds()[fd], tam) && !fs_guardar()) {
                if (fds_pos()[fd] > tam) fds_pos()[fd] = tam;
                r->eax = 0;
            }
        }
        break;
    }
    case 25: {
        /* ejecutar hijo preservando al padre: guarda imagen+brk+fds,
           lanza sin reset, restaura y devuelve rc. Sin anidar. */
        const char *pa = (const char *)r->ebx;
        const char *aa = (const char *)r->ecx;
        char ruta[128], args[256];
        uint32_t i = 0, k = 0;
        int rc;
        if (!pa) { r->eax = (uint32_t)-1; break; }
        if (!ptr_ok(di, r->ebx, 128, 0)) break;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        if (aa) {
            if (!ptr_ok(di, r->ecx, 1, 0)) break;
            while (k < 255 && aa[k]) { args[k] = aa[k]; k++; }
        }
        args[k] = 0;
        r->eax = (uint32_t)-1;
        if (di >= 0) proc_actual->modo = 0;   /* espera dentro de la syscall */
        rc = kern_ejecutar(0, ruta, args);
        r = regs_actual(r);
        r->eax = (uint32_t)rc;
        break;
    }
    case 26: {
        extern void shell(void);
        shell_anidada++;
        shell();
        shell_anidada--;
        r->eax = 0;
        break;
    }
    case 27: {
        extern int kbd_hay(void);
        r->eax = (uint32_t)kbd_hay();
        break;
    }

    /* ---- v1.9: procesos ------------------------------------------- */
    case 28: {   /* crear(nombre, args) -> pid (>0) o error */
        const char *pa = (const char *)r->ebx;
        const char *aa = (const char *)r->ecx;
        char ruta[128], args[256];
        uint32_t i = 0, k = 0;
        r->eax = (uint32_t)-1;
        if (!pa || !ptr_ok(di, r->ebx, 32, 0)) break;
        while (i < 127 && pa[i]) { ruta[i] = pa[i]; i++; }
        ruta[i] = 0;
        if (aa && ptr_ok(di, r->ecx, 1, 0)) {
            while (k < 255 && aa[k]) { args[k] = aa[k]; k++; }
        }
        args[k] = 0;
        r->eax = (uint32_t)kern_crea_proceso(ruta, args, 0);
        break;
    }
    case 29:     /* mi_pid() */
        r->eax = (uint32_t)(proc_actual ? proc_actual->pid : 0);
        break;
    case 30: {   /* listar(buf, max) -> numero de tareas escritas */
        /* Cada entrada: pid, estado, rc, grafico, marcos, nombre[16] */
        uint32_t max = r->ecx;
        if (max == 0) { r->eax = 0; break; }
        if (max > 1 + MAXPROC) max = 1 + MAXPROC;
        uint32_t n = 0;
        r->eax = 0;
        if (!ptr_ok(di, r->ebx, 36 * max, 1)) break;
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
        break;
    }
    case 31: {   /* esperar(pid) -> rc del hijo */
        int pid = (int)r->ebx;
        r->eax = (uint32_t)-1;
        if (pid <= 0) break;
        if (proc_actual && !proc_actual->es_usuario) {
            /* desde el nucleo no se bloquea: se consulta el estado */
            proc_t *h = proc_por_pid(pid);
            if (h && h->estado == EST_FIN) r->eax = (uint32_t)h->rc;
            break;
        }
        int _rc = proc_espera_hijo(pid);
        r = regs_actual(r);
        r->eax = (uint32_t)_rc;
        break;
    }
    case 32:     /* ceder() */
        if (proc_actual && proc_actual->es_usuario) {
            proc_actual->modo = 0;
            proc_cede();
            r->eax = 0;
        } else {
            r->eax = 0;
        }
        break;
    case 33: {   /* info_memoria(buf) -> 0; buf: 5 x uint32 */
        uint32_t v[5];
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->ebx, 20, 1)) break;
        v[0] = pag_marcos_totales();
        v[1] = pag_marcos_libres();
        v[2] = pag_marcos_usados();
        v[3] = pag_ram_total();
        v[4] = pag_e820_valido() ? (uint32_t)pag_e820_entradas() : 0;
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ebx, v, 20)) break;
        } else {
            for (int i = 0; i < 5; i++) ((uint32_t *)r->ebx)[i] = v[i];
        }
        r->eax = 0;
        break;
    }
    case 36: {   /* info_memoria2(buf) -> 0; buf: 8 x uint32 (v2.0).
                    34-35 reservadas para IPC (etapa 1, sin implementar). */
        uint32_t v[8];
        r->eax = (uint32_t)-1;
        if (di >= 0 && !ptr_ok(di, r->ebx, 32, 1)) break;
        v[0] = pag_marcos_libres();
        v[1] = pag_marcos_usados();
        v[2] = pag_usados_usuario();
        v[3] = pag_pico_usuario();
        v[4] = pag_doble_free();
        v[5] = proc_fugas() + pag_verifica();
        v[6] = pag_pd_usados();
        v[7] = pag_pt_usadas();
        if (di >= 0) {
            if (pag_escribe_usr((unsigned)di, r->ebx, v, 32)) break;
        } else {
            for (int i = 0; i < 8; i++) ((uint32_t *)r->ebx)[i] = v[i];
        }
        r->eax = 0;
        break;
    }
    default:
        r->eax = (uint32_t)-1;
        break;
    }
}

void syscall_init(void) {
}
