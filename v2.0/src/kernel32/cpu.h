#ifndef F32_CPU_H
#define F32_CPU_H
#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0,%1" :: "a"(v), "Nd"(p));
}
static inline uint8_t inb(uint16_t p) {
    uint8_t v;
    __asm__ volatile ("inb %1,%0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline void io_espera(void) {
    outb(0x80, 0);
}
static inline void cli(void) { __asm__ volatile ("cli"); }
static inline void sti(void) { __asm__ volatile ("sti"); }
static inline void hlt(void) { __asm__ volatile ("hlt"); }
static inline void pause_cpu(void) { __asm__ volatile ("pause"); }

/* Secciones criticas anidables: los ayudantes de pag/prod se llaman con
   IF=0 (syscall/IRQ) y con IF=1 (shell); un cli/sti a secas re-habilita
   interrupciones en mitad de una syscall. Se guarda/restaura IF. */
static inline uint32_t cpu_eflags(void) {
    uint32_t v;
    __asm__ volatile ("pushfl; popl %0" : "=r"(v));
    return v;
}
static inline void cpu_if_restaura(uint32_t f) {
    if (f & 0x200) __asm__ volatile ("sti");
}

/* --- v1.9: control de pagina (CR0/CR2/CR3) y TLB ------------------- */
#define CR0_PG 0x80000000u
#define CR0_WP 0x00010000u   /* v2.0: protege paginas de solo lectura tambien de escrituras del nucleo */

static inline uint32_t cpu_cr0(void) {
    uint32_t v;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(v));
    return v;
}
static inline void cpu_cr0_pon(uint32_t v) {
    __asm__ volatile ("movl %0, %%cr0" : : "r"(v));
}
static inline uint32_t cpu_cr2(void) {
    uint32_t v;
    __asm__ volatile ("movl %%cr2, %0" : "=r"(v));
    return v;
}
static inline uint32_t cpu_cr3(void) {
    uint32_t v;
    __asm__ volatile ("movl %%cr3, %0" : "=r"(v));
    return v;
}
static inline void cpu_cr3_pon(uint32_t v) {
    __asm__ volatile ("movl %0, %%cr3" : : "r"(v) : "memory");
}
/* Invalida la TLB de una pagina virtual. */
static inline void cpu_tlb_invalida(uint32_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}
static inline void cpu_tlb_todo(void) {
    __asm__ volatile ("movl %%cr3, %%eax\n\tmovl %%eax, %%cr3" : : : "eax", "memory");
}
static inline void cpu_iret(void) {
    __asm__ volatile ("iret");
}

#endif
