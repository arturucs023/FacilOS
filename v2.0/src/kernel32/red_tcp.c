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
    uint16_t wnd;   /* F6-C: ventana anunciada por el segmento */
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
            /* F6-B D2 + F6-C D9: saneado completo (controles y
               cargas; sin residuos de la conexion anterior). */
            libc_memset(t->snd, 0, TCP_MSS);
            libc_memset(t->rcv, 0, TCP_RCV);
            t->ip_rem = 0;
            t->pto_loc = 0;
            t->pto_rem = 0;
            t->iss = 0;
            t->irs = 0;
            t->snd_nxt = 0;
            t->snd_una = 0;
            t->rcv_nxt = 0;
            t->t0 = irq_ticks();
            t->rto = RTO0;
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
    /* F6-B D2 + F6-C D9: saneado completo para reutilizacion segura
       (controles y cargas; sin residuos de la conexion anterior). */
    t->estado = TCP_CERRADO;
    t->pid = 0;
    t->err = 0;
    libc_memset(t->snd, 0, TCP_MSS);
    libc_memset(t->rcv, 0, TCP_RCV);
    t->ip_rem = 0;
    t->pto_loc = 0;
    t->pto_rem = 0;
    t->iss = 0;
    t->irs = 0;
    t->snd_nxt = 0;
    t->snd_una = 0;
    t->rcv_nxt = 0;
    t->wnd_par = 0;
    t->snd_len = 0;
    t->rcv_ini = 0;
    t->rcv_n = 0;
    t->fin_rx = 0;
    t->fin_ack = 0;
    t->reint = 0;
    t->t0 = 0;
    t->rto = RTO0;
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

/* F6-C D15: cuarentena de tuplas (TIME_WAIT sin ocupar TCB). Tras un
   cierre ordenado o un aborto, la tupla queda en gracia TUP_GRACIA:
   un SYN nuevo o una apertura activa con la misma tupla se rechaza
   (el par reintenta por RTO y lo logra tras la gracia). Con 4 TCB,
   un TIME_WAIT clasico consumiria 25% de la tabla por cierre; la
   cuarentena da la misma proteccion (sin reencarnaciones) con 48 B
   fijos, caducidad perezosa y presion nula sobre la tabla. */
#define TUP_N 4
#define TUP_GRACIA 600u   /* 6 s: cubre el ciclo RTO del par */
static struct {
    uint32_t ip;
    uint16_t loc, rem;
    uint32_t hasta;
} tups[TUP_N];

static int tup_viva(uint32_t ip, uint16_t loc, uint16_t rem) {
    int i;
    uint32_t ahora = irq_ticks();
    for (i = 0; i < TUP_N; i++) {
        if (tups[i].hasta && tups[i].ip == ip &&
            tups[i].loc == loc && tups[i].rem == rem) {
            if ((int32_t)(tups[i].hasta - ahora) > 0) return 1;
            tups[i].hasta = 0;
        }
    }
    return 0;
}

static void tup_guarda(uint32_t ip, uint16_t loc, uint16_t rem) {
    int i, vict = 0;
    uint32_t ahora = irq_ticks();
    if (!ip || !loc || !rem) return;
    for (i = 0; i < TUP_N; i++) {
        if (!tups[i].hasta ||
            (int32_t)(tups[i].hasta - ahora) <= 0) {
            vict = i;
            break;
        }
        if ((int32_t)(tups[i].hasta - tups[vict].hasta) < 0)
            vict = i;
    }
    tups[vict].ip = ip;
    tups[vict].loc = loc;
    tups[vict].rem = rem;
    tups[vict].hasta = ahora + TUP_GRACIA;
}

/* Guarda la tupla de t antes de liberarla (t valida). */
static void cierra_guarda(tcp_t *t) {
    if (t->ip_rem && t->pto_loc && t->pto_rem)
        tup_guarda(t->ip_rem, t->pto_loc, t->pto_rem);
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
            kat_r[kat_n].wnd = (uint16_t)((seg[14] << 8) | seg[15]);
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
static void rst_band(uint32_t dst, uint16_t dp, uint16_t sp,
                     uint32_t seq, uint32_t ackn, uint8_t band);
static void rst_para(uint32_t dst, uint16_t dp, uint16_t sp,
                     uint32_t seq, uint32_t ackn) {
    rst_band(dst, dp, sp, seq, ackn, 0x14);   /* RST+ACK */
}

/* F6-C D10: RST pelado (sin ACK) para responder a un segmento que
   trae ACK: seq = SEG.ACK (RFC 793). */
static void rst_band(uint32_t dst, uint16_t dp, uint16_t sp,
                     uint32_t seq, uint32_t ackn, uint8_t band) {
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
    s[13] = band;   /* 0x14 RST+ACK, 0x04 RST pelado (D10) */
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
        /* F6-B D4: el FIN consume 1 numero (snd_nxt ya avanzo);
           la re-Tx repite fseq = snd_nxt-1, no snd_nxt. */
        tcp_tx(t, 0x11, 0, 0, t->snd_nxt - 1);
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

/* F6-C: avance del reloj sin depender de una syscall TCP bloqueada
   (red_bombea lo llama: RTO progresa con cualquier espera de red). */
void red_tcp_reloj(void) { tcp_reloj(); }

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
            int ocupado = 0;
            for (i = 0; i < TCP_N; i++) {
                tcp_t *q = &tcbs[i];
                if (q->estado == TCP_ESCUCHA && q->pto_loc == dp) {
                    if (q->pendiente >= 0) {
                        ocupado = 1;
                        break;
                    }
                    /* F6-C D15: SYN a tupla en gracia = gota
                       (el cliente reintenta tras la gracia). */
                    if (tup_viva(src, dp, sp)) {
                        c_err++;
                        return;
                    }
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
            /* F6-B D8: backlog lleno = gota silenciosa (el cliente
               reintenta por RTO); solo RST si no hay escucha. */
            if (ocupado) {
                c_err++;
                return;
            }
            /* Puerto cerrado: si el entrante trae ACK, RST pelado
               con seq = SEG.ACK (RFC 793); si no, RST+ACK con seq 0
               y ack del extremo (F6-C D10). */
            {
                int largo = n - thl + ((band & 0x02) ? 1 : 0) +
                            ((band & 0x01) ? 1 : 0);
                if (band & 0x10) rst_band(src, sp, dp, ackn, 0, 0x04);
                else rst_para(src, sp, dp, 0, seq + (uint32_t)largo);
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
        /* ACK: avanza lo pendiente (stop-and-wait: todo o nada).
           F6-B D11: ACK futuro (>snd_nxt) = gota entera; ACK viejo
           (<snd_una) se ignora pero datos/FIN validos siguen. */
        if (band & 0x10) {
            int ack_futuro = (int32_t)(ackn - c->snd_nxt) > 0;
            int ack_viejo = (int32_t)(ackn - c->snd_una) < 0;
            if (ack_futuro) {
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
                    /* F6-C D12: el lado activo tambien aprende la
                       ventana del SYN-ACK (antes se perdia). */
                    c->wnd_par = (uint16_t)((t[14] << 8) | t[15]);
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
            } else if (!ack_viejo && ackn == c->snd_nxt) {
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
            /* Si ack_viejo: no avanza nada, pero datos/FIN siguen. */
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
                /* F6-C D6: en FIN_TX se sigue aceptando lo del par
                   mientras no llego su FIN (half-close); lo que no
                   se admite es transmitir (ver envia_nb). El FIN del
                   mismo segmento se procesa abajo (datos+FIN). */
                if (c->estado != TCP_ESTABLECIDO &&
                    c->estado != TCP_FIN_RX &&
                    !(c->estado == TCP_FIN_TX && !c->fin_rx)) {
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
                } else if (c->fin_rx && fin_seq + 1 == c->rcv_nxt) {
                    /* F6-B D5: FIN duplicado (ACK nuestro perdido):
                       re-acusa sin avanzar. */
                    tcp_tx(c, 0x10, 0, 0, c->snd_nxt);
                    c_ok++;
                }
            } else if (c->estado == TCP_FIN_TX) {
                if (fin_seq == c->rcv_nxt) {
                    c->rcv_nxt++;
                    c->fin_rx = 1;
                    tcp_tx(c, 0x10, 0, 0, c->snd_nxt);
                    c_ok++;
                } else if (c->fin_rx && fin_seq + 1 == c->rcv_nxt) {
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
            /* F6-C D15: el efimero salta tuplas en gracia. */
            if (!choca && tup_viva(ip, efem, puerto)) choca = 1;
            if (!choca) break;
        }
        t->pto_loc = efem;
    }
    /* F6-C D15: la tupla en gracia no se reutiliza (transitorio);
       con puerto efimero el bucle ya la evita (red de seguridad). */
    if (tup_viva(ip, t->pto_loc, puerto)) {
        libera(t);
        return -6;
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
    int oid = abre_nb(ip, puerto, local, pid);
    tcp_t *t;
    uint32_t fin;
    if (oid < 0) return oid;
    fin = irq_ticks() + CONEX_ESP;
    for (;;) {
        /* F6-B D3: id estable (oid); si el slot se reutiliza,
           el alias se detecta y no se devuelve un id ajeno. */
        red_bombea();
        tcp_reloj();
        t = &tcbs[oid % TCP_N];
        if (t->pid != pid || id_de(t) != oid) {
            return -9;
        }
        if (t->estado == TCP_ESTABLECIDO) return oid;
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
    /* F6-C D13: espera 0 = consulta sin bloqueo (un bombeo para
       recoger lo ya llegado; sin dormir). */
    if (espera_ms == 0) {
        red_bombea();
        tcp_reloj();
    }
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
        if (espera_ms == 0) return 0;
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
    /* F6-C D12: respeta la ventana del par (0 = cerrada,
       n > wnd = insuficiente). La app reintenta; no se bloquea. */
    if (t->wnd_par == 0 || n > (int)t->wnd_par) return -8;
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
    /* F6-C D13: espera 0 = consulta sin bloqueo. */
    if (espera_ms == 0) {
        red_bombea();
        tcp_reloj();
    }
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
        if (espera_ms == 0) return 0;
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

/* F6-C D14: cierra con datos sin confirmar: no emite FIN con
   hueco (el par lo ignoraria y ambos lados colgarian); aborta con
   RST y devuelve -7. */
static int cierra_descarta(tcp_t *t) {
    rst_para(t->ip_rem, t->pto_rem, t->pto_loc, t->snd_nxt,
             t->rcv_nxt);
    cierra_guarda(t);
    libera(t);
    return -7;
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
    /* Drena envio previo (acotado) y FIN. Si tras el drenaje
       siguen datos sin confirmar, no se emite FIN con hueco
       (F6-C D14): se aborta con RST. */
    {
        uint32_t f2 = irq_ticks() + 300u;
        while (t->snd_len && (int32_t)(irq_ticks() - f2) < 0) {
            red_bombea();
            tcp_reloj();
            duerme();
        }
        if (t->snd_len) return cierra_descarta(t);
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
            /* F6-C D15: gracia de tupla tras cierre ordenado. */
            cierra_guarda(t);
            libera(t);
            return 0;
        }
        if ((int32_t)(irq_ticks() - fin) >= 0) {
            int e = t->fin_ack ? 0 : -7;
            if (t->fin_ack) cierra_guarda(t);
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
        /* F6-C D15: gracia de tupla tras aborto. */
        cierra_guarda(t);
    } else if (t->pendiente >= 0 && t->pendiente < TCP_N) {
        tcp_t *h = &tcbs[t->pendiente];
        if (h->estado != TCP_CERRADO) {
            rst_para(h->ip_rem, h->pto_rem, h->pto_loc, h->snd_nxt,
                     h->rcv_nxt);
            cierra_guarda(h);
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
        /* F6-B D2: incluye CERRADO+sticky del pid muerto (nadie
           lo recogera); sin RST en ese caso, solo saneado. */
        if (!t->pid || t->pid != pid) continue;
        if (t->estado == TCP_CERRADO) {
            libera(t);
            n++;
            continue;
        }
        /* RST solo sin bloquear: ruta + ARP en cache. */
        if (t->estado != TCP_ESCUCHA && !red_ruta_para(t->ip_rem, &gw) &&
            !red_arp_mira(gw ? gw : t->ip_rem, mac)) {
            rst_para(t->ip_rem, t->pto_rem, t->pto_loc, t->snd_nxt,
                     t->rcv_nxt);
        }
        /* F6-C D15: gracia de tupla tambien al morir el dueno. */
        if (t->estado != TCP_CERRADO) cierra_guarda(t);
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

/* F6-C: reescribe la ventana anunciada del marco KAT y recompone
   checksums IP/TCP. */
static void kat_pon_wnd(uint16_t w) {
    uint16_t c;
    uint8_t seudo[8];
    int k, tot;
    kat_frm[48] = (uint8_t)((w >> 8) & 0xFFu);
    kat_frm[49] = (uint8_t)(w & 0xFFu);
    kat_frm[24] = 0;
    kat_frm[25] = 0;
    c = red_ip_cksum(kat_frm + 14, 20);
    kat_frm[24] = (uint8_t)((c >> 8) & 0xFFu);
    kat_frm[25] = (uint8_t)(c & 0xFFu);
    for (k = 0; k < 4; k++) {
        seudo[k] = kat_frm[26 + k];
        seudo[4 + k] = kat_frm[30 + k];
    }
    tot = ((kat_frm[16] << 8) | kat_frm[17]) - 20;
    kat_frm[50] = 0;
    kat_frm[51] = 0;
    c = cks_tcp(seudo, seudo + 4, kat_frm + 34, tot);
    kat_frm[50] = (uint8_t)((c >> 8) & 0xFFu);
    kat_frm[51] = (uint8_t)(c & 0xFFu);
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
    /* 7. F6-B D2: limpia libera sticky del proceso muerto. */
    kat_n = 0;
    {
        int ids[4], k, n;
        tcp_t *t;
        for (k = 0; k < 4; k++) {
            ids[k] = kat_conecta((uint16_t)(50100 + k), 80,
                                 9000u + (uint32_t)k);
            if (ids[k] <= 0) ok = 0;
        }
        for (k = 0; k < 4; k++) {
            t = &tcbs[ids[k] % TCP_N];
            n = kat_arma(KAT_PAR, 80, (uint16_t)(50100 + k),
                         t->rcv_nxt, 0, 0x04, 0, 0);
            red_tcp_entrada(kat_frm, n);
            if (t->estado != TCP_CERRADO || t->err != -9) ok = 0;
        }
        if (red_tcp_limpia_proceso(9999) != 4) ok = 0;
        for (i = 0; i < TCP_N; i++) {
            if (tcbs[i].estado != TCP_CERRADO || tcbs[i].err) ok = 0;
        }
        {
            int a = red_tcp_escuchar(62001, 9999);
            int b = red_tcp_escuchar(62002, 9999);
            int c = red_tcp_escuchar(62003, 9999);
            int d = red_tcp_escuchar(62004, 9999);
            if (a <= 0 || b <= 0 || c <= 0 || d <= 0) ok = 0;
            if (red_tcp_limpia_proceso(9999) != 4) ok = 0;
        }
    }
    /* 8. F6-B D3: id antiguo rechazado tras reutilizar slot. */
    kat_n = 0;
    {
        int id1 = abre_nb(KAT_PAR, 83, 50110, 9999);
        if (id1 <= 0) ok = 0;
        else {
            int idx = id1 % TCP_N;
            int id2;
            libera(&tcbs[idx]);
            id2 = abre_nb(KAT_PAR, 84, 50111, 9999);
            if (id2 <= 0) ok = 0;
            else {
                if (red_tcp_estado(id1, 9999) != -2) ok = 0;
                red_tcp_abortar(id2, 9999);
            }
        }
    }
    /* 9. F6-B D4: re-Tx de FIN usa la misma secuencia. */
    kat_n = 0;
    {
        int id = kat_conecta(50200, 80, 9100);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            uint32_t fseq;
            int n0;
            if (fin_nb(t) != 0) ok = 0;
            else {
                fseq = t->snd_nxt - 1;
                if (kat_n < 1) ok = 0;
                else if (kat_r[kat_n - 1].band != 0x11) ok = 0;
                else if (kat_r[kat_n - 1].seq != fseq) ok = 0;
                n0 = kat_n;
                t->t0 -= 100000;
                tcp_reloj();
                if (kat_n != n0 + 1) ok = 0;
                else if (kat_r[kat_n - 1].band != 0x11) ok = 0;
                else if (kat_r[kat_n - 1].seq != fseq) ok = 0;
            }
            libera(t);
        }
    }
    /* 10. F6-B D5: FIN duplicado re-acusa sin avanzar. */
    kat_n = 0;
    {
        int id = kat_conecta(50201, 80, 9101);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            uint32_t rn;
            int n, k0;
            rn = t->rcv_nxt;
            n = kat_arma(KAT_PAR, 80, 50201, rn, t->snd_nxt,
                         0x11, 0, 0);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (!t->fin_rx) ok = 0;
            if (kat_n != k0 + 1) ok = 0;
            rn = t->rcv_nxt;
            n = kat_arma(KAT_PAR, 80, 50201, rn - 1, t->snd_nxt,
                         0x11, 0, 0);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (t->rcv_nxt != rn) ok = 0;
            if (kat_n != k0 + 1) ok = 0;
            else if (kat_r[kat_n - 1].band != 0x10) ok = 0;
            libera(t);
        }
    }
    /* 11. F6-B D8: backlog lleno no responde RST. */
    kat_n = 0;
    {
        int esc = red_tcp_escuchar(62010, 9999);
        if (esc <= 0) ok = 0;
        else {
            int n, k0;
            n = kat_arma(KAT_PAR, 4567, 62010, 3000, 0, 0x02, 0, 0);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (kat_n != k0 + 1) ok = 0;
            n = kat_arma(KAT_PAR, 4568, 62010, 4000, 0, 0x02, 0, 0);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (kat_n != k0) ok = 0;
            {
                tcp_t *q = &tcbs[esc % TCP_N];
                if (q->pendiente >= 0 && q->pendiente < TCP_N)
                    libera(&tcbs[q->pendiente]);
                q->pendiente = -1;
                red_tcp_cerrar(esc, 9999);
            }
        }
    }
    /* 12. F6-B D11: ACK viejo con datos validos no descarta. */
    kat_n = 0;
    {
        static const uint8_t dd[2] = { 'X', 'Y' };
        int id = kat_conecta(50202, 80, 9102);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            uint32_t una = t->snd_una, nxt = t->snd_nxt;
            uint32_t rcv = t->rcv_nxt;
            int n, k0;
            uint8_t tmp[8];
            n = kat_arma(KAT_PAR, 80, 50202, rcv, una - 1, 0x18,
                         dd, 2);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (t->rcv_n != 2) ok = 0;
            if (t->rcv_nxt != rcv + 2) ok = 0;
            if (kat_n != k0 + 1) ok = 0;
            if (red_tcp_recibir(id, 9999, tmp, 8, 0) != 2) ok = 0;
            rcv = t->rcv_nxt;
            n = kat_arma(KAT_PAR, 80, 50202, rcv, nxt + 100, 0x18,
                         dd, 2);
            {
                uint16_t rn0 = t->rcv_n;
                red_tcp_entrada(kat_frm, n);
                if (t->rcv_n != rn0) ok = 0;
            }
            libera(t);
        }
    }
    /* 13. F6-C D6: datos tras FIN propio (sin FIN ajeno). */
    kat_n = 0;
    {
        static const uint8_t dd[2] = { 'Q', 'W' };
        int id = kat_conecta(50210, 80, 9200);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            uint32_t rcv;
            int n, k0;
            uint8_t tmp[8];
            if (fin_nb(t) != 0) ok = 0;
            rcv = t->rcv_nxt;
            n = kat_arma(KAT_PAR, 80, 50210, rcv, t->snd_nxt,
                         0x18, dd, 2);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (t->rcv_n != 2) ok = 0;
            if (t->rcv_nxt != rcv + 2) ok = 0;
            if (kat_n != k0 + 1) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50210, rcv, t->snd_nxt,
                         0x18, dd, 2);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (t->rcv_n != 2) ok = 0;
            if (kat_n != k0 + 1) ok = 0;
            if (red_tcp_recibir(id, 9999, tmp, 8, 0) != 2) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50210, rcv + 2, t->snd_nxt,
                         0x19, dd, 2);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (t->rcv_n != 2) ok = 0;
            if (!t->fin_rx) ok = 0;
            if (kat_n != k0 + 2) ok = 0;   /* ACK datos + ACK FIN */
            libera(t);
        }
    }
    /* 14. F6-C D7: ventana llena anuncia 0 y la sonda responde. */
    kat_n = 0;
    {
        static uint8_t kre[536];
        int id = kat_conecta(50211, 80, 9201);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            uint32_t rcv = t->rcv_nxt;
            int n, k, k0;
            uint8_t tmp[536];
            int got = 0, q;
            for (k = 0; k < 536; k++) kre[k] = (uint8_t)k;
            for (k = 0; k < 3; k++) {
                n = kat_arma(KAT_PAR, 80, 50211, rcv, t->snd_nxt,
                             0x18, kre, 536);
                red_tcp_entrada(kat_frm, n);
                rcv += 536;
            }
            n = kat_arma(KAT_PAR, 80, 50211, rcv, t->snd_nxt,
                         0x18, kre, 440);
            red_tcp_entrada(kat_frm, n);
            if (t->rcv_n != TCP_RCV) ok = 0;
            if (kat_n < 1) ok = 0;
            else if (kat_r[kat_n - 1].wnd != 0) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50211, t->rcv_nxt, t->snd_nxt,
                         0x18, kre, 1);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (kat_n != k0 + 1) ok = 0;
            else {
                if (kat_r[kat_n - 1].band != 0x10) ok = 0;
                if (kat_r[kat_n - 1].ackn != t->rcv_nxt) ok = 0;
                if (kat_r[kat_n - 1].wnd != 0) ok = 0;
            }
            if (t->rcv_n != TCP_RCV) ok = 0;
            /* drenado total: la sonda cabe y la ventana reabre
               (2048-1 tras consumir la sonda). */
            while (t->rcv_n) {
                q = red_tcp_recibir(id, 9999, tmp, sizeof(tmp), 0);
                if (q <= 0) break;
                got += q;
            }
            if (got != TCP_RCV) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50211, t->rcv_nxt, t->snd_nxt,
                         0x18, kre, 1);
            k0 = kat_n;
            red_tcp_entrada(kat_frm, n);
            if (kat_n != k0 + 1) ok = 0;
            else if (kat_r[kat_n - 1].wnd != TCP_RCV - 1) ok = 0;
            libera(t);
        }
    }
    /* 15. F6-C D9: libera sanea controles y cargas. */
    kat_n = 0;
    {
        static const uint8_t abc[3] = { 'a', 'b', 'c' };
        int id = kat_conecta(50212, 80, 9202);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            int n = kat_arma(KAT_PAR, 80, 50212, t->rcv_nxt,
                             t->snd_nxt, 0x18, abc, 3);
            red_tcp_entrada(kat_frm, n);
            if (envia_nb(t, abc, 3) != 0) ok = 0;
            libera(t);
            if (t->ip_rem || t->pto_loc || t->pto_rem || t->iss ||
                t->irs || t->snd_nxt || t->snd_una || t->rcv_nxt ||
                t->wnd_par || t->snd_len || t->rcv_ini || t->rcv_n ||
                t->fin_rx || t->fin_ack || t->reint || t->t0 ||
                t->snd[0] || t->rcv[0]) ok = 0;
            if (t->rto != RTO0) ok = 0;
            {
                int id2 = abre_nb(KAT_PAR, 85, 50112, 9999);
                if (id2 <= 0) ok = 0;
                else {
                    tcp_t *u = &tcbs[id2 % TCP_N];
                    /* Recien abierto: cargas y pendientes a cero;
                       ip_rem/pto_rem/pto_loc/iss asignados. */
                    if (u->snd_len || u->rcv_n || u->fin_rx ||
                        u->fin_ack || u->snd[0] || u->rcv[0]) ok = 0;
                    if (u->pto_loc != 50112 || u->iss == 0) ok = 0;
                    if (u->ip_rem != KAT_PAR || u->pto_rem != 85) ok = 0;
                    red_tcp_abortar(id2, 9999);
                }
            }
        }
    }
    /* 16. F6-C D10: RST a cerrado con ACK usa seq=ackn sin ACK. */
    kat_n = 0;
    {
        int n, kat0b = kat_n;
        n = kat_arma(KAT_PAR, 80, 61998, 5000, 12345, 0x10, 0, 0);
        red_tcp_entrada(kat_frm, n);
        if (kat_n != kat0b + 1) ok = 0;
        else {
            if (kat_r[kat_n - 1].band != 0x04) ok = 0;
            if (kat_r[kat_n - 1].seq != 12345) ok = 0;
        }
        n = kat_arma(KAT_PAR, 80, 61997, 1000, 0, 0x02, 0, 0);
        kat0b = kat_n;
        red_tcp_entrada(kat_frm, n);
        if (kat_n != kat0b + 1) ok = 0;
        else {
            if (kat_r[kat_n - 1].band != 0x14) ok = 0;
            if (kat_r[kat_n - 1].ackn != 1001) ok = 0;
        }
    }
    /* 17. F6-C D12: se respeta wnd_par; SYN-ACK la ensena. */
    kat_n = 0;
    {
        static uint8_t zx[100];
        int id = kat_conecta(50213, 80, 9203);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            int n, k0, k;
            for (k = 0; k < 100; k++) zx[k] = (uint8_t)k;
            if (t->wnd_par != 0x0800) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50213, t->rcv_nxt, t->snd_nxt,
                         0x10, 0, 0);
            kat_pon_wnd(0);
            red_tcp_entrada(kat_frm, n);
            if (t->wnd_par != 0) ok = 0;
            k0 = kat_n;
            if (envia_nb(t, zx, 100) != -8) ok = 0;
            if (kat_n != k0) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50213, t->rcv_nxt, t->snd_nxt,
                         0x10, 0, 0);
            kat_pon_wnd(10);
            red_tcp_entrada(kat_frm, n);
            if (envia_nb(t, zx, 100) != -8) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50213, t->rcv_nxt, t->snd_nxt,
                         0x10, 0, 0);
            kat_pon_wnd(1000);
            red_tcp_entrada(kat_frm, n);
            k0 = kat_n;
            if (envia_nb(t, zx, 100) != 0) ok = 0;
            if (kat_n != k0 + 1) ok = 0;
            else if (kat_r[kat_n - 1].nd != 100) ok = 0;
            n = kat_arma(KAT_PAR, 80, 50213, t->rcv_nxt, t->snd_nxt,
                         0x10, 0, 0);
            red_tcp_entrada(kat_frm, n);
            libera(t);
        }
        {
            int ida = abre_nb(KAT_PAR, 86, 50113, 9999);
            if (ida <= 0) ok = 0;
            else {
                tcp_t *t = &tcbs[ida % TCP_N];
                int n;
                if (t->wnd_par != TCP_MSS) ok = 0;
                n = kat_arma(KAT_PAR, 86, 50113, 4000, t->iss + 1,
                             0x12, 0, 0);
                kat_pon_wnd(0);
                red_tcp_entrada(kat_frm, n);
                if (t->estado != TCP_ESTABLECIDO) ok = 0;
                else if (t->wnd_par != 0) ok = 0;
                libera(t);
            }
        }
    }
    /* 18. F6-C D13: espera 0 no bloquea. */
    kat_n = 0;
    {
        int esc = red_tcp_escuchar(62020, 9999);
        if (esc <= 0) ok = 0;
        else {
            uint32_t a = irq_ticks();
            int r = red_tcp_aceptar(esc, 9999, 0);
            uint32_t d = irq_ticks() - a;
            if (r != 0) ok = 0;
            if (d > 1) ok = 0;
            red_tcp_cerrar(esc, 9999);
        }
        {
            int id = kat_conecta(50214, 80, 9204);
            if (id <= 0) ok = 0;
            else {
                uint8_t tmp[8];
                uint32_t a, d;
                int r;
                a = irq_ticks();
                r = red_tcp_recibir(id, 9999, tmp, sizeof(tmp), 0);
                d = irq_ticks() - a;
                if (r != 0) ok = 0;
                if (d > 1) ok = 0;
                libera(&tcbs[id % TCP_N]);
            }
        }
    }
    /* 19. F6-C D14: cerrar con datos sin ACK no manda FIN con
       hueco: aborta con RST y devuelve -7. */
    kat_n = 0;
    {
        static const uint8_t abc[3] = { 'a', 'b', 'c' };
        int id = kat_conecta(50215, 80, 9205);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            int k0, k, r, vio_fin = 0, vio_rst = 0;
            if (envia_nb(t, abc, 3) != 0) ok = 0;
            k0 = kat_n;
            r = cierra_descarta(t);
            if (r != -7) ok = 0;
            if (t->estado != TCP_CERRADO) ok = 0;
            for (k = k0; k < kat_n; k++) {
                if (kat_r[k].band == 0x11) vio_fin = 1;
                if (kat_r[k].band == 0x14) vio_rst = 1;
            }
            if (vio_fin) ok = 0;
            if (!vio_rst) ok = 0;
        }
    }
    /* 20. F6-C D15: cuarentena de tupla tras cierre. */
    kat_n = 0;
    {
        int id, k;
        /* Higiene: la gracia es best-effort con 4 entradas; se parte
           de tabla vacia para no desalojar lo probado. */
        for (k = 0; k < TUP_N; k++) tups[k].hasta = 0;
        id = kat_conecta(50216, 80, 9206);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            uint32_t rip = t->ip_rem;
            uint16_t rpo = t->pto_rem, lpo = t->pto_loc;
            int id2, n, k0, k;
            cierra_guarda(t);
            libera(t);
            /* misma tupla con puerto local explicito: -6 */
            id2 = abre_nb(rip, rpo, lpo, 9999);
            if (id2 != -6) ok = 0;
            if (id2 > 0) red_tcp_abortar(id2, 9999);
            /* tupla distinta: sin bloqueo */
            id2 = abre_nb(rip, 87, 50114, 9999);
            if (id2 <= 0) ok = 0;
            else red_tcp_abortar(id2, 9999);
            /* SYN pasivo a la tupla en gracia: gota */
            {
                int esc = red_tcp_escuchar(lpo, 9999);
                if (esc <= 0) ok = 0;
                else {
                    n = kat_arma(rip, rpo, lpo, 5000, 0, 0x02, 0, 0);
                    k0 = kat_n;
                    red_tcp_entrada(kat_frm, n);
                    if (kat_n != k0) ok = 0;
                    red_tcp_cerrar(esc, 9999);
                }
            }
            /* caduca: reutilizable */
            for (k = 0; k < TUP_N; k++) tups[k].hasta = 0;
            id2 = abre_nb(rip, rpo, lpo, 9999);
            if (id2 <= 0) ok = 0;
            else red_tcp_abortar(id2, 9999);
        }
    }
    /* 21. F6-C reloj: tcp_reloj avanza con red_bombea. */
    kat_n = 0;
    {
        static const uint8_t abc[3] = { 'a', 'b', 'c' };
        int id = kat_conecta(50217, 80, 9207);
        if (id <= 0) ok = 0;
        else {
            tcp_t *t = &tcbs[id % TCP_N];
            int k0;
            if (envia_nb(t, abc, 3) != 0) ok = 0;
            t->t0 -= 1000;
            k0 = kat_n;
            red_bombea();
            if (t->reint != 1) ok = 0;
            if (kat_n != k0 + 1) ok = 0;
            libera(t);
        }
    }
    kat_on = 0;
    kat_n = 0;
    kprintf("red: KAT tcp %s\r\n", ok ? "OK" : "FALLO");
}
