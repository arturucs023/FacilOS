#include "ulib.h"

void __main(void) {
}

/* aislamiento32 (v1.9): pruebas A-H de aislamiento y del planificador.
 * Los intentos peligrosos se hacen en procesos aparte (aisla32) para que
 * este proceso pueda seguir y comprobar que el sistema sobrevive.
 *
 *  A  el hijo intenta escribir en la memoria del kernel (0x100000)
 *  B  memoria privada: el hijo llena su buffer con 0x55 y el padre
 *     comprueba que el suyo sigue con 0xAA; ademas el hijo lee 0x7F0000,
 *     fuera de su espacio, y debe morir con #PF
 *  C  el hijo divide entre cero
 *  D  el hijo ejecuta una instruccion invalida
 *  E  un hijo muere mientras dos contadores siguen vivos
 *  F  crear y destruir 200 procesos mirando los marcos libres
 *  G  agotar el limite de procesos simultaneos
 *  H  ejecucion prolongada de cuatro contadores
 */

static int fallos = 0;
static int pruebas = 0;

static void ok(const char *nom, int cond, const char *detalle) {
    pruebas++;
    if (!cond) fallos++;
    u_print("\r\n[ok] detalle=");
    u_print_hex((unsigned)detalle);
    u_print(" byte0=");
    u_print_hex((unsigned)(detalle ? (unsigned char)detalle[0] : 0));
    u_print(" ");
    u_print("PRUEBA ");
    u_print(nom);
    u_print(": ");
    u_print(cond ? "OK" : "FALLO");
    if (detalle && detalle[0]) {
        u_print("(");
        u_print_hex((unsigned)detalle);
        u_print("=");
        u_print(detalle);
        u_print(")");
    }
    u_print("\r\n");
}

static int lanza(const char *modo) {
    return u_crear("aisla32", modo);
}

int main(void) {
    char det[24];
    unsigned int m0[5], m1[5];
    uproc_t p[12];

    u_print("\r\naislamiento32 v1.9 - pruebas de aislamiento y procesos\r\n");
    u_print("yo soy el proceso ");
    u_print_num(u_mi_pid());
    u_print("\r\n");

    /* A: escribir en memoria del kernel */
    {
        u_print("[A] codigo en memoria: ");
        for (int i = 0; i < 12; i++) {
            u_print_hex((unsigned)(unsigned char)*(volatile unsigned char *)
                        (0x200890 + i));
            u_print(" ");
        }
        u_print("\r\n");
        int pid = lanza("kernel");
        int rc = pid < 0 ? pid : u_esperar(pid);
        u_itoa(rc, det);
        ok("A", rc == -1, det);
    }

    /* B: memoria privada entre procesos */
    {
        static unsigned char mio[256];
        for (int i = 0; i < 256; i++) mio[i] = 0xAA;
        u_print("[B] lanzo privado\r\n");
        int p1 = lanza("privado");
        u_print("[B] espero privado\r\n");
        int rc = p1 < 0 ? p1 : u_esperar(p1);
        u_print("[B] rc recibido\r\n");
        int intacto = 1;
        for (int i = 0; i < 256; i++) if (mio[i] != 0xAA) intacto = 0;
        u_print("[B] lanzo fuera\r\n");
        int p2 = lanza("fuera");
        int rc2 = p2 < 0 ? p2 : u_esperar(p2);
        u_print("[B] fin\r\n");
        u_print("  privado rc=");
        u_print_num(rc);
        u_print(" fuera rc=");
        u_print_num(rc2);
        u_print(" mi buffer intacto=");
        u_print(intacto ? "si" : "no");
        u_print("\r\n");
        ok("B", intacto && rc == 7 && rc2 == -1, "");
    }

    /* C: division por cero */
    {
        int pid = lanza("cero");
        int rc = pid < 0 ? pid : u_esperar(pid);
        u_itoa(rc, det);
        ok("C", rc == -1, det);
    }

    /* D: instruccion invalida */
    {
        int pid = lanza("mala");
        int rc = pid < 0 ? pid : u_esperar(pid);
        u_itoa(rc, det);
        ok("D", rc == -1, det);
    }

    /* E: uno termina mientras otros siguen */
    {
        int a = u_crear("contador32", "A");
        int b = u_crear("contador32", "B");
        int c = lanza("cero");
        u_dormir(500);
        int n = u_listar(p, 12);
        int rc = c > 0 ? u_esperar(c) : -99;
        int rc_a = a > 0 ? u_esperar(a) : -99;
        int rc_b = b > 0 ? u_esperar(b) : -99;
        u_print("  c rc=");
        u_print_num(rc);
        u_print(" a rc=");
        u_print_num(rc_a);
        u_print(" b rc=");
        u_print_num(rc_b);
        u_print(" tareas=");
        u_print_num(n);
        u_print("\r\n");
        ok("E", rc == -1 && rc_a == 'A' && rc_b == 'B' && n >= 2, "");
    }

    /* F: crear y destruir 40 veces (cada hijo dura 0.2 s) */
    {
        u_info_memoria(m0);
        int ultimo = 0, err = 0;
        for (int i = 0; i < 40; i++) {
            int pid = u_crear("contador32", "F 2");
            if (pid < 0) { err = pid; break; }
            ultimo = u_esperar(pid);
            if (ultimo != 'F') { err = -2; break; }
        }
        u_info_memoria(m1);
        u_print("  40 procesos; marcos libres antes=");
        u_print_num((int)m0[1]);
        u_print(" despues=");
        u_print_num((int)m1[1]);
        u_print("\r\n");
        u_itoa(ultimo, det);
        ok("F", err == 0 && ultimo == 'F' && (int)m1[1] >= (int)m0[1] - 2, det);
    }

    /* G: limite de procesos simultaneos. Hay MAXPROC (8) huecos de
       usuario y uno lo ocupa este mismo proceso, asi que caben 7 hijos
       a la vez; el octavo intento debe fallar de forma controlada.
       Se duerme antes para que proc_fusiona drene los terminados
       pendientes (la liberacion es diferida). */
    {
        int pids[12];
        int n = 0, overflow = 0;
        u_dormir(500);
        for (int i = 0; i < 12; i++) {
            pids[i] = u_crear("dormir32", "");
            if (pids[i] < 0) { overflow = pids[i]; break; }
            n++;
        }
        u_print("  simultaneos=");
        u_print_num(n);
        u_print(" error=");
        u_print_num(overflow);
        u_print("\r\n");
        int todos = 1;
        for (int i = 0; i < n; i++) if (u_esperar(pids[i]) != 0) todos = 0;
        ok("G", n == 7 && overflow < 0 && todos, "");
    }

    /* H: ejecucion prolongada de cuatro procesos (con drenaje previo:
       los dormidos de G acaban de terminar y su liberacion es diferida). */
    {
        int pids[4];
        u_dormir(500);
        for (int i = 0; i < 4; i++) pids[i] = u_crear("contador32", "H");
        int ultimo = 0;
        for (int i = 0; i < 4; i++) if (pids[i] > 0) ultimo = u_esperar(pids[i]);
        u_print("  cuatro contadores, ultimo rc=");
        u_print_num(ultimo);
        u_print("\r\n");
        ok("H", ultimo == 'H', "");
    }

    u_print("RESUMEN aislamiento32: ");
    u_print_num(pruebas);
    u_print(" pruebas, ");
    u_print_num(fallos);
    u_print(" fallos\r\n");
    return fallos;
}
