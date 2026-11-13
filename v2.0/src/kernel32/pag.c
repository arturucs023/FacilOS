#include "pag.h"
#include "libc.h"
#include "cpu.h"
#include "bootinfo.h"
#include "klog.h"

/* FacilOS32 v1.9: marcos fisicos, directorios y tablas de pagina.
 *
 * Las tablas (pd_pool/pt_pool/ventana_pt) viven en el .bss del nucleo
 * (ver pag.c), no en 0xC00000: ese comentario de v1.8 quedo obsoleto.
 *
 * Decisiones de diseno:
 *  - Paginas de 4 KiB en todas partes (sin paginas grandes).
 *  - El nucleo se mapea por identidad y es SUPERVISOR en todos los
 *    procesos; los procesos comparten esas mismas paginas fisicas, asi
 *    que un proceso de usuario que escriba ahi provoca #PF.
 *  - El bit U/S se decide por pagina (PT), no por directorio: un mismo
 *    directorio cubre rango de usuario y de nucleo.
 *  - El nucleo nunca cambia de CR0 para tocar memoria de usuario: usa una
 *    ventana (0x40000000) mapeada con PAG_USUARIO=0, de modo que ningun
 *    proceso puede usarla ni siquiera por error.
 */

#define E820_BASE 0x5000u
#define E820_MAX 32

static uint32_t pd_pool[PAG_MAX_PD][1024] __attribute__((aligned(4096)));
static uint32_t pt_pool[PAG_MAX_PT][1024] __attribute__((aligned(4096)));
static uint32_t ventana_pt[1024] __attribute__((aligned(4096)));

/* Que tablas de pagina usa cada directorio: las dos propias de 0-8 MB. Se
   guardan para devolverlas al pool al liberar el proceso (si no, tras unas
   decenas de procesos se agotarian las PT). */
static unsigned pt_de_dir[PAG_MAX_PD][2];

static uint8_t mapa_marcos[PAG_RAM_MAX / PAG_MARCO / 8] __attribute__((aligned(4)));static uint8_t marco_dueno[PAG_RAM_MAX / PAG_MARCO];
static uint8_t pd_uso[PAG_MAX_PD];
static uint8_t pt_uso[PAG_MAX_PT];
static uint32_t marcos_top = 0;
static uint32_t ram_total = 0;
static int e820_ok = 0;
static int e820_n = 0;
static int con_paginacion = 0;
static uint32_t usados_usuario = 0;
static uint32_t pico_usuario = 0;
static uint32_t doble_free = 0;

static uint32_t *pt_de(uint32_t *pd, uint32_t virt);

uint32_t pag_dir(unsigned i) { return (uint32_t)pd_pool[i]; }
uint32_t pag_dir_nucleo(void) { return (uint32_t)pd_pool[0]; }
int pag_hay_mapa(void) { return con_paginacion; }
uint32_t pag_ram_total(void) { return ram_total; }
int pag_e820_valido(void) { return e820_ok; }
int pag_e820_entradas(void) { return e820_n; }

static int bit_marco(uint32_t f) { return (mapa_marcos[f >> 3] >> (f & 7)) & 1; }

static void pon_bit(uint32_t f, int v) {
    uint8_t m = (uint8_t)(1u << (f & 7));
    if (v) mapa_marcos[f >> 3] |= m;
    else mapa_marcos[f >> 3] &= (uint8_t)~m;
}

static void reserva(uint32_t ini, uint32_t fin) {
    if (fin > PAG_RAM_MAX) fin = PAG_RAM_MAX;
    for (uint32_t a = ini & ~(PAG_MARCO - 1); a < fin; a += PAG_MARCO) {
        pon_bit(a / PAG_MARCO, 1);
        marco_dueno[a / PAG_MARCO] = DUENO_RESERVA;
    }
}

static void abre(uint32_t base, uint32_t len) {
    if (len > PAG_RAM_MAX) len = PAG_RAM_MAX;
    if (base >= PAG_RAM_MAX) return;
    base &= ~(PAG_MARCO - 1);
    for (uint32_t a = base; a < base + len; a += PAG_MARCO) {
        uint32_t f = a / PAG_MARCO;
        if (f < PAG_RAM_MAX / PAG_MARCO) {
            pon_bit(f, 0);
            marco_dueno[f] = DUENO_LIBRE;
        }
    }
}

/* ---- E820 -------------------------------------------------------------- */

static void lee_e820(void) {
    e820_ok = 0;
    e820_n = 0;
    ram_total = 0;
    uint32_t libre = 0;
    int n = BI->e820n;
    if (n <= 0 || n > E820_MAX) return;
    for (int i = 0; i < n; i++) {
        const uint8_t *e = (const uint8_t *)(E820_BASE + (uint32_t)i * 24);
        uint32_t lo = (uint32_t)e[0] | ((uint32_t)e[1] << 8) |
                      ((uint32_t)e[2] << 16) | ((uint32_t)e[3] << 24);
        uint32_t hi = (uint32_t)e[4] | ((uint32_t)e[5] << 8) |
                      ((uint32_t)e[6] << 16) | ((uint32_t)e[7] << 24);
        uint32_t len = (uint32_t)e[8] | ((uint32_t)e[9] << 8) |
                       ((uint32_t)e[10] << 16) | ((uint32_t)e[11] << 24);
        uint32_t tipo = (uint32_t)e[16] | ((uint32_t)e[17] << 8) |
                        ((uint32_t)e[18] << 16) | ((uint32_t)e[19] << 24);
        if (lo == 0 && len == 0) break;          /* fin de la tabla */
        e820_n++;
        if (tipo != 1) continue;                  /* solo RAM utilizable */
        if (hi != 0) continue;                    /* por encima de 4 GB */
        if (len == 0) continue;
        uint32_t base = lo & ~(PAG_MARCO - 1);
        if (base < 0x100000) base = 0x100000;    /* el 1er MB es del sistema */
        if (base >= lo + len) continue;
        uint32_t l = (lo + len) - base;
        abre(base, l);
        libre += l;
    }
    if (e820_n > 0 && libre >= PAG_RAM_MIN) {
        e820_ok = 1;
        ram_total = libre;
    } else {
        e820_n = 0;
        e820_ok = 0;
    }
}

/* ---- marcos ------------------------------------------------------------ */

/* Zonas que el gestor de marcos nunca entrega (ver pag.h). */
static void reserva_fijas(void) {
    reserva(0, 0x100000);          /* BIOS, fuente, bootinfo, E820, pila, copias */
    reserva(0x100000, 0x200000);   /* nucleo */
    reserva(0x1A00000, 0x1C00000); /* pilas anon arranque + pool del FS */
    reserva(0x800000, 0xD00000);   /* heap del kernel, dorso, tablas */
    reserva(0xE00000, 0xF00000);   /* pilas de anillo 0 de los procesos */
    reserva(0x1000000, 0x1800000);   /* F8: cola alta del catalogo (8 MB) */
}

/* Dorso grande (pantalla completa a 1920x1080x32): bloque contiguo alto
   detectado por E820. Reserva fija desde el arranque para que la
   contabilidad de marcos no baile. 0 si no cabe (poca RAM: el modo
   grande queda sin soporte pero el resto sigue igual). */
static uint32_t dorso_grande = 0;

uint32_t pag_dorso_grande(void) { return dorso_grande; }

static void reserva_dorso_grande(void) {
    uint32_t total = PAG_RAM_MAX / PAG_MARCO;
    uint32_t ini = total - DORSO_GRANDE_MARCOS;
    uint32_t f;
    dorso_grande = 0;
    for (;;) {
        for (f = 0; f < DORSO_GRANDE_MARCOS; f++)
            if (bit_marco(ini + f)) break;
        if (f == DORSO_GRANDE_MARCOS) break;
        if (ini <= 256) return;   /* por debajo de 1 MB: no hay sitio */
        ini--;
    }
    for (f = 0; f < DORSO_GRANDE_MARCOS; f++) {
        pon_bit(ini + f, 1);
        marco_dueno[ini + f] = DUENO_RESERVA;
    }
    dorso_grande = ini * PAG_MARCO;
}

int32_t pag_toma_marco(void) {
    for (uint32_t f = 1; f < marcos_top; f++) {
        if (!bit_marco(f)) {
            pon_bit(f, 1);
            marco_dueno[f] = DUENO_USUARIO;
            usados_usuario++;
            if (usados_usuario > pico_usuario) pico_usuario = usados_usuario;
            return (int32_t)(f * PAG_MARCO);
        }
    }
    return -1;
}

void pag_devuelve_marco(uint32_t fis) {
    if (fis >= PAG_RAM_MAX) return;
    uint32_t f = fis / PAG_MARCO;
    if (!bit_marco(f)) {
        doble_free++;       /* liberar un marco ya libre: se cuenta, no corrompe */
        return;
    }
    pon_bit(f, 0);
    if (marco_dueno[f] == DUENO_USUARIO && usados_usuario > 0)
        usados_usuario--;
    marco_dueno[f] = DUENO_LIBRE;
}

uint32_t pag_usados_usuario(void) { return usados_usuario; }
uint32_t pag_pico_usuario(void) { return pico_usuario; }
uint32_t pag_doble_free(void) { return doble_free; }

uint32_t pag_pd_usados(void) {
    uint32_t n = 0;
    for (unsigned i = 0; i < PAG_MAX_PD; i++) if (pd_uso[i]) n++;
    return n;
}

uint32_t pag_pt_usadas(void) {
    uint32_t n = 0;
    for (unsigned i = 0; i < PAG_MAX_PT; i++) if (pt_uso[i]) n++;
    return n;
}

uint32_t pag_verifica(void) {
    uint32_t mal = 0;
    for (uint32_t f = 0; f < marcos_top; f++) {
        int bit = bit_marco(f);
        int oc = marco_dueno[f] != DUENO_LIBRE;
        if (bit != oc) mal++;
    }
    return mal;
}

uint32_t pag_cuenta_rango(unsigned di, uint32_t virt, uint32_t tam) {
    uint32_t n = 0;
    if (di >= PAG_MAX_PD) return 0;
    for (uint32_t v = virt & ~(PAG_MARCO - 1); v < virt + tam; v += PAG_MARCO) {
        uint32_t *pt = pt_de(pd_pool[di], v);
        if (!pt) continue;
        uint32_t e = pt[(v >> 12) & 0x3FF];
        if ((e & PAG_PRESENTE) && (e & PAG_USUARIO)) n++;
    }
    return n;
}

uint32_t pag_marcos_libres(void) {
    uint32_t n = 0;
    for (uint32_t f = 0; f < marcos_top; f++) if (!bit_marco(f)) n++;
    return n;
}
uint32_t pag_marcos_totales(void) { return marcos_top; }
uint32_t pag_marcos_usados(void) { return marcos_top - pag_marcos_libres(); }

/* ---- tablas ------------------------------------------------------------ */

static uint32_t *pt_nueva(void) {
    for (unsigned k = 0; k < PAG_MAX_PT; k++) {
        if (pt_uso[k]) continue;
        pt_uso[k] = 1;
        uint32_t *pt = pt_pool[k];
        for (int i = 0; i < 1024; i++) pt[i] = 0;
        return pt;
    }
    return 0;
}

static uint32_t *pt_de(uint32_t *pd, uint32_t virt) {
    uint32_t e = pd[(virt >> 22) & 0x3FF];
    if (!(e & PAG_PRESENTE)) return 0;
    return (uint32_t *)(e & 0xFFFFF000u);
}

/* Devuelve la tabla del directorio, creandola si hace falta. El bit de
   usuario del directorio se pone siempre: quien manda es la PT. */
static uint32_t *pt_suya(uint32_t *pd, uint32_t virt) {
    uint32_t ind = (virt >> 22) & 0x3FF;
    uint32_t *pt = pt_de(pd, virt);
    if (pt) return pt;
    if (ind == (PAG_VENTANA >> 22)) pt = ventana_pt;
    else pt = pt_nueva();
    if (!pt) return 0;
    pd[ind] = (uint32_t)pt | PAG_PRESENTE | PAG_ESCRITURA | PAG_USUARIO;
    return pt;
}

int pag_mapa(unsigned di, uint32_t virt, uint32_t fis, uint32_t flags) {
    uint32_t *pd = pd_pool[di];
    uint32_t *pt = pt_suya(pd, virt);
    if (!pt) return -1;
    {
        uint32_t j = (virt >> 12) & 0x3FF;
        if (pt[j] & PAG_PRESENTE) {
            uint32_t viejo = pt[j] & 0xFFFFF000u;
            /* F8: no devolver marcos reservados del nucleo. Remapear
             * identidad con pag_mapa convertia RESERVA en LIBRE. */
            if (marco_dueno[viejo / PAG_MARCO] == DUENO_RESERVA) {
                klog(KLOG_WARN, "pag", "mapa sobre reserva");
            } else {
                pag_devuelve_marco(viejo);
            }
        }
        pt[j] = (fis & 0xFFFFF000u) | PAG_PRESENTE | flags;
    }
    cpu_tlb_invalida(virt);
    return 0;
}

int pag_mapa_nuevas(unsigned di, uint32_t virt, uint32_t tam, uint32_t flags) {
    uint32_t n = (tam + PAG_MARCO - 1) / PAG_MARCO;
    for (uint32_t i = 0; i < n; i++) {
        int32_t f = pag_toma_marco();
        if (f < 0) {
            pag_desmapa(di, virt, i * PAG_MARCO);
            return -1;
        }
        /* El marco se pone a cero por la ventana: la identidad del nucleo
           solo cubre sus propias zonas, no cualquier marco de RAM. */
        volatile uint32_t *p = (volatile uint32_t *)PAG_VENTANA;
        pag_ventana((uint32_t)f);
        for (uint32_t k = 0; k < PAG_MARCO / 4; k++) p[k] = 0;
        pag_ventana_cierra();
        if (pag_mapa(di, virt + i * PAG_MARCO, (uint32_t)f, flags)) {
            pag_devuelve_marco((uint32_t)f);
            pag_desmapa(di, virt, i * PAG_MARCO);
            return -1;
        }
    }
    return 0;
}

int pag_desmapa(unsigned di, uint32_t virt, uint32_t tam) {
    uint32_t *pd = pd_pool[di];
    uint32_t n = (tam + PAG_MARCO - 1) / PAG_MARCO;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t v = virt + i * PAG_MARCO;
        uint32_t *pt = pt_de(pd, v);
        if (!pt) continue;
        uint32_t j = (v >> 12) & 0x3FF;
        if (pt[j] & PAG_PRESENTE) {
            pag_devuelve_marco(pt[j] & 0xFFFFF000u);
            pt[j] = 0;
        }
        cpu_tlb_invalida(v);
    }
    return 0;
}

int pag_translada(unsigned di, uint32_t virt, uint32_t *fis) {
    uint32_t *pt = pt_de(pd_pool[di], virt);
    if (!pt) return -1;
    uint32_t e = pt[(virt >> 12) & 0x3FF];
    if (!(e & PAG_PRESENTE)) return -1;
    if (fis) *fis = (e & 0xFFFFF000u) + (virt & 0xFFF);
    return 0;
}

int pag_presente(unsigned di, uint32_t virt) {
    return pag_translada(di, virt, 0) == 0;
}

int pag_valida_usuario(unsigned di, uint32_t virt, uint32_t n, int escritura) {
    if (n == 0) return 1;
    uint32_t fin = virt + n;
    if (fin < virt) return 0;
    for (uint32_t v = virt & ~(PAG_MARCO - 1); v < fin; v += PAG_MARCO) {
        uint32_t *pt = pt_de(pd_pool[di], v);
        if (!pt) return 0;
        uint32_t e = pt[(v >> 12) & 0x3FF];
        if (!(e & PAG_PRESENTE)) return 0;
        if (!(e & PAG_USUARIO)) return 0;
        if (escritura && !(e & PAG_ESCRITURA)) return 0;
    }
    return 1;
}

/* ---- ventana del kernel (solo supervisor) ------------------------------ */

/* La tabla de la ventana se registra en el directorio del nucleo (sin
   bit de usuario) y los procesos la heredan al copiar su directorio: asi
   el kernel puede abrirla este en el contexto de cualquier proceso. */
static void ventana_alta(void) {
    pd_pool[0][(PAG_VENTANA >> 22) & 0x3FF] =
        (uint32_t)ventana_pt | PAG_PRESENTE | PAG_ESCRITURA;
}

void pag_ventana(uint32_t fis) {
    ventana_alta();
    ventana_pt[0] = (fis & 0xFFFFF000u) | PAG_PRESENTE | PAG_ESCRITURA;
    cpu_tlb_invalida(PAG_VENTANA);
}

void pag_ventana_cierra(void) {
    ventana_pt[0] = 0;
    cpu_tlb_invalida(PAG_VENTANA);
}

/* ---- acceso del kernel a memoria de usuario --------------------------- */
/* Copia por paginas usando la ventana: el kernel no cambia de CR0 y la
   ventana es supervisor, asi que ningun proceso puede tocarla. */

static void *copia_usr(unsigned di, uint32_t virt, void *otro, uint32_t n,
                       int escribir) {
    volatile uint8_t *vent = (volatile uint8_t *)PAG_VENTANA;
    while (n) {
        uint32_t f;
        if (pag_translada(di, virt, &f)) return 0;
        uint32_t off = virt & (PAG_MARCO - 1);
        uint32_t trozo = PAG_MARCO - off;
        if (trozo > n) trozo = n;
        pag_ventana(f & ~(PAG_MARCO - 1));
        if (escribir) {
            for (uint32_t k = 0; k < trozo; k++) vent[off + k] = ((uint8_t *)otro)[k];
        } else {
            for (uint32_t k = 0; k < trozo; k++) ((uint8_t *)otro)[k] = vent[off + k];
        }
        pag_ventana_cierra();
        virt += trozo;
        otro = (uint8_t *)otro + trozo;
        n -= trozo;
    }
    return otro;
}

int pag_escribe_usr(unsigned di, uint32_t virt, const void *src, uint32_t n) {
    if (!pag_valida_usuario(di, virt, n, 1)) return -1;
    return copia_usr(di, virt, (void *)src, n, 1) ? 0 : -1;
}

int pag_lee_usr(unsigned di, uint32_t virt, void *dst, uint32_t n) {
    if (!pag_valida_usuario(di, virt, n, 0)) return -1;
    return copia_usr(di, virt, dst, n, 0) ? 0 : -1;
}

void pag_usa(unsigned di) { cpu_cr3_pon((uint32_t)pd_pool[di]); }
void pag_tlb(void) { cpu_tlb_todo(); }

/* ---- arranque de la paginacion ----------------------------------------- */

static void mapea_rango(unsigned di, uint32_t virt, uint32_t fin, uint32_t flags) {
    for (uint32_t v = virt; v < fin; v += PAG_MARCO)
        if (pag_mapa(di, v, v, flags)) return;
}

static void mapea_lfb(void) {
    uint32_t lfb = (uint32_t)BI->lfb_lo | ((uint32_t)BI->lfb_hi << 16);
    uint32_t k;
    if (!lfb) return;
    /* Ventana de 12 MB: cabe 1920x1080x32 (8,3 MB) con margen. */
    for (k = 0; k < 3072; k++) {
        if (pag_mapa(0, lfb + k * PAG_MARCO, lfb + k * PAG_MARCO,
                     PAG_PRESENTE | PAG_ESCRITURA))
            return;
    }
}

/* v2.0-etapa4: IOAPIC (0xFEC00000) + LAPIC (0xFEE00000), identidad,
   supervisor, sin cache. Antes de crear procesos para que todos los
   directorios la hereden (el IRQ del raton puede llegar con cualquier
   CR3 y su EOI debe funcionar siempre). Si no hay APIC, las lecturas
   dan ~0xFFFFFFFF y el driver de raton lo detecta (sin tocar nada). */
static void mapea_apic(void) {
    pag_mapa(0, 0xFEC00000u, 0xFEC00000u,
             PAG_PRESENTE | PAG_ESCRITURA | PAG_NOCACHE);
    pag_mapa(0, 0xFEE00000u, 0xFEE00000u,
             PAG_PRESENTE | PAG_ESCRITURA | PAG_NOCACHE);
}

void pag_inicia(void) {
    for (uint32_t i = 0; i < PAG_MAX_PD; i++)
        for (int k = 0; k < 1024; k++) pd_pool[i][k] = 0;
    for (unsigned i = 0; i < PAG_MAX_PT; i++)
        for (int k = 0; k < 1024; k++) pt_pool[i][k] = 0;
    for (unsigned i = 0; i < PAG_MAX_PD; i++) {
        pd_uso[i] = 0;
        pt_de_dir[i][0] = pt_de_dir[i][1] = 0;
    }
    pd_uso[0] = 1;   /* directorio del nucleo */
    for (unsigned i = 0; i < PAG_MAX_PT; i++) pt_uso[i] = 0;
    for (int k = 0; k < 1024; k++) ventana_pt[k] = 0;
    for (uint32_t f = 0; f < PAG_RAM_MAX / PAG_MARCO; f++) mapa_marcos[f >> 3] = 0xFF;

    lee_e820();
    if (!e820_ok) {
        /* Alternativa segura y documentada: de 1 MB a 64 MB. */
        abre(0x100000, PAG_RAM_MAX - 0x100000);
        ram_total = PAG_RAM_MAX - 0x100000;
    }
    reserva_fijas();
    reserva_dorso_grande();
    marcos_top = PAG_RAM_MAX / PAG_MARCO;
    while (marcos_top > 0 && bit_marco(marcos_top - 1)) marcos_top--;
    /* Lo que de verdad queda disponible tras reservar las zonas fijas. */
    ram_total = (uint32_t)marcos_top * PAG_MARCO;

    /* Identidad del nucleo: 0-4 MB, 4-8 MB, 8-13 MB y el LFB. Todo
       supervisor: ningun proceso puede escribir ahi. */
    uint32_t sup = PAG_PRESENTE | PAG_ESCRITURA;
    /* 0-2 MB (nucleo), 16-24 MB (cola alta del catalogo ISO),
       26-28 MB (pilas anon de arranque y pool del FS),
       8-13 MB (heap, dorso, tablas). El resto NO se mapea: 2-8 MB
       son para los procesos, y sin mapear aqui no hay aliasing
       entre un marco de usuario y la identidad del nucleo. */
    mapea_rango(0, 0x000000, 0x200000, sup);
    mapea_rango(0, 0x1000000, 0x1800000, sup); /* F8: cola alta 8 MB */
    mapea_rango(0, 0x1A00000, 0x1C00000, sup);
    mapea_rango(0, 0x800000, 0xD00000, sup);
    mapea_rango(0, 0xE00000, 0xF00000, sup);
    mapea_lfb();
    /* Dorso grande (si cupo): identidad para que todo directorio (los
       nuevos copian este) dibuje en el sin ventana. La reserva ya se
       hizo junto a las fijas para cuadrar la contabilidad. */
    if (dorso_grande)
        mapea_rango(0, dorso_grande,
                    dorso_grande + DORSO_GRANDE_MARCOS * PAG_MARCO, sup);
    mapea_apic();

    ventana_alta();
    pag_usa(0);
    /* v2.0: WP=1 (ver D9). El nucleo escribe memoria de usuario solo por la
       ventana (supervisor) o en paginas RW validadas; las escrituras a
       paginas RO petan tambien desde Ring0. */
    cpu_cr0_pon(cpu_cr0() | CR0_PG | CR0_WP);
    con_paginacion = 1;
    cpu_tlb_todo();
}

void pag_libera_dir(int di) {
    uint32_t fl = cpu_eflags();
    cli();
    if (di <= 0 || di >= PAG_MAX_PD) { cpu_if_restaura(fl); return; }
    for (int b = 0; b < 2; b++) {
        unsigned k = pt_de_dir[di][b];
        if (k) {
            unsigned idx = k - 1;
            if (idx < PAG_MAX_PT) {
                uint32_t *pt = pt_pool[idx];
                for (int i = 0; i < 1024; i++) pt[i] = 0;
                pt_uso[idx] = 0;
            }
            pt_de_dir[di][b] = 0;
        }
    }
    for (int i = 0; i < 1024; i++) pd_pool[di][i] = 0;
    pd_uso[di] = 0;
    cpu_if_restaura(fl);
}

/* Directorio nuevo: copia el del nucleo y rehace 0-8 MB con paginas
   propias para el usuario (0-2 MB y 5-7 MB siguen siendo del nucleo). */
int pag_nuevo_dir(void) {
    uint32_t fl = cpu_eflags();
    cli();
    /* Busca un directorio libre (nunca el 0 del nucleo). */
    int di = -1;
    for (unsigned i = 1; i < PAG_MAX_PD; i++) {
        if (!pd_uso[i]) { di = (int)i; break; }
    }
    if (di < 0) { cpu_if_restaura(fl); return -1; }
    /* Necesita al menos 2 tablas de pagina libres. */
    unsigned libres_pt = 0;
    for (unsigned k = 0; k < PAG_MAX_PT; k++) if (!pt_uso[k]) libres_pt++;
    if (libres_pt < 2) { cpu_if_restaura(fl); return -1; }
    pd_uso[di] = 1;
    uint32_t *pd = pd_pool[di];
    uint32_t *pk = pd_pool[0];
    for (int i = 0; i < 1024; i++) pd[i] = pk[i];
    /* 0-4 MB y 4-8 MB: tablas propias con la misma imagen del nucleo y
       las paginas de usuario sustituidas por las del proceso. */
    uint32_t flags_pde = PAG_PRESENTE | PAG_ESCRITURA | PAG_USUARIO;
    for (int b = 0; b < 2; b++) {
        uint32_t *vieja = (uint32_t *)(pk[b] & 0xFFFFF000u);
        uint32_t *nueva = pt_nueva();
        if (!nueva) {
            /* libera lo pedido hasta ahora */
            for (int bb = 0; bb < b; bb++) {
                unsigned k = pt_de_dir[di][bb];
                if (k) { pt_uso[k - 1] = 0; pt_de_dir[di][bb] = 0; }
            }
            pd_uso[di] = 0;
            cpu_if_restaura(fl);
            return -1;
        }
        unsigned idx = (unsigned)(nueva - (uint32_t *)pt_pool[0]) / 1024u;
        pt_de_dir[di][b] = idx + 1;
        for (int i = 0; i < 1024; i++) nueva[i] = vieja[i];
        pd[b] = (uint32_t)nueva | flags_pde;
    }
    cpu_if_restaura(fl);
    return di;
}
