#include "seg.h"
#include "libc.h"

typedef struct {
    uint16_t lim_lo;
    uint16_t base_lo;
    uint8_t base_mid;
    uint8_t acceso;
    uint8_t gran;
    uint8_t base_hi;
} __attribute__((packed)) gdt_ent_t;

typedef struct {
    uint32_t prev, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
    uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trampa;
    uint16_t mapa;
} __attribute__((packed)) tss_t;

static gdt_ent_t gdt[6];
static tss_t tss;
struct {
    uint16_t lim;
    uint32_t base;
} __attribute__((packed)) gdt_ptr;

uint32_t kern_esp_save = 0;
int user_rc = 0;
int en_usuario = 0;

/* v1.9: esp0 pasa a gestionarlo el planificador (una pila de anillo 0 por
   proceso). Para el arranque se deja el valor clasico. */
#define IRQ_PILA0 0x8F000

void tss_pon_esp0(uint32_t v) { tss.esp0 = v; }

/* Diagnostico v1.7: lee esp0 del TSS. */
uint32_t seg_esp0(void) {
    return tss.esp0;
}

extern void gdt_cargar(void);
extern void tss_cargar(void);
extern void entrar_usuario(uint32_t entry, uint32_t uesp);
extern void salir_usuario(void);

static void pon_gdt(int n, uint32_t base, uint32_t lim, uint8_t acc, uint8_t gran) {
    gdt[n].lim_lo = (uint16_t)(lim & 0xFFFF);
    gdt[n].base_lo = (uint16_t)(base & 0xFFFF);
    gdt[n].base_mid = (uint8_t)((base >> 16) & 0xFF);
    gdt[n].acceso = acc;
    gdt[n].gran = (uint8_t)(((lim >> 16) & 0x0F) | (gran & 0xF0));
    gdt[n].base_hi = (uint8_t)((base >> 24) & 0xFF);
}

void seg_init(void) {
    __asm__ volatile ("cli");
    pon_gdt(0, 0, 0, 0, 0);
    pon_gdt(1, 0, 0xFFFFF, 0x9A, 0xCF);
    pon_gdt(2, 0, 0xFFFFF, 0x92, 0xCF);
    pon_gdt(3, 0, 0xFFFFF, 0xFA, 0xCF);
    pon_gdt(4, 0, 0xFFFFF, 0xF2, 0xCF);
    uint32_t tb = (uint32_t)&tss;
    for (uint32_t i = 0; i < sizeof(tss); i++) ((uint8_t *)&tss)[i] = 0;
    tss.ss0 = 0x10;
    /* v1.9: proc_inicia() pone la pila de la tarea del sistema; hasta
       entonces la pila de IRQ clasica. */
    tss.esp0 = 0x8F000;
    tss.mapa = sizeof(tss);
    gdt[5].lim_lo = (uint16_t)(sizeof(tss) - 1);
    gdt[5].base_lo = (uint16_t)(tb & 0xFFFF);
    gdt[5].base_mid = (uint8_t)((tb >> 16) & 0xFF);
    gdt[5].acceso = 0x89;
    gdt[5].gran = 0x00;
    gdt[5].base_hi = (uint8_t)((tb >> 24) & 0xFF);
    gdt_ptr.lim = sizeof(gdt) - 1;
    gdt_ptr.base = (uint32_t)gdt;
    gdt_cargar();
    tss_cargar();
    __asm__ volatile ("sti");
}

/* Pila de anillo 0 para interrupciones en anillo 3 (v1.9: la asigna el
   planificador, una por proceso; ver proc.c). Esta funcion se queda solo
   para el arranque directo, antes de que haya procesos. */
static uint32_t pila_anon = 0;

int lanzar_usuario2(uint32_t entry, uint32_t uesp, int fresco) {
    if (fresco) {
        tss.esp0 = IRQ_PILA0;
    } else {
        if (pila_anon == 0) pila_anon = 0x700000;
        if (pila_anon <= 0x600000) pila_anon = 0x600000;
        pila_anon -= 0x10000;
        tss.esp0 = pila_anon;
    }
    en_usuario = 1;
    entrar_usuario(entry, uesp);
    en_usuario = 0;
    return user_rc;
}

int lanzar_usuario(uint32_t entry, uint32_t uesp) {
    return lanzar_usuario2(entry, uesp, 1);
}
