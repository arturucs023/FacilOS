#include "ulib.h"

void __main(void) {
}

/* servidor (etapa 4, E2): servidor grafico minimo por leases.
 * Toma el framebuffer, registra 1 demo, compone un frame estatico y sale
 * con q/ESC. Sin raton aun (E4); teclado propio (la shell queda excluida
 * mientras corre, como el menu). */

#define NVENT 16
#define PANT_W 800
#define PANT_H 600

typedef struct {
    int usado;
    int pid;
    int w, h, x, y, z;
    int visible;
} ventana_t;

static ventana_t vents[NVENT];
static int nz = 0;

static void fondo(void) {
    u_destino(0);
    u_limpiar(C_NEGRO);
    u_rectangulo(0, 0, PANT_W, 24, C_AZUL);
    u_texto(8, 4, "FacilOS escritorio v0", C_BLANCO);
}

static void cromo(ventana_t *v) {
    u_rectangulo(v->x, v->y, v->w, v->h, C_GRIS);
    u_rectangulo(v->x, v->y, v->w, 20, C_AZUL);
    u_texto(v->x + 8, v->y + 2, "demo", C_AMARILLO);
}

static void asegura_propiedad(void) {
    u_texto(0, 0, "", C_NEGRO);
}

static int registra(int pid, int w, int h) {
    int i, z;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > PANT_W) w = PANT_W;
    if (h > PANT_H) h = PANT_H;
    for (i = 0; i < NVENT; i++) {
        if (!vents[i].usado) break;
    }
    if (i >= NVENT) return -1;
    z = nz++;
    vents[i].usado = 1;
    vents[i].pid = pid;
    vents[i].w = w;
    vents[i].h = h;
    vents[i].x = (PANT_W - w) / 2;
    vents[i].y = (PANT_H - h) / 2;
    vents[i].z = z;
    vents[i].visible = 1;
    return i;
}

static void libera(int i) {
    if (i < 0 || i >= NVENT) return;
    vents[i].usado = 0;
    vents[i].visible = 0;
}

/* Recompone: fondo + chrome de cada ventana + DRAW a las sucias. */
static void frame(void) {
    int o, k;
    asegura_propiedad();
    fondo();
    /* orden Z ascendente (abajo primero) */
    for (o = 0; o < nz + 4; o++) {
        for (k = 0; k < NVENT; k++) {
            ventana_t *v = &vents[k];
            char m[32];
            if (!v->usado || !v->visible || v->z != o) continue;
            cromo(v);
            m[0] = 'D';
            m[1] = 'R';
            m[2] = 'A';
            m[3] = 'W';
            m[4] = ' ';
            {
                char nb[12];
                int j = 0, neg = 0, val;
                val = v->x;
                if (val < 0) { neg = 1; val = -val; }
                if (!val) nb[j++] = '0';
                else {
                    char rv[12];
                    int ri = 0;
                    while (val && ri < 11) {
                        rv[ri++] = (char)('0' + val % 10);
                        val /= 10;
                    }
                    while (ri) nb[j++] = rv[--ri];
                }
                nb[j] = 0;
                {
                    int t = 5, q = 0;
                    if (neg) m[t++] = '-';
                    while (nb[q]) m[t++] = nb[q++];
                    m[t++] = ' ';
                    val = v->y;
                    neg = 0;
                    j = 0;
                    if (val < 0) { neg = 1; val = -val; }
                    if (!val) nb[j++] = '0';
                    else {
                        char rv[12];
                        int ri = 0;
                        while (val && ri < 11) {
                            rv[ri++] = (char)('0' + val % 10);
                            val /= 10;
                        }
                        while (ri) nb[j++] = rv[--ri];
                    }
                    nb[j] = 0;
                    q = 0;
                    if (neg) m[t++] = '-';
                    while (nb[q] && t < 30) m[t++] = nb[q++];
                    m[t] = 0;
                }
            }
            u_ipc_enviar(v->pid, m, u_strlen(m) + 1);
        }
    }
}

int main(void) {
    char b[64];
    int de = 0;
    char arg[16];
    int demo = -1;
    u_print("servidor grafico v0 (etapa 4)\n");
    asegura_propiedad();
    u_itoa(u_mi_pid(), arg);
    demo = u_crear("wdemo", arg);
    if (demo <= 0) {
        u_print("sin demo\n");
        return 1;
    }
    frame();
    for (;;) {
        if (u_ipc_hay() > 0) {
            int r = u_ipc_recibir(b, 64, &de);
            if (r < 0) continue;
            if (r >= 3 && b[0] == 'R' && b[1] == 'E' && b[2] == 'G') {
                int k = 4, w = 0, h = 0, id;
                char idm[16];
                while (k < r && (b[k] < '0' || b[k] > '9')) k++;
                w = u_atoi(b + k);
                while (k < r && b[k] >= '0' && b[k] <= '9') k++;
                while (k < r && (b[k] < '0' || b[k] > '9')) k++;
                h = u_atoi(b + k);
                id = registra(de, w, h);
                idm[0] = 'I';
                idm[1] = 'D';
                idm[2] = ' ';
                {
                    int t = 3;
                    char nb[12];
                    int j = 0, v = id >= 0 ? id : 0;
                    if (!v) nb[j++] = '0';
                    else {
                        char rv[12];
                        int ri = 0;
                        while (v && ri < 11) {
                            rv[ri++] = (char)('0' + v % 10);
                            v /= 10;
                        }
                        while (ri) nb[j++] = rv[--ri];
                    }
                    nb[j] = 0;
                    j = 0;
                    while (nb[j] && t < 15) idm[t++] = nb[j++];
                    idm[t] = 0;
                }
                u_ipc_enviar(de, idm, u_strlen(idm) + 1);
                frame();
            } else if (r >= 3 && b[0] == 'B' && b[1] == 'Y' && b[2] == 'E') {
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de) libera(k);
                }
                frame();
            }
            /* ACK y otros se consumen sin mas (ritmo sin esperas) */
        } else if (u_hay_tecla()) {
            int k = u_tecla();
            if (k == 'q' || k == 'Q' || k == 27) {
                int i;
                for (i = 0; i < NVENT; i++) {
                    if (!vents[i].usado) continue;
                    u_ipc_enviar(vents[i].pid, "QUIT", 5);
                }
                for (i = 0; i < NVENT; i++) {
                    if (!vents[i].usado) continue;
                    u_esperar(vents[i].pid);
                    vents[i].usado = 0;
                }
                u_print("servidor terminado\n");
                return 0;
            }
        } else {
            u_dormir(20);
        }
    }
}