#include "ulib.h"

void __main(void) {
}

/* sucio32 (v2.0): syscalls con argumentos invalidos deben devolver error
 * controlado (-1 o codigo negativo), nunca panico ni corrupcion.
 * Devuelve nº de fallos. */

static int fallos = 0;
static int pruebas = 0;

static void ok(int cond, const char *nom) {
    pruebas++;
    if (!cond) {
        fallos++;
        u_printf("FALLO %s\n", nom);
    } else {
        u_printf("ok %s\n", nom);
    }
}

int main(void) {
    char buf[64];
    unsigned int v[8];
    u_print("sucio32 v2.0\n");
    ok(sys_imprimir((const char *)0x100, 10) == -1, "imp-puntero-malo");
    ok(sys_imprimir((const char *)0x200000, 0) == 0, "imp-cero");
    ok(sys_leer(99, buf, 10) == -1, "leer-fd-malo");
    ok(sys_leer(-1, buf, 10) == -1, "leer-fd-neg");
    ok(sys_escribir(99, buf, 10) == -1, "escribir-fd-malo");
    ok(sys_buscar(99, 0, 0) == -1, "buscar-fd-malo");
    ok(sys_truncar(99, 10) == -1, "truncar-fd-malo");
    ok(sys_borrar("noexiste_xyz") == -1, "borrar-no");
    ok(sys_abrir("noexiste_xyz") < 0, "abrir-no");
    ok(sys_abrir("") < 0, "abrir-vacio");
    /* graficos: primero toma propiedad con texto valido */
    ok(sys_texto(8, 8, "s", 1, 7) == 0, "texto-toma");
    ok(sys_texto(8, 8, (const char *)0x100, 4, 7) == -1, "texto-malo");
    ok(sys_pixel(799, 599, 7) == 0, "pixel-ok");
    ok(sys_sprite(0, 0, (const void *)0x100) == -1, "sprite-malo");
    ok(sys_blit((const void *)0x100) == -1, "blit-malo");
    /* procesos/memoria */
    ok(sys_listar((void *)0x100, 5) == 0, "listar-malo");
    ok(sys_listar(buf, 0) == 0, "listar-cero");
    ok(sys_info_memoria((unsigned int *)0x100) == -1, "info-malo");
    ok(sys_info_memoria2((unsigned int *)0x100) == -1, "info2-malo");
    ok(sys_proc_stat(9999, v) == -1, "pstat-pid-malo");
    ok(sys_proc_stat(0, (unsigned int *)0x100) == -1, "pstat-buf-malo");
    ok(sys_proc_stat(0, v) == 0, "pstat-shell-ok");
    ok(sys_esperar(-5) == -1, "esperar-neg");
    ok(sys_esperar(9999) == -1, "esperar-no");
    ok(sys_crear_proceso("noexiste_xyz", "") == -100, "crear-no");
    ok(sys_ejecutar("noexiste_xyz", "") == -100, "ejecutar-no");
    ok(sys_memoria(64 * 1024 * 1024) < 0, "sbrk-enorme");
    ok(sys_llama(99, 0, 0, 0) == -1, "sys-desconocida");
    ok(sys_llama(34, 0, 0, 0) == -1, "sys-reservada-34");
    ok(sys_llama(35, 0, 0, 0) == -1, "sys-reservada-35");
    ok(sys_ipc_hay() == 0, "hay-vacio");
    ok(sys_ipc_enviar(sys_mi_pid(), "h", 2) == 0, "hay-autoenvio");
    ok(sys_ipc_hay() == 1, "hay-uno");
    {
        char hb[8];
        int de = 0;
        ok(sys_ipc_recibir(hb, 8, &de) == 2, "hay-recoge");
    }
    ok(sys_ipc_hay() == 0, "hay-vacio2");
    /* v2.0-etapa4: raton (solo no-bloqueante; leer bloquearia). */
    ok(sys_hay_raton() == 0, "raton-vacio");
    ok(sys_dormir(50) == 0, "dormir-ok");
    /* el nucleo sigue integro */
    ok(sys_info_memoria2(v) == 0 && v[4] == 0 && v[5] == 0, "nucleo-integro");
    u_printf("sucio32: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}
