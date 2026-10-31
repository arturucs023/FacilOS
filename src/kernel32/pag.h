#ifndef F32_PAG_H
#define F32_PAG_H
#include <stdint.h>

/* FacilOS32 v1.9 - paginacion de 32 bits con paginas de 4 KiB.
 *
 * Reparto de direcciones (el mismo fisico que v1.8, para no romper nada):
 *   0x000000-0x0FFFFF  BIOS, fuente 0x6000, bootinfo 0x7000, E820 0x5000,
 *                      pila de arranque 0x90000, copias temporales
 *   0x100000-0x1FFFFF  nucleo (codigo, datos, BSS)          supervisor
 *   0x200000-0x2DFFFF  usuario: codigo+BSS y pila           usuario
 *   0x2E0000-0x2EFFFF  cima de la pila de usuario (0x2F0000 argumentos)
 *   0x2D0000           pagina de guardia bajo la pila (no presente)
 *   0x300000-0x47FFFF  usuario: heap (sbrk)                  usuario
 *   0x480000-0x5FFFFF  pilas de anillo 0                    supervisor
 *   0x600000-0x63FFFF  pool de datos del FS en RAM          supervisor
 *   0x800000-0x9FFFFF  heap del kernel                      supervisor
 *   0xA00000-0xBFFFFF  dorso grafico                        supervisor
 *   0xC00000-0xC5A000  tablas de pagina (PDs y PTs)         supervisor
 *   0x40000000         ventana del kernel para tocar memoria de usuario
 *   LFB (VESA)         ventana de 4 MB                      supervisor
 *
 * Cada proceso tiene su propio directorio y sus propias paginas de
 * usuario; el nucleo (0-0x1FFFFF, 0x480000-0x7FFFFF, 0x800000-0xCFFFFF y el
 * LFB) se comparte como supervisor, asi que un proceso de usuario no
 * puede escribirlo (bits de privilegio 0 y CR0.WP mas adelante).
 */

#define PAG_MARCO 4096u
#define PAG_PRESENTE 0x01u
#define PAG_ESCRITURA 0x02u
#define PAG_USUARIO 0x04u
#define PAG_CACHE 0x08u
#define PAG_TAMANO (PAG_MARCO * 1024u)   /* 4 MiB: un directorio */

#define PAG_BASE_TABLAS 0xC00000u
#define PAG_MAX_PD 12u
#define PAG_MAX_PT 96u
#define PAG_VENTANA 0x40000000u

/* Topes del gestor de marcos. */
#define PAG_RAM_MAX 0x4000000u    /* 64 MB administrables */
#define PAG_RAM_MIN 0x400000u     /* 4 MB: por debajo, alternativa insegura */

void pag_inicia(void);
int pag_hay_mapa(void);

/* Marcos fisicos. */
int32_t pag_toma_marco(void);
void pag_devuelve_marco(uint32_t fis);
uint32_t pag_marcos_totales(void);
uint32_t pag_marcos_libres(void);
uint32_t pag_marcos_usados(void);

/* Tablas. pag_dir(0) es el directorio del nucleo (plantilla). */
uint32_t pag_dir(unsigned i);
uint32_t pag_dir_nucleo(void);
int pag_nuevo_dir(void);
void pag_libera_dir(int di);          /* copia la parte del nucleo; -1 si no hay */

/* Mapear/desmapear paginas de 4 KiB en un directorio. */
int pag_mapa(unsigned di, uint32_t virt, uint32_t fis, uint32_t flags);
int pag_mapa_nuevas(unsigned di, uint32_t virt, uint32_t tam, uint32_t flags);
int pag_desmapa(unsigned di, uint32_t virt, uint32_t tam);
int pag_translada(unsigned di, uint32_t virt, uint32_t *fis);
int pag_presente(unsigned di, uint32_t virt);

/* Validacion para syscalls: [virt, virt+n) mapeada, de usuario y con los
 * permisos pedidos. Devuelve 1 si vale. */
int pag_valida_usuario(unsigned di, uint32_t virt, uint32_t n, int escritura);

/* Ventana del kernel: mapea un marco fisico en PAG_VENTANA. */
void pag_ventana(uint32_t fis);
void pag_ventana_cierra(void);

/* Copia del kernel hacia/desde memoria de usuario (pagina a pagina, con
 * validacion previa). Devuelve 0 o -1. */
int pag_escribe_usr(unsigned di, uint32_t virt, const void *src, uint32_t n);
int pag_lee_usr(unsigned di, uint32_t virt, void *dst, uint32_t n);

/* CR3 */
void pag_usa(unsigned di);
void pag_tlb(void);

/* E820 */
uint32_t pag_ram_total(void);
int pag_e820_valido(void);
int pag_e820_entradas(void);

#endif
