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
    int foco;
} ventana_t;

static ventana_t vents[NVENT];
static int nz = 0;
static int foco_id = -1;

/* Si durante la espera de ACK llega un REG, su ventana entra en la
   siguiente pasada de frame (si no, no se dibujaria hasta la proxima
   tecla). */
static int nuevo_marco = 0;

static void fondo(void) {
    u_destino(0);
    u_limpiar(C_NEGRO);
    u_rectangulo(0, 0, PANT_W, 24, C_AZUL);
    u_texto(8, 4, "FacilOS escritorio v0", C_BLANCO);
}

static void cromo(ventana_t *v) {
    u_rectangulo(v->x, v->y, v->w, v->h, C_GRIS);
    u_rectangulo(v->x, v->y, v->w, 20, v->foco ? C_ROJO : C_AZUL);
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
    /* cascada por slot para que varias ventanas no se solapen del todo */
    vents[i].x = (PANT_W - w) / 2 + (i * 60) % 320;
    vents[i].y = (PANT_H - h) / 2 + (i * 40) % 200;
    if (vents[i].x + w > PANT_W) vents[i].x = PANT_W - w;
    if (vents[i].y + h > PANT_H) vents[i].y = PANT_H - h;
    vents[i].z = z;
    vents[i].visible = 1;
    vents[i].foco = 0;
    return i;
}

static void libera(int i) {
    if (i < 0 || i >= NVENT) return;
    vents[i].usado = 0;
    vents[i].visible = 0;
    if (foco_id == i) foco_id = -1;
}

/* Pone el foco (redibuja chrome). -1 = ninguno. */
static void fija_foco(int id) {
    int i;
    for (i = 0; i < NVENT; i++) vents[i].foco = 0;
    foco_id = -1;
    if (id >= 0 && id < NVENT && vents[id].usado) {
        vents[id].foco = 1;
        foco_id = id;
    }
}

static int siguiente_foco(void) {
    int i, ini = (foco_id + 1) % NVENT;
    for (i = 0; i < NVENT; i++) {
        int k = (ini + i) % NVENT;
        if (vents[k].usado && vents[k].visible) return k;
    }
    return -1;
}

/* Atiende un REG en cualquier contexto (bucle o espera de ACK): registra
   y responde ID sin dibujar (la ventana entra en el proximo frame). */
static void atiende_reg(int de, const char *b, int r) {
    int k = 4, w = 0, h = 0, id;
    char idm[16];
    while (k < r && (b[k] < '0' || b[k] > '9')) k++;
    w = u_atoi(b + k);
    while (k < r && b[k] >= '0' && b[k] <= '9') k++;
    while (k < r && (b[k] < '0' || b[k] > '9')) k++;
    h = u_atoi(b + k);
    id = registra(de, w, h);
    u_print("reg pid=");
    u_print_num(de);
    u_print(" id=");
    u_print_num(id);
    u_print("\n");
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
    fija_foco(id);
    nuevo_marco = 1;
}

/* Recompone: fondo + (por ventana de abajo arriba: chrome + DRAW con
   espera de ACK). La espera es con deteccion de muerte (stat FIN): sin
   ella, el contenido tardio de una ventana baja taparia el chrome de las
   altas y un cliente muerto colgaria al servidor. */
static void espera_ack(int pid) {
    char b[64];
    int de = 0;
    for (;;) {
        if (u_ipc_hay() > 0) {
            int r = u_ipc_recibir(b, 64, &de);
            if (r < 0) continue;
            if (de == pid && r >= 3 && b[0] == 'A' && b[1] == 'C' &&
                b[2] == 'K')
                return;
            if (r >= 3 && b[0] == 'B' && b[1] == 'Y' && b[2] == 'E') {
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == de) libera(k);
                }
                continue;
            }
            /* REG u otros durante la espera: REG se atiende aqui mismo
               (registra+ID); el resto se consume. */
            if (r >= 3 && b[0] == 'R' && b[1] == 'E' && b[2] == 'G') {
                atiende_reg(de, b, r);
                continue;
            }
            continue;
        }
        {
            unsigned int st[5];
            if (u_proc_stat(pid, st) != 0 || st[0] == 4) {
                int k;
                for (k = 0; k < NVENT; k++) {
                    if (vents[k].usado && vents[k].pid == pid) libera(k);
                }
                return;
            }
        }
        u_dormir(10);
    }
}

static void frame(void) {
    int ids[NVENT], nn = 0, o, k, q;
    /* instantanea ordenada por z */
    for (o = 0; o < nz + 4; o++) {
        for (k = 0; k < NVENT; k++) {
            if (vents[k].usado && vents[k].visible && vents[k].z == o)
                ids[nn++] = k;
        }
    }
    asegura_propiedad();
    fondo();
    for (q = 0; q < nn; q++) {
        ventana_t *v = &vents[ids[q]];
        char m[32];
        int t = 0;
        if (!v->usado || !v->visible) continue;
        asegura_propiedad();
        cromo(v);
        m[t++] = 'D';
        m[t++] = 'R';
        m[t++] = 'A';
        m[t++] = 'W';
        m[t++] = ' ';
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
            espera_ack(v->pid);
        }
    if (nuevo_marco) {
        nuevo_marco = 0;
        frame();
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
    {
        int demo2 = u_crear("wdemo", arg);
        (void)demo2;
    }
    frame();
    fija_foco(siguiente_foco());
    frame();
    for (;;) {
        if (u_ipc_hay() > 0) {
            int r = u_ipc_recibir(b, 64, &de);
            if (r < 0) continue;
            if (r >= 3 && b[0] == 'R' && b[1] == 'E' && b[2] == 'G') {
                atiende_reg(de, b, r);
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
            } else if (k == 'f' || k == 'F') {
                fija_foco(siguiente_foco());
                frame();
            } else if (k == 'x' || k == 'X') {
                if (foco_id >= 0) {
                    u_ipc_enviar(vents[foco_id].pid, "QUIT", 5);
                    u_esperar(vents[foco_id].pid);
                    libera(foco_id);
                    fija_foco(siguiente_foco());
                    frame();
                }
            } else if (k == TEC_ARRIBA_U || k == TEC_ABAJO_U ||
                       k == TEC_IZQ_U || k == TEC_DER_U) {
                if (foco_id >= 0) {
                    ventana_t *v = &vents[foco_id];
                    if (k == TEC_ARRIBA_U && v->y >= 16) v->y -= 16;
                    if (k == TEC_ABAJO_U &&
                        v->y + v->h + 16 <= PANT_H)
                        v->y += 16;
                    if (k == TEC_IZQ_U && v->x >= 16) v->x -= 16;
                    if (k == TEC_DER_U &&
                        v->x + v->w + 16 <= PANT_W)
                        v->x += 16;
                    frame();
                }
            } else if (foco_id >= 0 && k >= 32 && k < 127) {
                /* tecla normal -> ventana con foco */
                char m[16];
                m[0] = 'K';
                m[1] = 'E';
                m[2] = 'Y';
                m[3] = ' ';
                {
                    int t = 4, v = k, j = 0;
                    char nb[12];
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
                    while (nb[j] && t < 15) m[t++] = nb[j++];
                    m[t] = 0;
                }
                u_ipc_enviar(vents[foco_id].pid, m, u_strlen(m) + 1);
                frame();
            }
        } else {
            u_dormir(20);
        }
    }
}