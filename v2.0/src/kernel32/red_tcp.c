#include "red_tcp.h"
#include "red.h"
#include "red_ip.h"
#include "red_arp.h"
#include "drv_red.h"
#include "libc.h"
#include "irq.h"
#include "proc.h"

/* v2.3-red F5: TCP propio. Stop-and-wait por segmento (un solo
   segmento sin ACK a la vez): simple, correcto y suficiente en
   LAN. Sin opciones, sin reensamblado, sin 2MSL (documentado). */

#define RTO0 50          /* 500 ms */
#define RTO_MAX 400      /* 4 s */
#define NREINT 4         /* reintentos (5 envios en total) */
#define CONEX_ESP 1000   /* tope conectar (ticks): ~10 s */
#define CIERRE_ESP 500   /* tope cierre (ticks): ~5 s */

/* Direcciones del KAT (sinteticas; sin trafico al cable). */
#define KAT_PAR 0x6302000Au   /* 10.0.2.99 */
#define KAT_MIO 0x0F02000Au   /* 10.0.2.15 */

typedef struct {
    uint8_t estado;
    uint16_t gen;      /* id = gen*TCP_N + idx (nunca se recicla) */
    uint32_t pid;
    int err;           /* sticky al morir (-7/-9); lo recoge cerrar */
    uint32_t ip_rem;
    uint16_t pto_loc, pto_rem;
    uint32_t iss, irs, snd_nxt, snd_una, rcv_nxt;
    uint16_t wnd_par;
    uint8_t snd[TCP_MSS];
    uint16_t snd_len;  /* pendiente de ACK (0 = libre) */
    uint8_t rcv[TCP_RCV];
    uint16_t rcv_ini, rcv_n;
    uint8_t fin_rx;    /* FIN ajeno visto */
    uint8_t fin_ack;   /* ACK de nuestro FIN visto */
    uint8_t reint;
    uint32_t t0;       /* ultimo envio con ACK pendiente */
    uint32_t rto;
    uint32_t uso;
    int hijo_de;       /* escucha padre (SYN_RX) o -1 */
    int pendiente;     /* hijo pendiente (escucha) o -1 */
} tcp_t;

static tcp_t tcbs[TCP_N];
static uint16_t gen_seq = 1;
static uint32_t c_ok = 0, c_err = 0;

/* Captura de Tx para el KAT (cero tramas al cable). */
static int kat_on = 0;
typedef struct {
    uint8_t band;
    uint16_t dp;
    uint32_t seq, ackn;
    uint8_t nd;
} katreg_t;
static katreg_t kat_r[32];
static int kat_n = 0;

void red_tcp_init(void) {
    int i;
    for (i = 0; i < TCP_N; i++) {
        tcbs[i].estado = TCP_CERRADO;
        tcbs[i].gen = 0;
        tcbs[i].pid = 0;
        tcbs[i].err = 0;
        tcbs[i].pendiente = -1;
        tcbs[i].hijo_de = -1;
    }
    gen_seq = 1;
    kat_on = 0;
    kat_n = 0;
}

/* Suma TCP con pseudo-cabecera (protocolo 6). */
static uint16_t cks_tcp(const uint8_t *src4, const uint8_t *dst4,
                        const uint8_t *t, int n) {
    uint32_t s = 0;
    int i;
    s += ((uint32_t)src4[0] << 8) | src4[1];
    s += ((uint32_t)src4[2] << 8) | src4[3];
    s += ((uint32_t)dst4[0] << 8) | dst4[1];
    s += ((uint32_t)dst4[2] << 8) | dst4[3];
    s += 6u;
    s += (uint32_t)n;
    for (i = 0; i + 1 < n; i += 2)
        s += ((uint32_t)t[i] << 8) | t[i + 1];
    if (i < n) s += (uint32_t)t[i] << 8;
    while (s >> 16) s = (s & 0xFFFFu) + (s >> 16);
    return (uint16_t)(~s & 0xFFFFu);
}

static int idx_de_id(int id, uint32_t pid, tcp_t **pt) {
    int i;
    tcp_t *t;
    if (id <= 0) return -2;
    i = id % TCP_N;
    t = &tcbs[i];
    if ((int)(t->gen * TCP_N + (uint32_t)i) != id) return -2;
    if (!t->pid || t->pid != pid) return -6;
    if (pt) *pt = t;
    if (t->estado == TCP_CERRADO) return t->err ? t->err : -2;
    return 0;
}

static tcp_t *nuevo_tcb(uint32_t pid) {
    int i;
    for (i = 0; i < TCP_N; i++) {
        if (tcbs[i].estado == TCP_CERRADO && !tcbs[i].err) {
            tcp_t *t = &tcbs[i];
            t->gen = gen_seq++;
            if (!gen_seq) gen_seq = 1;
            if (!t->gen) t->gen = gen_seq++;
            t->pid = pid;
            t->err = 0;
            t->snd_len = 0;
            t->rcv_ini = 0;
            t->rcv_n = 0;
            t->fin_rx = 0;
            t->fin_ack = 0;
            t->reint = 0;
            t->wnd_par = 0;
            t->hijo_de = -1;
            t->pendiente = -1;
            t->uso = irq_ticks();
            return t;
        }
    }
    return 0;
}

static int id_de(tcp_t *t) {
    return (int)(t->gen * TCP_N + (uint32_t)(t - tcbs));
}

static void libera(tcp_t *t) {
    t->estado = TCP_CERRADO;
    t->pid = 0;
    t->err = 0;
    t->pendiente = -1;
    t->hijo_de = -1;
    t->gen = gen_seq++;
    if (!gen_seq) gen_seq = 1;
}

/* Muere con codigo sticky (lo recoge cerrar/recibir/enviar). */
static void muere(tcp_t *t, int e) {
    t->estado = TCP_CERRADO;
    t->err = e;
}

static uint16_t wnd_loc(tcp_t *t) {
    uint32_t libre = TCP_RCV - t->rcv_n;
    return (uint16_t)(libre > 0xFFFFu ? 0xFFFFu : libre);
}

/* Salida: KAT captura, normal red_ip_enviar (proto 6). */
static int tcp_salida(uint32_t dst, const uint8_t *seg, int n) {
    if (kat_on) {
        if (kat_n < 32) {
            kat_r[kat_n].band = seg[13];
            kat_r[kat_n].dp =
                (uint16_t)((seg[2] << 8) | seg[3]);
            kat_r[kat_n].seq =
                ((uint32_t)seg[4] << 24) | ((uint32_t)seg[5] << 16) |
                ((uint32_t)seg[6] << 8) | seg[7];
            kat_r[kat_n].ackn =
                ((uint32_t)seg[8] << 24) | ((uint32_t)seg[9] << 16) |
                ((uint32_t)seg[10] << 8) | seg[11];
            kat_r[kat_n].nd = (uint8_t)(n - 20 > 255 ? 255 : n - 20);
            kat_n++;
        }
        return n;
    }
    return red_ip_enviar(dst, 6, seg, n, 64);
}

/* Construye y envia segmento (sin opciones, 20 B). seq explicita;
   ack = rcv_nxt si band lleva ACK. */
static int tcp_tx(tcp_t *t, uint8_t band, const uint8_t *d, int n,
                  uint32_t seq) {
    uint8_t s[20 + TCP_MSS];
    uint8_t seudo[8];
    uint32_t mio;
    int i;
    if (n < 0 || n > TCP_MSS) return -2;
    mio = red_cfg_ip();
    if (!mio && !kat_on) return -2;
    s[0] = (uint8_t)((t->pto_loc >> 8) & 0xFFu);
    s[1] = (uint8_t)(t->pto_loc & 0xFFu);
    s[2] = (uint8_t)((t->pto_rem >> 8) & 0xFFu);
    s[3] = (uint8_t)(t->pto_rem & 0xFFu);
    s[4] = (uint8_t)((seq >> 24) & 0xFFu);
    s[5] = (uint8_t)((seq >> 16) & 0xFFu);
    s[6] = (uint8_t)((seq >> 8) & 0xFFu);
    s[7] = (uint8_t)(seq & 0xFFu);
    {
        uint32_t a = (band & 0x10) ? t->rcv_nxt : 0;
        s[8] = (uint8_t)((a >> 24) & 0xFFu);
        s[9] = (uint8_t)((a >> 16) & 0xFFu);
        s[10] = (uint8_t)((a >> 8) & 0xFFu);
        s[11] = (uint8_t)(a & 0xFFu);
    }
    s[12] = 0x50;
    s[13] = band;
    {
        uint16_t w = wnd_loc(t);
        s[14] = (uint8_t)((w >> 8) & 0xFFu);
        s[15] = (uint8_t)(w & 0xFFu);
    }
    s[16] = 0;
    s[17] = 0;
    s[18] = 0;
    s[19] = 0;
    for (i = 0; i < n; i++) s[20 + i] = d[i];
    seudo[0] = (uint8_t)(mio & 0xFFu);
    seudo[1] = (uint8_t)((mio >> 8) & 0xFFu);
    seudo[2] = (uint8_t)((mio >> 16) & 0xFFu);
    seudo[3] = (uint8_t)((mio >> 24) & 0xFFu);
    seudo[4] = (uint8_t)(t->ip_rem & 0xFFu);
    seudo[5] = (uint8_t)((t->ip_rem >> 8) & 0xFFu);
    seudo[6] = (uint8_t)((t->ip_rem >> 16) & 0xFFu);
    seudo[7] = (uint8_t)((t->ip_rem >> 24) & 0xFFu);
    {
        uint16_t c = cks_tcp(seudo, seudo + 4, s, 20 + n);
        s[16] = (uint8_t)((c >> 8) & 0xFFu);   /* TCP: tal cual */
        s[17] = (uint8_t)(c & 0xFFu);
    }
    {
        int k = tcp_salida(t->ip_rem, s, 20 + n);
        if (k < 0) return k;
    }
    t->t0 = irq_ticks();
    return 20 + n;
}

/* RST a (dst,dp,sp) con seq y ack dados. */
static void rst_para(uint32_t dst, uint16_t dp, uint16_t sp,
                     uint32_t seq, uint32_t ackn) {
    uint8_t s[20];
    uint8_t seudo[8];
    uint32_t mio = red_cfg_ip();
    if (!mio) {
        if (!kat_on) return;
        mio = KAT_MIO;   /* KAT: sin cable igualmente (captura) */
    }
    s[0] = (uint8_t)((sp >> 8) & 0xFFu);
    s[1] = (uint8_t)(sp & 0xFFu);
    s[2] = (uint8_t)((dp >> 8) & 0xFFu);
    s[3] = (uint8_t)(dp & 0xFFu);
    s[4] = (uint8_t)((seq >> 24) & 0xFFu);
    s[5] = (uint8_t)((seq >> 16) & 0xFFu);
    s[6] = (uint8_t)((seq >> 8) & 0xFFu);
    s[7] = (uint8_t)(seq & 0xFFu);
    s[8] = (uint8_t)((ackn >> 24) & 0xFFu);
    s[9] = (uint8_t)((ackn >> 16) & 0xFFu);
    s[10] = (uint8_t)((ackn >> 8) & 0xFFu);
    s[11] = (uint8_t)(ackn & 0xFFu);
    s[12] = 0x50;
    s[13] = 0x14;   /* RST+ACK */
    s[14] = 0;
    s[15] = 0;
    s[16] = 0;
    s[17] = 0;
    s[18] = 0;
    s[19] = 0;
    seudo[0] = (uint8_t)(mio & 0xFFu);
    seudo[1] = (uint8_t)((mio >> 8) & 0xFFu);
    seudo[2] = (uint8_t)((mio >> 16) & 0xFFu);
    seudo[3] = (uint8_t)((mio >> 24) & 0xFFu);
    seudo[4] = (uint8_t)(dst & 0xFFu);
    seudo[5] = (uint8_t)((dst >> 8) & 0xFFu);
    seudo[6] = (uint8_t)((dst >> 16) & 0xFFu);
    seudo[7] = (uint8_t)((dst >> 24) & 0xFFu);
    {
        uint16_t c = cks_tcp(seudo, seudo + 4, s, 20);
        s[16] = (uint8_t)((c >> 8) & 0xFFu);
        s[17] = (uint8_t)(c & 0xFFu);
    }
    tcp_salida(dst, s, 20);
}

/* Reenvia lo pendiente (SYN / SYN-ACK / datos / FIN). */
static void reenvia(tcp_t *t) {
    if (t->estado == TCP_SYN_TX) {
        tcp_tx(t, 0x02, 0, 0, t->iss);
    } else if (t->estado == TCP_SYN_RX) {
        tcp_tx(t, 0x12, 0, 0, t->iss);
    } else if (t->snd_len) {
        tcp_tx(t, 0x18, t->snd, t->snd_len, t->snd_una);
    } else if (t->estado == TCP_FIN_TX) {
        tcp_tx(t, 0x11, 0, 0, t->snd_nxt);
    }
}

/* Reloj: RTO con backoff y agotamiento. Llamar en cada bombeo. */
static void tcp_reloj(void) {
    int i;
    uint32_t ahora = irq_ticks();
    for (i = 0; i < TCP_N; i++) {
        tcp_t *t = &tcbs[i];
        int pendiente;
        if (t->estado == TCP_CERRADO || t->estado == TCP_ESCUCHA ||
            t->estado == TCP_ESTABLECIDO || t->estado == TCP_FIN_RX)
            pendiente = t->snd_len > 0;
        else
            pendiente = 1;
        if (!pendiente) continue;
        if ((int32_t)(ahora - t->t0) < (int32_t)t->rto) continue;
        if (t->reint >= NREINT) {
            if (t->hijo_de >= 0) {
                int p = t->hijo_de;
                if (p >= 0 && p < TCP_N &&
                    tcbs[p].pendiente == i)
                    tcbs[p].pendiente = -1;
                libera(t);
            } else if (t->estado == TCP_SYN_TX) {
                muere(t, -7);
            } else {
                muere(t, -7);
            }
            continue;
        }
        t->reint++;
        t->rto *= 2;
        if (t->rto > RTO_MAX) t->rto = RTO_MAX;
        reenvia(t);
    }
}

void red_tcp_entrada(const uint8_t *m, int tam) {
    int hlen, thl, n, tot;
    const uint8_t *ip, *t;
    uint16_t sp, dp;
    uint32_t seq, ackn;
    uint8_t band;
    int i;
    tcp_t *c = 0;
    if (tam < 14 + 20 + 20) {
        c_err++;
        return;
    }
    ip = m + 14;
    hlen = (ip[0] & 0x0F) * 4;
    tot = (ip[2] << 8) | ip[3];
    /* Longitud TCP real = total IP - cabecera IP (el marco puede
       traer relleno Ethernet: no debe entrar en la pseudo-cabecera
       ni contarse como datos). */
    if (hlen < 20 || tot < hlen + 20 || 14 + tot > tam) {
        c_err++;
        return;
    }
    n = tot - hlen;
    t = m + 14 + hlen;
    thl = (t[12] >> 4) * 4;
    if (thl < 20 || thl > n) {
        c_err++;
        return;
    }
    {
        uint8_t seudo[12];
        uint32_t s;
        int k;
        for (k = 0; k < 4; k++) {
            seudo[k] = ip[12 + k];
            seudo[4 + k] = ip[16 + k];
        }
        seudo[8] = 0;
        seudo[9] = 6;
        seudo[10] = (uint8_t)(((n) >> 8) & 0xFFu);
        seudo[11] = (uint8_t)((n) & 0xFFu);
        s = red_ip_cksum(seudo, 12);
        s += red_ip_cksum(t, n);
        while (s >> 16) s = (s & 0xFFFFu) + (s >> 16);
        if ((uint16_t)(~s & 0xFFFFu) != 0) {
            c_err++;
            return;
        }
    }
    sp = (uint16_t)((t[0] << 8) | t[1]);
    dp = (uint16_t)((t[2] << 8) | t[3]);
    seq = ((uint32_t)t[4] << 24) | ((uint32_t)t[5] << 16) |
          ((uint32_t)t[6] << 8) | t[7];
    ackn = ((uint32_t)t[8] << 24) | ((uint32_t)t[9] << 16) |
           ((uint32_t)t[10] << 8) | t[11];
    band = t[13];
    tcp_reloj();
    {
        uint32_t src = (uint32_t)ip[12] | ((uint32_t)ip[13] << 8) |
                       ((uint32_t)ip[14] << 16) | ((uint32_t)ip[15] << 24);
        /* Tupla exacta primero (conexion viva manda). */
        for (i = 0; i < TCP_N; i++) {
            tcp_t *q = &tcbs[i];
            if (q->estado != TCP_CERRADO && q->pto_loc == dp &&
                q->pto_rem == sp && q->ip_rem == src) {
                c = q;
                break;
            }
        }
        if (!c && (band & 0x04)) return;   /* RST sin dueno */
        if (!c) {
            /* Escucha: solo SYN pelado, con hijo libre. */
            for (i = 0; i < TCP_N; i++) {
                tcp_t *q = &tcbs[i];
                if (q->estado == TCP_ESCUCHA && q->pto_loc == dp &&
                    q->pendiente < 0) {
                    tcp_t *h = nuevo_tcb(q->pid);
                    if (!h) {
                        c_err++;
                        return;
                    }
                    h->estado = TCP_SYN_RX;
                    h->pto_loc = dp;
                    h->pto_rem = sp;
                    h->ip_rem = src;
                    h->irs = seq;
                    h->rcv_nxt = seq + 1;
                    h->iss = irq_ticks() + ((uint32_t)h->pid << 16) +
                             (uint32_t)id_de(h);
                    h->snd_nxt = h->iss + 1;
                    h->snd_una = h->iss;
                    h->rto = RTO0;
                    h->hijo_de = i;
                    q->pendiente = (int)(h - tcbs);
                    if (band != 0x02) {
                        c_err++;
                        libera(h);
                        q->pendiente = -1;
                        return;
                    }
                    if (tcp_tx(h, 0x12, 0, 0, h->iss) < 0) {
                        c_err++;
                        libera(h);
                        q->pendiente = -1;
                        return;
                    }
                    c_ok++;
                    return;
                }
            }
            /* Puerto cerrado: RST con seq 0 y ack del extremo. */
            {
                int largo = n - thl + ((band & 0x02) ? 1 : 0) +
                            ((band & 0x01) ? 1 : 0);
                rst_para(src, sp, dp, 0, seq + (uint32_t)largo);
            }
            c_err++;
            return;
        }
        /* RST: en apertura se valida por ACK (rechazo rapido);
           establecida, por ventana (no aceptar basura vieja). */
        if (band & 0x04) {
            if (c->estado == TCP_SYN_TX || c->estado == TCP_SYN_RX) {
                if (ackn != c->snd_nxt) return;
                if (c->hijo_de >= 0) {
                    int p = c->hijo_de;
                    if (p >= 0 && p < TCP_N &&
                        tcbs[p].pendiente == (int)(c - tcbs))
                        tcbs[p].pendiente = -1;
                    libera(c);
                } else {
                    muere(c, -7);
                }
                return;
            }
            if ((int32_t)(seq - c->rcv_nxt) < -(int32_t)TCP_RCV)
                return;
            if (c->hijo_de >= 0) {
                int p = c->hijo_de;
                if (p >= 0 && p < TCP_N &&
                    tcbs[p].pendiente == (int)(c - tcbs))
                    tcbs[p].pendiente = -1;
                libera(c);
            } else {
                muere(c, -9);
            }
            return;
        }
        if (c->estado == TCP_ESCUCHA) return;   /* datos a escucha */
        /* ACK: avanza lo pendiente (stop-and-wait: todo o nada). */
        if (band & 0x10) {
            if ((int32_t)(ackn - c->snd_una) < 0 ||
                (int32_t)(ackn - c->snd_nxt) > 0) {
                c_err++;
                return;
            }
            if (c->estado == TCP_SYN_TX) {
                if (!(band & 0x02)) {
                    /* ACK pelado en SYN_TX: lo ignora (retransmite). */
                } else if (ackn == c->iss + 1) {
                    c->irs = seq;
                    c->rcv_nxt = seq + 1;
                    c->snd_una = ackn;
                    c->estado = TCP_ESTABLECIDO;
                    c->reint = 0;
                    c->rto = RTO0;
                    tcp_tx(c, 0x10, 0, 0, c->snd_nxt);
                    c_ok++;
                }
                return;
            }
            if (c->estado == TCP_SYN_RX) {
                if (ackn == c->snd_nxt) {
                    c->snd_una = ackn;
                    c->estado = TCP_ESTABLECIDO;
                    c->reint = 0;
                    c->rto = RTO0;
                    c_ok++;
                }
                /* Si no casa, el RTO reenviara el SYN-ACK. */
            } else if (ackn == c->snd_nxt) {
                if (c->snd_len) {
                    c->snd_len = 0;
                    c->reint = 0;
                    c->rto = RTO0;
                }
                if (c->fin_ack == 0 && c->estado == TCP_FIN_TX &&
                    (int32_t)(ackn - (c->snd_nxt)) >= 0) {
                    c->fin_ack = 1;
                }
            }
            c->wnd_par = (uint16_t)((t[14] << 8) | t[15]);
        } else if (c->estado != TCP_SYN_TX) {
            c_err++;
            return;   /* sin ACK fuera de apertura */
        }
        /* SYN simultaneo en SYN_TX. */
        if ((band & 0x02) && c->estado == TCP_SYN_TX &&
            !(band & 0x10)) {
            c->irs = seq;
            c->rcv_nxt = seq + 1;
            c->estado = TCP_SYN_RX;
            tcp_tx(c, 0x12, 0, 0, c->iss);
            return;
        }
        /* Datos. */
        {
            int dl = n - thl;
            if (dl > 0) {
                if (c->estado != TCP_ESTABLECIDO &&
                    c->estado != TCP_FIN_RX) {
                    c_err++;
                    return;
                }
                if (seq == c->rcv_nxt) {
                    int cabe = TCP_RCV - c->rcv_n;
                    if (dl > cabe) dl = cabe;
                    if (dl > 0) {
                        int k;
                        for (k = 0; k < dl; k++)
                            c->rcv[(c->rcv_ini + c->rcv_n + k) %
                                   TCP_RCV] = t[thl + k];
                        c->rcv_n += (uint16_t)dl;
                        c->rcv_nxt += (uint32_t)dl;
                        c_ok++;
                    }
                    tcp_tx(c, 0x10, 0, 0, c->snd_nxt);
                } else {
                    /* Duplicado u OOO: acuse del punto actual. */
                    tcp_tx(c, 0x10, 0, 0, c->snd_nxt);
                }
            }
        }
        /* FIN. */
        if (band & 0x01) {
            uint32_t fin_seq = seq + (uint32_t)(n - thl) +
                               ((band & 0x02) ? 1u : 0u);
            if (c->estado == TCP_ESTABLECIDO ||
                c->estado == TCP_FIN_RX) {
                if (fin_seq == c->rcv_nxt) {
                    c->rcv_nxt++;
                    c->fin_rx = 1;
                    if (c->estado == TCP_ESTABLECIDO)
                        c->estado = TCP_FIN_RX;
                    tcp_tx(c, 0x10, 0, 0, c->snd_nxt);
                    c_ok++;
                }
            } else if (c->estado == TCP_FIN_TX) {
                if (fin_seq == c->rcv_nxt) {
                    c->rcv_nxt++;
                    c->fin_rx = 1;
                    tcp_tx(c, 0x10, 0, 0, c->snd_nxt);
                    c_ok++;
                }
            }
        }
    }
}

/* ---- API bloqueante (rodajas como ARP/ICMP/UDP) ---- */

static void duerme(void) {
    proc_actual->modo = 0;
    proc_bloquea_ticks(irq_ticks() + 2u);
}

int red_tcp_abrir(uint32_t ip, uint16_t puerto, uint16_t local,
                  uint32_t pid);   /* declarada; definicion tras nb */
static int abre_nb(uint32_t ip, uint16_t puerto, uint16_t local,
                   uint32_t pid) {
    tcp_t *t;
    if (!ip || !puerto || !pid) return -2;
    t = nuevo_tcb(pid);
    if (!t) return -6;
    t->estado = TCP_SYN_TX;
    t->ip_rem = ip;
    t->pto_rem = puerto;
    if (local) {
        int i;
        for (i = 0; i < TCP_N; i++) {
            if (i != (int)(t - tcbs) && tcbs[i].estado != TCP_CERRADO &&
                tcbs[i].pto_loc == local) {
                libera(t);
                return -6;
            }
        }
        t->pto_loc = local;
    } else {
        static uint16_t efem = 49152;
        int k;
        for (k = 0; k < 16384; k++) {
            int choca = 0, i;
            if (++efem < 49152) efem = 49152;
            for (i = 0; i < TCP_N; i++) {
                if (i != (int)(t - tcbs) &&
                    tcbs[i].estado != TCP_CERRADO &&
                    tcbs[i].pto_loc == efem) {
                    choca = 1;
                    break;
                }
            }
            if (!choca) break;
        }
        t->pto_loc = efem;
    }
    t->iss = irq_ticks() + (pid << 16) + (uint32_t)id_de(t) * 7919u;
    t->snd_nxt = t->iss + 1;
    t->snd_una = t->iss;
    t->wnd_par = TCP_MSS;
    t->rto = RTO0;
    t->reint = 0;
    if (tcp_tx(t, 0x02, 0, 0, t->iss) < 0) {
        libera(t);
        return -5;
    }
    return id_de(t);
}

int red_tcp_abrir(uint32_t ip, uint16_t puerto, uint16_t local,
                  uint32_t pid) {
    int id = abre_nb(ip, puerto, local, pid);
    tcp_t *t;
    uint32_t fin;
    if (id < 0) return id;
    t = &tcbs[id % TCP_N];
    fin = irq_ticks() + CONEX_ESP;
    for (;;) {
        id = id_de(t);
        red_bombea();
        tcp_reloj();
        t = &tcbs[id % TCP_N];
        if (t->pid != pid ||
            (int)(t->gen * TCP_N + (uint32_t)(id % TCP_N)) != id) {
            return -9;
        }
        if (t->estado == TCP_ESTABLECIDO) return id;
        if (t->estado == TCP_CERRADO) {
            int e = t->err ? t->err : -7;
            libera(t);
            return e;
        }
        if ((int32_t)(irq_ticks() - fin) >= 0) {
            libera(t);
            return -7;
        }
        duerme();
    }
}

int red_tcp_escuchar(uint16_t puerto, uint32_t pid) {
    tcp_t *t;
    int i;
    if (!puerto || !pid) return -2;
    for (i = 0; i < TCP_N; i++) {
        if (tcbs[i].estado != TCP_CERRADO &&
            tcbs[i].pto_loc == puerto)
            return -6;
    }
    t = nuevo_tcb(pid);
    if (!t) return -6;
    t->estado = TCP_ESCUCHA;
    t->pto_loc = puerto;
    return id_de(t);
}

int red_tcp_aceptar(int id_esc, uint32_t pid, uint32_t espera_ms) {
    tcp_t *q;
    uint32_t fin;
    int r = idx_de_id(id_esc, pid, &q);
    if (r < 0) return r;
    if (q->estado != TCP_ESCUCHA) return -8;
    if (espera_ms > 10000) espera_ms = 10000;
    fin = irq_ticks() + espera_ms / 10u + 1u;
    for (;;) {
        int h;
        r = idx_de_id(id_esc, pid, &q);
        if (r < 0) return r;
        if (q->estado != TCP_ESCUCHA) return -8;
        h = q->pendiente;
        if (h >= 0 && h < TCP_N) {
            tcp_t *c = &tcbs[h];
            if (c->estado == TCP_ESTABLECIDO) {
                q->pendiente = -1;
                c->hijo_de = -1;
                return id_de(c);
            }
        }
        if ((int32_t)(irq_ticks() - fin) >= 0) return 0;
        red_bombea();
        tcp_reloj();
        duerme();
    }
}

static int envia_nb(tcp_t *t, const uint8_t *d, int n) {
    int i;
    if (!d || n <= 0 || n > TCP_MSS) return -2;
    if (t->estado != TCP_ESTABLECIDO && t->estado != TCP_FIN_RX)
        return -8;
    if (t->snd_len) return -8;   /* stop-and-wait: uno en vuelo */
    for (i = 0; i < n; i++) t->snd[i] = d[i];
    t->snd_len = (uint16_t)n;
    if (tcp_tx(t, 0x18, t->snd, n, t->snd_nxt) < 0) {
        t->snd_len = 0;
        return -5;
    }
    t->snd_nxt += (uint32_t)n;
    t->reint = 0;
    t->rto = RTO0;
    return 0;
}

int red_tcp_enviar(int id, uint32_t pid, const uint8_t *d, int n) {
    tcp_t *t;
    uint32_t fin;
    int r = idx_de_id(id, pid, &t);
    if (r < 0) return r;
    r = envia_nb(t, d, n);
    if (r < 0) return r;
    fin = irq_ticks() + CONEX_ESP + 200u;
    for (;;) {
        r = idx_de_id(id, pid, &t);
        if (r < 0) return r;
        if (!t->snd_len) return n;
        if ((int32_t)(irq_ticks() - fin) >= 0) {
            muere(t, -7);
            return -7;
        }
        red_bombea();
        tcp_reloj();
        duerme();
    }
}

int red_tcp_recibir(int id, uint32_t pid, uint8_t *dst, int max,
                    uint32_t espera_ms) {
    tcp_t *t;
    uint32_t fin;
    int r = idx_de_id(id, pid, &t);
    int k;
    if (r < 0) return r;
    if (!dst || max <= 0) return -2;
    if (t->estado != TCP_ESTABLECIDO && t->estado != TCP_FIN_RX &&
        t->estado != TCP_FIN_TX)
        return -8;
    if (espera_ms > 10000) espera_ms = 10000;
    fin = irq_ticks() + espera_ms / 10u + 1u;
    for (;;) {
        r = idx_de_id(id, pid, &t);
        if (r < 0) return r;
        if (t->rcv_n) {
            int copia = t->rcv_n > max ? max : t->rcv_n;
            for (k = 0; k < copia; k++)
                dst[k] = t->rcv[(t->rcv_ini + k) % TCP_RCV];
            t->rcv_ini = (t->rcv_ini + copia) % TCP_RCV;
            t->rcv_n -= (uint16_t)copia;
            return copia;
        }
        if (t->fin_rx) return -12;
        if ((int32_t)(irq_ticks() - fin) >= 0) return 0;
        red_bombea();
        tcp_reloj();
        duerme();
    }
}

/* Envia FIN (desde ESTABLECIDO o FIN_RX). 0 ok, negativo error. */
static int fin_nb(tcp_t *t) {
    if (t->estado != TCP_ESTABLECIDO && t->estado != TCP_FIN_RX)
        return -8;
    t->estado = TCP_FIN_TX;
    t->fin_ack = 0;
    t->reint = 0;
    t->rto = RTO0;
    if (tcp_tx(t, 0x11, 0, 0, t->snd_nxt) < 0) return -5;
    t->snd_nxt++;
    return 0;
}

/* Cierre ya completo (FIN nuestro con ACK + FIN ajeno): envia el
   ACK final. 1 si completo, 0 si aun falta. */
static int fin_listo(tcp_t *t) {
    if (t->fin_rx && t->fin_ack) {
        tcp_tx(t, 0x10, 0, 0, t->snd_nxt);
        return 1;
    }
    return 0;
}

int red_tcp_cerrar(int id, uint32_t pid) {
    tcp_t *t = 0;
    uint32_t fin;
    int r = idx_de_id(id, pid, &t);
    if (r == -2 || r == -6) return r;
    if (r < 0) {   /* CERRADO con sticky: recoge y libera */
        int e = r;
        if (t) libera(t);
        return e;
    }
    if (t->estado == TCP_ESCUCHA) {
        int h = t->pendiente;
        if (h >= 0 && h < TCP_N && tcbs[h].estado != TCP_CERRADO) {
            rst_para(tcbs[h].ip_rem, tcbs[h].pto_rem,
                     tcbs[h].pto_loc, tcbs[h].snd_nxt,
                     tcbs[h].rcv_nxt);
            libera(&tcbs[h]);
        }
        libera(t);
        return 0;
    }
    if (t->estado == TCP_SYN_TX || t->estado == TCP_SYN_RX) {
        rst_para(t->ip_rem, t->pto_rem, t->pto_loc, t->snd_nxt,
                 t->rcv_nxt);
        libera(t);
        return 0;
    }
    if (t->estado == TCP_CERRADO) {
        int e = t->err ? t->err : 0;
        libera(t);
        return e;
    }
    /* Drena envio previo (acotado) y FIN. */
    {
        uint32_t f2 = irq_ticks() + 300u;
        while (t->snd_len && (int32_t)(irq_ticks() - f2) < 0) {
            red_bombea();
            tcp_reloj();
            duerme();
        }
        t->snd_len = 0;
    }
    r = fin_nb(t);
    if (r < 0) {
        libera(t);
        return r == -8 ? -8 : -5;
    }
    fin = irq_ticks() + CIERRE_ESP;
    for (;;) {
        r = idx_de_id(id, pid, &t);
        if (r < 0) return r;
        if (fin_listo(t)) {
            libera(t);
            return 0;
        }
        if ((int32_t)(irq_ticks() - fin) >= 0) {
            int e = t->fin_ack ? 0 : -7;
            libera(t);
            return e;
        }
        red_bombea();
        tcp_reloj();
        duerme();
    }
}

int red_tcp_abortar(int id, uint32_t pid) {
    tcp_t *t;
    int r = idx_de_id(id, pid, &t);
    if (r < 0) {
        if (r == -6 || r == -2) return r;
        /* CERRADO con sticky: libera y 0. */
        int i = id % TCP_N;
        if (i < 0 || i >= TCP_N) return -2;
        t = &tcbs[i];
        if ((int)(t->gen * TCP_N + (uint32_t)i) != id) return -2;
        if (t->pid != pid) return -6;
        libera(t);
        return 0;
    }
    if (t->estado != TCP_ESCUCHA) {
        rst_para(t->ip_rem, t->pto_rem, t->pto_loc, t->snd_nxt,
                 t->rcv_nxt);
    } else if (t->pendiente >= 0 && t->pendiente < TCP_N) {
        tcp_t *h = &tcbs[t->pendiente];
        if (h->estado != TCP_CERRADO) {
            rst_para(h->ip_rem, h->pto_rem, h->pto_loc, h->snd_nxt,
                     h->rcv_nxt);
            libera(h);
        }
        t->pendiente = -1;
    }
    libera(t);
    return 0;
}

int red_tcp_estado(int id, uint32_t pid) {
    tcp_t *t;
    int r = idx_de_id(id, pid, &t);
    if (r < 0) return r;
    return (int)t->estado;
}

int red_tcp_limpia_proceso(uint32_t pid) {
    int i, n = 0;
    uint8_t mac[6];
    if (!pid) return 0;
    for (i = 0; i < TCP_N; i++) {
        tcp_t *t = &tcbs[i];
        uint32_t gw = 0;
        if (t->estado == TCP_CERRADO || t->pid != pid) continue;
        /* RST solo sin bloquear: ruta + ARP en cache. */
        if (t->estado != TCP_ESCUCHA && !red_ruta_para(t->ip_rem, &gw) &&
            !red_arp_mira(gw ? gw : t->ip_rem, mac)) {
            rst_para(t->ip_rem, t->pto_rem, t->pto_loc, t->snd_nxt,
                     t->rcv_nxt);
        }
        if (t->pendiente >= 0 && t->pendiente < TCP_N)
            libera(&tcbs[t->pendiente]);
        t->pendiente = -1;
        libera(t);
        n++;
    }
    return n;
}

/* ---- KAT (captura de Tx; cero trafico al cable) ---- */

static uint8_t kat_frm[1600];

/* Trama eth+ip+tcp completa con checksums validos. */
static int kat_arma(uint32_t src, uint16_t sp, uint16_t dp,
                    uint32_t seq, uint32_t ackn, uint8_t band,
                    const uint8_t *d, int n) {
    uint8_t *m = kat_frm;
    int i, tot = 20 + 20 + n;
    uint16_t c;
    for (i = 0; i < 14; i++) m[i] = 0;
    m[12] = 0x08;
    m[13] = 0x00;
    m[14] = 0x45;
    m[15] = 0;
    m[16] = (uint8_t)((tot >> 8) & 0xFFu);
    m[17] = (uint8_t)(tot & 0xFFu);
    m[18] = 0x12;
    m[19] = 0x34;
    m[20] = 0;
    m[21] = 0;
    m[22] = 64;
    m[23] = 6;
    m[24] = 0;
    m[25] = 0;
    m[26] = (uint8_t)(src & 0xFFu);
    m[27] = (uint8_t)((src >> 8) & 0xFFu);
    m[28] = (uint8_t)((src >> 16) & 0xFFu);
    m[29] = (uint8_t)((src >> 24) & 0xFFu);
    m[30] = (uint8_t)(KAT_MIO & 0xFFu);
    m[31] = (uint8_t)((KAT_MIO >> 8) & 0xFFu);
    m[32] = (uint8_t)((KAT_MIO >> 16) & 0xFFu);
    m[33] = (uint8_t)((KAT_MIO >> 24) & 0xFFu);
    c = red_ip_cksum(m + 14, 20);
    m[24] = (uint8_t)((c >> 8) & 0xFFu);
    m[25] = (uint8_t)(c & 0xFFu);
    m[34] = (uint8_t)((sp >> 8) & 0xFFu);
    m[35] = (uint8_t)(sp & 0xFFu);
    m[36] = (uint8_t)((dp >> 8) & 0xFFu);
    m[37] = (uint8_t)(dp & 0xFFu);
    m[38] = (uint8_t)((seq >> 24) & 0xFFu);
    m[39] = (uint8_t)((seq >> 16) & 0xFFu);
    m[40] = (uint8_t)((seq >> 8) & 0xFFu);
    m[41] = (uint8_t)(seq & 0xFFu);
    m[42] = (uint8_t)((ackn >> 24) & 0xFFu);
    m[43] = (uint8_t)((ackn >> 16) & 0xFFu);
    m[44] = (uint8_t)((ackn >> 8) & 0xFFu);
    m[45] = (uint8_t)(ackn & 0xFFu);
    m[46] = 0x50;
    m[47] = band;
    m[48] = 0x08;
    m[49] = 0x00;
    m[50] = 0;
    m[51] = 0;
    m[52] = 0;
    m[53] = 0;
    for (i = 0; i < n; i++) m[54 + i] = d[i];
    {
        uint8_t seudo[8];
        int k;
        for (k = 0; k < 4; k++) {
            seudo[k] = m[26 + k];
            seudo[4 + k] = m[30 + k];
        }
        c = cks_tcp(seudo, seudo + 4, m + 34, 20 + n);
        m[50] = (uint8_t)((c >> 8) & 0xFFu);
        m[51] = (uint8_t)(c & 0xFFu);
    }
    return 14 + tot;
}

/* Handshake activo hasta ESTABLECIDO (devuelve id o negativo). */
static int kat_conecta(uint16_t lp, uint16_t rp, uint32_t irs) {
    int id = abre_nb(KAT_PAR, rp, lp, 9999);
    tcp_t *t;
    int n;
    if (id <= 0) return -1;
    t = &tcbs[id % TCP_N];
    n = kat_arma(KAT_PAR, rp, lp, irs, t->iss + 1, 0x12, 0, 0);
    red_tcp_entrada(kat_frm, n);
    if (t->estado != TCP_ESTABLECIDO) return -2;
    return id;
}

void red_tcp_kat(void) {
    uint8_t buf[64];
    uint32_t e0, e1;
    int i, ok = 1, kat0;
    kat_on = 1;
    kat_n = 0;
    /* 1. Vectores malos: corta, checksum, SYN a puerto cerrado. */
    kat_n = 0;
    e0 = c_err;
    red_tcp_entrada(kat_frm, 30);    {
        int n = kat_arma(KAT_PAR, 80, 50000, 1000, 0, 0x02, 0, 0);
        kat_frm[14 + 20 + 16] ^= 0xFF;
        red_tcp_entrada(kat_frm, n);
    }
    e1 = c_err;
    if (e1 != e0 + 2) ok = 0;   /* corta + checksum */
    if (kat_n != 0) ok = 0;   /* nada que transmitir */
    {
        /* RST en apertura: con ACK valido cierra (-7); sin el,
           se ignora y sigue retransmitiendo. */
        tcp_t *t;
        int id = abre_nb(KAT_PAR, 81, 50010, 9999);
        uint32_t iss;
        int n;
        kat_n = 0;
        if (id <= 0) ok = 0;
        else {
            t = &tcbs[id % TCP_N];
            iss = t->iss;
            n = kat_arma(KAT_PAR, 81, 50010, 4000, iss + 1, 0x04, 0, 0);
            red_tcp_entrada(kat_frm, n);
            if (t->estado != TCP_CERRADO || t->err != -7) ok = 0;
            if (red_tcp_cerrar(id, 9999) != -7) ok = 0;
        }
        id = abre_nb(KAT_PAR, 82, 50011, 9999);
        kat_n = 0;
        if (id <= 0) ok = 0;
        else {
            t = &tcbs[id % TCP_N];
            iss = t->iss;
            n = kat_arma(KAT_PAR, 82, 50011, 4000, iss + 5, 0x04, 0, 0);
            red_tcp_entrada(kat_frm, n);
            if (t->estado != TCP_SYN_TX) ok = 0;
            red_tcp_abortar(id, 9999);
        }
    }
    {
        int n = kat_arma(KAT_PAR, 80, 61999, 1000, 0, 0x02, 0, 0);
        kat0 = kat_n;
        red_tcp_entrada(kat_frm, n);
        if (kat_n != kat0 + 1) ok = 0;
        else if (kat_r[kat_n - 1].band != 0x14) ok = 0;
        else if (kat_r[kat_n - 1].dp != 80) ok = 0;
        else if (kat_r[kat_n - 1].ackn != 1001) ok = 0;
    }
    /* 2. Handshake activo + datos + FIN ajeno + EOF. */
    kat_n = 0;
    {
        static const uint8_t hola[] = { 'h', 'o', 'l', 'a' };
        int id, n;
        tcp_t *t;
        kat0 = kat_n;
        id = kat_conecta(50000, 80, 7000);
        if (id <= 0) ok = 0;
        else {
            t = &tcbs[id % TCP_N];
            if (kat_n != kat0 + 2) ok = 0;
            else if (kat_r[kat0].band != 0x02) ok = 0;
            else if (kat_r[kat0 + 1].band != 0x10) ok = 0;
            else if (kat_r[kat0 + 1].ackn != 7001) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50000, 7001, t->snd_nxt,
                         0x18, hola, 4);
            kat0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (red_tcp_recibir(id, 9999, buf, (int)sizeof(buf),
                                0) != 4)
                ok = 0;
            if (buf[0] != 'h' || buf[3] != 'a') ok = 0;
            if (kat_n != kat0 + 1) ok = 0;   /* ACK puro */
            else if (kat_r[kat_n - 1].ackn != 7005) ok = 0;
            /* OOO: se descarta (dup ACK, sin avanzar). */
            n = kat_arma(KAT_PAR, 80, 50000, 7010, t->snd_nxt,
                         0x18, hola, 4);
            red_tcp_entrada(kat_frm, n);
            if (t->rcv_nxt != 7005) ok = 0;
            /* FIN ajeno: EOF tras drenar (ya drenado). */
            n = kat_arma(KAT_PAR, 80, 50000, 7005, t->snd_nxt,
                         0x11, 0, 0);
            red_tcp_entrada(kat_frm, n);
            if (t->estado != TCP_FIN_RX) ok = 0;
            if (red_tcp_recibir(id, 9999, buf, (int)sizeof(buf),
                                0) != -12)
                ok = 0;
            /* Cierre nuestro por pasos (sin bloquear). */
            {
                tcp_t *tt = &tcbs[id % TCP_N];
                uint32_t fseq;
                if (fin_nb(tt) != 0) ok = 0;
                if (kat_r[kat_n - 1].band != 0x11) ok = 0;
                fseq = tt->snd_nxt - 1;
                n = kat_arma(KAT_PAR, 80, 50000, 7006, fseq + 1,
                             0x10, 0, 0);
                red_tcp_entrada(kat_frm, n);
                if (!tt->fin_ack) ok = 0;
                if (!fin_listo(tt)) ok = 0;
                if (kat_r[kat_n - 1].band != 0x10) ok = 0;
                libera(tt);
                if (red_tcp_estado(id, 9999) != -2) ok = 0;
            }
        }
    }
    /* 3. Retransmision y agotamiento. */
    kat_n = 0;
    {
        int id = kat_conecta(50001, 80, 8000);
        tcp_t *t;
        static const uint8_t abc[] = { 'a', 'b', 'c' };
        if (id <= 0) ok = 0;
        else {
            t = &tcbs[id % TCP_N];
            if (envia_nb(t, abc, 3) != 0) ok = 0;
            kat0 = kat_n;
            t->t0 -= 1000;
            tcp_reloj();
            if (t->reint != 1) ok = 0;
            if (kat_n != kat0 + 1) ok = 0;
            else if (kat_r[kat_n - 1].nd != 3) ok = 0;
            t->t0 -= 100000;
            {
                int k;
                for (k = 0; k < 8; k++) {
                    t->t0 -= 100000;   /* el reenvio refresca t0 */
                    tcp_reloj();
                }
            }
            if (t->estado != TCP_CERRADO || t->err != -7) ok = 0;
            if (red_tcp_recibir(id, 9999, buf, (int)sizeof(buf),
                                0) != -7)
                ok = 0;
            if (red_tcp_cerrar(id, 9999) != -7) ok = 0;
            if (red_tcp_estado(id, 9999) != -2) ok = 0;
        }
    }
    /* 4. RST valido mata con -9 sticky. */
    kat_n = 0;
    {
        int id = kat_conecta(50002, 80, 9000);
        tcp_t *t;
        int n;
        if (id <= 0) ok = 0;
        else {
            t = &tcbs[id % TCP_N];
            n = kat_arma(KAT_PAR, 80, 50002, t->rcv_nxt, 0, 0x04,
                         0, 0);
            red_tcp_entrada(kat_frm, n);
            if (t->estado != TCP_CERRADO || t->err != -9) ok = 0;
            if (red_tcp_recibir(id, 9999, buf, (int)sizeof(buf),
                                0) != -9)
                ok = 0;
            if (red_tcp_cerrar(id, 9999) != -9) ok = 0;
        }
    }
    /* 5. Pasiva: escucha, SYN, acepta. */
    kat_n = 0;
    {
        int esc, n, hid;
        tcp_t *t;
        esc = red_tcp_escuchar(61000, 9999);
        if (esc <= 0) ok = 0;
        else {
            kat0 = kat_n;
            n = kat_arma(KAT_PAR, 4567, 61000, 3000, 0, 0x02, 0, 0);
            red_tcp_entrada(kat_frm, n);
            t = &tcbs[esc % TCP_N];
            if (t->pendiente < 0) ok = 0;
            if (kat_n != kat0 + 1) ok = 0;
            else if (kat_r[kat_n - 1].band != 0x12) ok = 0;
            if (ok && t->pendiente >= 0 && t->pendiente < TCP_N) {
                tcp_t *h = &tcbs[t->pendiente];
                n = kat_arma(KAT_PAR, 4567, 61000, 3001,
                             h->snd_nxt, 0x10, 0, 0);
                red_tcp_entrada(kat_frm, n);
            }
            hid = red_tcp_aceptar(esc, 9999, 0);
            if (hid <= 0) ok = 0;
            else if (red_tcp_estado(hid, 9999) != TCP_ESTABLECIDO)
                ok = 0;
            /* Aborto (no bloqueante) + cierre de la escucha. */
            if (red_tcp_abortar(hid, 9999) != 0) ok = 0;
            if (red_tcp_cerrar(esc, 9999) != 0) ok = 0;
        }
    }
    /* 6. Tabla llena: sin desalojo en TCP (conexiones con estado). */
    kat_n = 0;
    {
        int a, b, c, d, e;
        a = red_tcp_escuchar(61001, 9999);
        b = red_tcp_escuchar(61002, 9999);
        c = red_tcp_escuchar(61003, 9999);
        d = red_tcp_escuchar(61004, 9999);
        if (a <= 0 || b <= 0 || c <= 0 || d <= 0) ok = 0;
        e = red_tcp_escuchar(61005, 9999);
        if (e != -6) ok = 0;
        if (red_tcp_limpia_proceso(9999) != 4) ok = 0;
        for (i = 0; i < TCP_N; i++) {
            if (tcbs[i].estado != TCP_CERRADO) ok = 0;
        }
    }
    kat_on = 0;
    kat_n = 0;
    kprintf("red: KAT tcp %s\r\n", ok ? "OK" : "FALLO");
}
