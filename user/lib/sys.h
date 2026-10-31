#ifndef F32_SYS_H
#define F32_SYS_H

/* Llamadas FacilOS32 desde programas: int 0x80.
   0=salir, 1=imprimir(ptr,len), 2=ticks, 3=abrir, 4=leer, 5=escribir,
   6=cerrar, 7=memoria, 8=leer_tecla, 9=crear, 10=pixel, 11=linea, 12=rectangulo,
   13=texto, 14=limpiar_pantalla. Sin libc: todo estatico.
   v1.6: 15=buscar, 16=borrar, 17=dormir, 18=destino, 19=presentar,
   20=sprite, 21=sprite_t, 22=blit, 23=leer_pixel, 24=truncar.
   v1.7: 25=ejecutar (hijo con estado preservado), 26=terminal (shell),
   27=hay_tecla (sin bloqueo).
   leer/escribir usan el offset del fd (buscar lo mueve); escribir
   extiende pero no trunca (usar truncar). */

static inline int sys_llama(int n, int a, int b, int c) {
    int r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "0"(n), "b"(a), "c"(b), "d"(c)
                      : "memory");
    return r;
}

static inline int sys_llama5(int n, int a, int b, int c, int d, int e) {
    int r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "0"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e)
                      : "memory");
    return r;
}

static inline void sys_salir(int codigo) {
    sys_llama(0, codigo, 0, 0);
    for (;;) { }
}

static inline int sys_imprimir(const char *s, int len) {
    return sys_llama(1, (int)s, len, 0);
}

static inline int sys_ticks(void) {
    return sys_llama(2, 0, 0, 0);
}

static inline int sys_abrir(const char *ruta) {
    return sys_llama(3, (int)ruta, 0, 0);
}

static inline int sys_leer(int fd, void *buf, int n) {
    return sys_llama(4, fd, (int)buf, n); /* avanza offset; 0 = fin */
}

static inline int sys_escribir(int fd, const void *buf, int n) {
    return sys_llama(5, fd, (int)buf, n);
}

static inline int sys_cerrar(int fd) {
    return sys_llama(6, fd, 0, 0);
}

static inline int sys_memoria(int n) {
    return sys_llama(7, n, 0, 0);
}

static inline int sys_leer_tecla(void) {
    return sys_llama(8, 0, 0, 0);
}

static inline int sys_crear(const char *ruta) {
    return sys_llama(9, (int)ruta, 0, 0);
}

static inline int sys_pixel(int x, int y, int color) {
    return sys_llama(10, x, y, color);
}

static inline int sys_linea(int x1, int y1, int x2, int y2, int color) {
    return sys_llama5(11, x1, y1, x2, y2, color);
}

static inline int sys_rectangulo(int x, int y, int w, int h, int color) {
    return sys_llama5(12, x, y, w, h, color);
}

static inline int sys_texto(int x, int y, const char *s, int len, int color) {
    return sys_llama5(13, x, y, (int)s, len, color);
}

static inline int sys_limpiar_pantalla(int color) {
    return sys_llama(14, color, 0, 0);
}

/* v1.6 */
#define BUSCAR_INI 0
#define BUSCAR_ACT 1
#define BUSCAR_FIN 2

static inline int sys_buscar(int fd, int off, int base) {
    return sys_llama(15, fd, off, base);
}

static inline int sys_borrar(const char *ruta) {
    return sys_llama(16, (int)ruta, 0, 0);
}

static inline int sys_dormir(int ms) {
    return sys_llama(17, ms, 0, 0);
}

static inline int sys_destino(int d) {
    return sys_llama(18, d, 0, 0);
}

static inline int sys_presentar(int x, int y, int w, int h) {
    return sys_llama5(19, x, y, w, h, 0);
}

static inline int sys_sprite(int x, int y, const void *spr) {
    return sys_llama(20, x, y, (int)spr);
}

static inline int sys_sprite_t(int x, int y, const void *spr, int clave) {
    return sys_llama5(21, x, y, (int)spr, clave, 0);
}

static inline int sys_blit(const void *op) {
    return sys_llama(22, (int)op, 0, 0);
}

static inline int sys_leer_pixel(int x, int y) {
    return sys_llama(23, x, y, 0);
}

static inline int sys_truncar(int fd, int tam) {
    return sys_llama(24, fd, tam, 0);
}

/* v1.7: ejecutar preserva al padre (rc del hijo, <0 error); terminal
   abre la shell y vuelve al salir; hay_tecla no bloquea. */
static inline int sys_ejecutar(const char *ruta, const char *args) {
    return sys_llama(25, (int)ruta, (int)args, 0);
}

static inline int sys_terminal(void) {
    return sys_llama(26, 0, 0, 0);
}

static inline int sys_hay_tecla(void) {
    return sys_llama(27, 0, 0, 0);
}

/* v1.9 - procesos. Cada proceso tiene su propio espacio de paginas.
   28=crear(nombre,args) -> pid (>0) o error negativo,
   29=mi_pid() -> pid, 30=listar(buf) -> n tareas (36 B cada una:
   pid, estado, rc, grafico, marcos, nombre[16]),
   31=esperar(pid) -> rc del hijo, 32=ceder() -> 0,
   33=info_memoria(buf) -> 0; buf = 5 x uint32 (marcos totales, libres,
   usados, RAM util, entradas E820).
   v2.0: 34-35 reservadas para IPC (sin implementar); 36=info_memoria2(buf),
   buf = 8 x uint32 (libres, usados, usados_usuario, pico_usuario,
   doble_free, anomalias, pd_usados, pt_usadas). */
static inline int sys_crear_proceso(const char *nombre, const char *args) {
    return sys_llama(28, (int)nombre, (int)args, 0);
}

static inline int sys_mi_pid(void) {
    return sys_llama(29, 0, 0, 0);
}

static inline int sys_listar(void *buf, int max) {
    return sys_llama(30, (int)buf, max, 0);
}

static inline int sys_esperar(int pid) {
    return sys_llama(31, pid, 0, 0);
}

static inline int sys_ceder(void) {
    return sys_llama(32, 0, 0, 0);
}

static inline int sys_info_memoria(unsigned int *buf) {
    return sys_llama(33, (int)buf, 0, 0);
}

static inline int sys_info_memoria2(unsigned int *buf) {
    return sys_llama(36, (int)buf, 0, 0);
}

static inline int sys_proc_stat(int pid, unsigned int *buf) {
    return sys_llama(37, pid, (int)buf, 0);
}

#define PROC_NUEVO 0
#define PROC_LISTO 1
#define PROC_EJECUTO 2
#define PROC_BLOQ 3
#define PROC_FIN 4

#endif
