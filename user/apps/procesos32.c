#include "ulib.h"

void __main(void) {
}

/* procesos32 (v1.9): tabla de procesos del sistema.
   - F5 o Intro: refresco
   - procesos <nombre>: crea un proceso de ese nombre del catalogo
   - ESP: salir
   Tambien imprime el estado de la memoria (syscall 33). */

static void cabecera(void) {
    u_print("\r\n");
    u_print("procesos32 v1.9 - tabla de procesos\r\n");
    u_print(" pid  estado      rc  mar  nombre\r\n");
}

static void linea(uproc_t *p) {
    char b[16];
    u_print(" ");
    u_itoa(p->pid, b);
    u_print(b);
    u_print("  ");
    u_print(u_estado_nombre(p->estado));
    u_print("  ");
    u_itoa(p->rc, b);
    u_print(b);
    u_print("  ");
    u_itoa(p->marcos, b);
    u_print(b);
    u_print("  ");
    u_print(p->nombre);
    u_print("\r\n");
}

static void memoria(void) {
    unsigned int v[5];
    if (u_info_memoria(v)) {
        u_print("no se pudo leer la memoria del sistema\r\n");
        return;
    }
    u_print("memoria: marcos ");
    u_print_num((int)v[0]);
    u_print(" totales, ");
    u_print_num((int)v[1]);
    u_print(" libres, ");
    u_print_num((int)v[2]);
    u_print(" usados; RAM util ");
    u_print_num((int)v[3] / 1024);
    u_print(" KB; E820 ");
    u_print_num((int)v[4]);
    u_print(" entradas\r\n");
}

int main(void) {
    uproc_t p[12];
    int n;
    for (;;) {
        cabecera();
        n = u_listar(p, 12);
        for (int i = 0; i < n; i++) linea(&p[i]);
        u_print("yo soy el proceso ");
        u_print_num(u_mi_pid());
        u_print("\r\n");
        memoria();
        u_print("[Intro] refrescar  [p] crear  [t1_lib] crear t1_lib  "
                 "[ESC] salir\r\n");
        int t = u_tecla();
        if (t == 27) break;
        if (t == 13 || t == 'r') continue;
        if (t == 'p' || t == 't') {
            const char *prog = (t == 't') ? "t1_lib" : "reloj";
            int pid = u_crear(prog, "");
            u_print("crear ");
            u_print(prog);
            u_print(" -> ");
            u_print_num(pid);
            u_print(pid > 0 ? " (pid)\r\n" : " (error)\r\n");
            continue;
        }
    }
    u_print("procesos32 terminado\r\n");
    return 0;
}
