#ifndef F32_PROC_H
#define F32_PROC_H
#include <stdint.h>

/* FacilOS32 v1.9 - procesos y planificador Round Robin sobre el PIT.
 *
 * Tarea 0 = "shell": el nucleo en anillo 0 (tambien es donde vive el
 * arranque y desde donde se llama a la shell del sistema). Las tareas
 * 1..MAXPROC son procesos de usuario en anillo 3, cada uno con su
 * directorio de paginas, sus marcos y su pila de kernel.
 *
 * Estados: NUEVO -> LISTO -> EJECUTANDO -> {LISTO, BLOQUEADO, TERMINADO}
 * Las transiciones las hace proc.c; fuera de el solo se leen.
 */

#define MAXPROC 8
#define NFD 16

#define EST_NUEVO 0
#define EST_LISTO 1
#define EST_EJECUTO 2
#define EST_BLOQ 3
#define EST_FIN 4

#define PILA_USR_PAGS 32        /* 128 KB de pila de usuario */
#define MARCOS_MAX 768          /* marcos por proceso (codigo+BSS+pila+heap) */

typedef struct {
    int usado;
    int pid;
    char nombre[16];
    int estado;
    int es_usuario;
    unsigned di;                /* indice del directorio de paginas */
    uint32_t cr3;
    uint32_t esp_k;             /* cima de la pila de anillo 0 */
    int pila_kern;              /* ranura de la pila de kernel (-1 = ninguna) */

    /* Contexto de usuario */
    uint32_t esp, ebp, eax, ebx, ecx, edx, esi, edi, eflags, eip;
    uint32_t esp_guard;         /* ESP de anillo 0 al ceder el kernel */
    uint32_t k_eax, k_ecx, k_edx, k_ebx, k_esi, k_edi;   /* GTR de anillo 0 al ceder */
    uint32_t k_ebp;             /* EBP del llamante al ceder (marco a retomar) */
    uint32_t k_ret;                        /* etiqueta de vuelta al ceder */
    void *sc_regs;              /* marco de la syscall en curso (por proceso) */

    /* Regiones virtuales del proceso (v2.0: seguimiento para diagnostico
       y futura asignacion dinamica; el chequeo de destruccion las usa). */
#define VMAR_N 8
#define VMAR_CODIGO 1
#define VMAR_PILA 2
#define VMAR_HEAP 3
    struct {
        uint32_t ini, tam;
        uint32_t tipo;
    } vmars[VMAR_N];
    int nvmars;
    int modo;                   /* 0 = reanudar en anillo 0, 1 = en anillo 3 */

    /* Memoria */
    uint32_t marcos[MARCOS_MAX];
    int nmarcos;
    uint32_t cod_pags;          /* paginas de codigo+BSS */

    /* Estado de usuario */
    int rc;
    int ppid;
    int fds[NFD];
    uint32_t fds_pos[NFD];
    uint32_t brk;
    int grafico;                /* ha usado el framebuffer (o es el menu) */
    int espera_tecla;
    uint32_t dormir_hasta;
    int ultimo_exc;             /* excepcion que lo mato, 0 si ninguna */
    uint32_t loteria;
    int reciclable;             /* terminado y ya nadie lee su rc */

    /* Observabilidad del planificador (v2.0, entrega 3). */
    uint32_t ticks_cpu;         /* ticks PIT visto en ejecucion */
    uint32_t cedes;             /* veces que se le cedio la CPU */
    uint32_t pico_marcos;       /* maximo nmarcos simultaneo */

    /* Buzon IPC (v2.0, entrega 5): cola FIFO de mensajes. */
#define IPC_NMSG 16
#define IPC_MAX 64
    struct {
        int ocupado;
        int emisor;
        uint32_t len;
        uint8_t datos[IPC_MAX];
    } buzon[IPC_NMSG];
    int buz_ini, buz_fin, buz_n;
    int espera_msg;             /* bloqueado esperando mensaje */
} proc_t;

extern proc_t proc_tareas[1 + MAXPROC];
extern proc_t *proc_actual;

/* --- ciclo de vida --------------------------------------------------- */
int proc_crea(const char *nombre, unsigned di, uint32_t eip, uint32_t esp);
void proc_libera(proc_t *p);
void proc_termina(proc_t *p, int rc);
proc_t *proc_por_pid(int pid);
proc_t *proc_por_indice(int i);
void proc_anota_marco(proc_t *p, uint32_t fis);
/* Pila de anillo 0 por proceso, en la zona reservada 0xE00000. */
uint32_t proc_toma_pila_kernel(void);
int proc_slot_de_pila(uint32_t cima);
void proc_devuelve_pila_kernel(int slot);
int proc_cuantos(void);
int proc_ultimo_pid(void);
void proc_fusiona(void);                /*回收: libera marcos de terminados */
uint32_t proc_fugas(void);              /* desajustes destruccion detectados */
int proc_vma_add(proc_t *p, uint32_t ini, uint32_t tam, uint32_t tipo);

/* --- planificador ---------------------------------------------------- */
void proc_inicia(void);
void proc_cede(void);                 /* voluntaria; si no hay nadie, sigue */
void proc_bloquea_tecla(void);
void proc_bloquea_ticks(uint32_t hasta);
void proc_bloquea_msg(void);
int  proc_espera_hijo(int pid);
void proc_muere(int rc);              /* no retorna */
void proc_guarda_usuario(proc_t *p, const void *marco);
void proc_estado_actual(void);
void proc_tick(void);
void proc_despierta_tecla(void);
void proc_despierta_hijos(int pid);
int  proc_hay_graficos(void);
#endif
