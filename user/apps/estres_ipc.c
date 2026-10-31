#include "ulib.h"

void __main(void) {
}

/* estres_ipc (etapa 2, fase 8): IPC bajo presion, todo acotado.
 * Roles (mismo binario): "ecoT" (eco tolerante, cuenta y sale con "fin"),
 * "lluvia <n> <dst>" (envia n etiquetados a dst, recoge sus ecos, rc=fallos).
 * Sin args: director. Ninguna espera es infinita: cada bloqueo lo rompe un
 * envio garantizado del protocolo; esperar() solo sobre hijos que terminan
 * por construccion. */

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

static int rol_eco(void) {
    char b[64];
    int de = 0, n = 0;
    for (;;) {
        int r = u_ipc_recibir(b, 64, &de);
        if (r < 0) return 90 + n;
        if (r >= 3 && b[0] == 'f' && b[1] == 'i' && b[2] == 'n') return n;
        /* eco tolerante: el destino puede haber muerto; se cuenta igual */
        u_ipc_enviar(de, b, r);
        n++;
    }
}

static int rol_lluvia(const char *a) {
    char b[64];
    int de = 0, n, dst, i, mal = 0;
    while (*a == ' ') a++;
    n = u_atoi(a);
    while (*a && *a != ' ') a++;
    while (*a == ' ') a++;
    dst = u_atoi(a);
    if (n < 1) n = 1;
    if (n > 60) n = 60;
    if (dst <= 0) return 70;
    for (i = 0; i < n; i++) {
        char m[16];
        int r, j = 0;
        m[j++] = 'L';
        m[j++] = (char)('0' + (u_mi_pid() % 10));
        m[j++] = (char)('0' + (i / 10) % 10);
        m[j++] = (char)('0' + i % 10);
        if (u_ipc_enviar(dst, m, 5)) return 71;
        r = u_ipc_recibir(b, 64, &de);
        if (r != 5 || de != dst) return 72;
        if (b[0] != 'L' || b[1] != m[1] || b[2] != m[2] || b[3] != m[3])
            mal++;
    }
    return mal;
}

static int rol_muerete(const char *a) {
    int dst, i;
    while (*a == ' ') a++;
    dst = u_atoi(a);
    if (dst <= 0) return 70;
    for (i = 0; i < 5; i++) {
        if (u_ipc_enviar(dst, "M", 2)) return 73;
    }
    {
        volatile unsigned char *q = (volatile unsigned char *)0x100000;
        *q = 1;
    }
    return 74;
}

int main(void) {
    const char *a = u_args();
    unsigned int w0[8], w[8];
    while (*a == ' ') a++;
    if (!u_strncmp(a, "ecoT", 4)) return rol_eco();
    if (!u_strncmp(a, "lluvia", 6)) return rol_lluvia(a + 6);
    if (!u_strncmp(a, "muerete", 7)) return rol_muerete(a + 7);
    u_print("estres_ipc etapa2/fase8\n");
    ok(u_info_memoria2(w0) == 0, "info2-ok");
    /* P1: 200 ping-pong con eco verificado */
    {
        char b[64];
        int de = 0, i, h = u_crear("estres_ipc", "ecoT");
        int t0, dt;
        ok(h > 0, "p1-crear-eco");
        if (h <= 0) return 99;
        t0 = u_ticks();
        for (i = 0; i < 200; i++) {
            char m[8];
            int r;
            m[0] = 'Q';
            m[1] = (char)('0' + (i / 10) % 10);
            m[2] = (char)('0' + i % 10);
            if (u_ipc_enviar(h, m, 4)) { ok(0, "p1-enviar"); break; }
            r = u_ipc_recibir(b, 64, &de);
            if (r != 4 || de != h || b[0] != 'Q' || b[1] != m[1] ||
                b[2] != m[2]) {
                ok(0, "p1-eco");
                break;
            }
        }
        dt = u_ticks() - t0;
        ok(i == 200, "p1-doscientos");
        u_print("200 idas en ");
        u_print_num(dt);
        u_print(" ticks\n");
        ok(u_ipc_enviar(h, "fin", 4) == 0, "p1-fin");
        ok(u_esperar(h) == 200, "p1-eco-200");
    }
    /* P2: 3 emisores x 30 al mismo eco */
    {
        int e, w1, w2, w3, r1, r2, r3;
        char ar[32];
        e = u_crear("estres_ipc", "ecoT");
        ok(e > 0, "p2-crear-eco");
        if (e <= 0) return 99;
        u_itoa(e, ar + 7);
        {
            char a1[32], a2[32], a3[32];
            u_strcpy(a1, "lluvia 30 ");
            u_strcat(a1, ar + 7);
            u_strcpy(a2, a1);
            u_strcpy(a3, a1);
            w1 = u_crear("estres_ipc", a1);
            w2 = u_crear("estres_ipc", a2);
            w3 = u_crear("estres_ipc", a3);
        }
        ok(w1 > 0 && w2 > 0 && w3 > 0, "p2-crear-lluvias");
        r1 = w1 > 0 ? u_esperar(w1) : -99;
        r2 = w2 > 0 ? u_esperar(w2) : -99;
        r3 = w3 > 0 ? u_esperar(w3) : -99;
        ok(r1 == 0 && r2 == 0 && r3 == 0, "p2-lluvias-limpias");
        ok(u_ipc_enviar(e, "fin", 4) == 0, "p2-fin");
        ok(u_esperar(e) == 90, "p2-eco-90");
    }
    /* P3: emisor muere tras 5; eco sigue y cuenta */
    {
        int e = u_crear("estres_ipc", "ecoT");
        int m, rc;
        char ar[16];
        ok(e > 0, "p3-crear-eco");
        if (e <= 0) return 99;
        u_itoa(e, ar);
        {
            char aa[32];
            u_strcpy(aa, "muerete ");
            u_strcat(aa, ar);
            m = u_crear("estres_ipc", aa);
        }
        ok(m > 0, "p3-crear-muerete");
        rc = m > 0 ? u_esperar(m) : -99;
        ok(rc == -1, "p3-muerete-muere");
        for (rc = 0; rc < 10; rc++) {
            if (u_ipc_enviar(e, "P", 2)) break;
        }
        ok(rc == 10, "p3-padres");
        ok(u_ipc_enviar(e, "fin", 4) == 0, "p3-fin");
        ok(u_esperar(e) == 15, "p3-eco-15");
    }
    /* P4: orden FIFO con 16 rapidos. Antes se drenan los 10 ecos rancios
       de P3 y se cede tras la rafaga para que el eco vacie (sin ceder,
       el eco no corre y "fin" no cabe: protocolo, no kernel). */
    {
        char b[64];
        int de = 0, i, h = u_crear("estres_ipc", "ecoT");
        ok(h > 0, "p4-crear-eco");
        if (h <= 0) return 99;
        for (i = 0; i < 10; i++) {
            int r = u_ipc_recibir(b, 64, &de);
            if (r != 2 || b[0] != 'P') break;
        }
        ok(i == 10, "p4-drena-rancios");
        for (i = 0; i < 16; i++) {
            char m[8];
            m[0] = 'F';
            m[1] = (char)('0' + i % 10);
            if (u_ipc_enviar(h, m, 3)) break;
        }
        ok(i == 16, "p4-lleno-16");
        u_ceder();
        u_ceder();
        for (i = 0; i < 16; i++) {
            int r = u_ipc_recibir(b, 64, &de);
            if (r != 3 || de != h || b[0] != 'F' ||
                b[1] != (char)('0' + i % 10))
                break;
        }
        ok(i == 16, "p4-orden");
        ok(u_ipc_enviar(h, "fin", 4) == 0, "p4-fin");
        ok(u_esperar(h) == 16, "p4-eco-16");
    }
    /* P5: 1000 idas y vueltas */
    {
        char b[64];
        int de = 0, i, h = u_crear("estres_ipc", "ecoT");
        int t0, dt;
        ok(h > 0, "p5-crear-eco");
        if (h <= 0) return 99;
        t0 = u_ticks();
        for (i = 0; i < 1000; i++) {
            int r;
            if (u_ipc_enviar(h, "Z", 2)) break;
            r = u_ipc_recibir(b, 64, &de);
            if (r != 2 || de != h || b[0] != 'Z') break;
        }
        dt = u_ticks() - t0;
        ok(i == 1000, "p5-mil");
        u_print("1000 idas en ");
        u_print_num(dt);
        u_print(" ticks\n");
        ok(u_ipc_enviar(h, "fin", 4) == 0, "p5-fin");
        ok(u_esperar(h) == 1000, "p5-eco-1000");
    }
    /* P6: recursos tras todo (drenaje verificado) */
    {
        int i, okb = 0;
        for (i = 0; i < 10; i++) {
            if (u_info_memoria2(w)) break;
            if ((int)w[0] == (int)w0[0] && w[4] == 0 && w[5] == 0) {
                okb = 1;
                break;
            }
            u_dormir(100);
        }
        ok(okb, "p6-balance");
    }
    u_printf("estres_ipc: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos;
}