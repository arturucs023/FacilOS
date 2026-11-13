/* imgdec: PNG + JPEG (baseline) propios, freestanding.
 *
 * Sin UNIX/BSD/Linux/Windows: aritmetica entera, sin libc, sin FPU.
 * PNG: firma, IHDR, PLTE, tRNS, IDAT (concatena), IEND; tipos 0/2/3/4/6
 * (prof. 1/2/4/8, y 16 con truncado al byte alto); sin entrelazado.
 * JPEG: baseline 8 bits (SOF0), 1 o 3 componentes, muestreos 1x1/2x2/2x1
 * (y generico 1..4), tablas 8/16 bits, reinicios DRI, YCbCr->RGB.
 * Progresivo (SOF2) no soportado.
 *
 * Limites duros en todos los bucles (datos web hostiles).
 */

#ifdef IMGDEC_HOST
#include <stdlib.h>
#define IMALLOC(n) malloc((n))
#define IFREE(p) free((p))
#else
void *u_malloc(int n);
void u_free(void *p);
#define IMALLOC(n) u_malloc((n))
#define IFREE(p) u_free((p))
#endif

/* ---------- utilidades ---------- */

static unsigned im_u32(const unsigned char *p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8) | (unsigned)p[3];
}

static unsigned im_u16(const unsigned char *p) {
    return ((unsigned)p[0] << 8) | (unsigned)p[1];
}

/* ---------- lector de bits LSB (deflate) ---------- */

typedef struct {
    const unsigned char *d;
    int n, pos;
    unsigned bits;   /* acumulador */
    int nbits;       /* validos en bits */
    int mal;         /* 1 si se leyo mas alla */
} IMBR;

static void imbr_ini(IMBR *b, const unsigned char *d, int n) {
    b->d = d;
    b->n = n;
    b->pos = 0;
    b->bits = 0;
    b->nbits = 0;
    b->mal = 0;
}

static void imbr_mas(IMBR *b) {
    while (b->nbits <= 24) {
        if (b->pos >= b->n) {
            b->mal = 1;
            return;
        }
        b->bits |= (unsigned)b->d[b->pos++] << b->nbits;
        b->nbits += 8;
    }
}

/* asegura k bits (k<=15); 0 ok */
static int imbr_ver(IMBR *b, int k) {
    if (b->nbits < k) {
        imbr_mas(b);
        if (b->mal && b->nbits < k) return -1;
    }
    return 0;
}

static unsigned imbr_verbits(IMBR *b, int k) {
    return b->bits & (k >= 32 ? 0xFFFFFFFFu : ((1u << k) - 1u));
}

static void imbr_quita(IMBR *b, int k) {
    b->bits >>= k;
    b->nbits -= k;
}

/* ---------- Huffman generico (rapido 8 bits + arbol) ---------- */

#define IMH_MAXS 288
#define IMH_TNODOS 600

typedef struct {
    int rapido[256];            /* (len<<9)|sim, o -1 */
    int arbol[IMH_TNODOS * 2];  /* hijo0,hijo1; hoja = ~sim */
    int nodos;                  /* siguiente nodo libre (0 = raiz) */
} IMH;

/* revierte los 'len' bits bajos de c */
static unsigned im_rev(unsigned c, int len) {
    unsigned r = 0;
    int i;
    for (i = 0; i < len; i++) {
        r = (r << 1) | (c & 1u);
        c >>= 1;
    }
    return r;
}

/* lens[s] = longitud (0 = sin codigo). 0 ok. */
static int imh_construye(IMH *h, const unsigned char *lens, int ns) {
    int cuenta[16], i, len, cod = 0, prox[16], k;
    for (i = 0; i < 256; i++) h->rapido[i] = -1;
    for (i = 0; i < IMH_TNODOS * 2; i++) h->arbol[i] = -9999;
    h->nodos = 1;
    for (i = 0; i < 16; i++) cuenta[i] = 0;
    for (i = 0; i < ns; i++)
        if (lens[i] > 0 && lens[i] < 16) cuenta[lens[i]]++;
    for (len = 1; len < 16; len++) {
        prox[len] = cod;
        cod = (cod + cuenta[len]) << 1;
    }
    {   /* completez estilo puff: nunca sobre-suscrito */
        int izq = 1, ln;
        for (ln = 1; ln < 16; ln++) {
            izq <<= 1;
            izq -= cuenta[ln];
            if (izq < 0) return -1;
        }
    }
    for (i = 0; i < ns; i++) {
        int sim = i;
        unsigned c;
        len = lens[i];
        if (!len) continue;
        c = (unsigned)prox[len]++;
        c = im_rev(c, len);  /* LSB primero */
        if (len <= 8) {
            int paso = 1 << len, j;
            for (j = (int)c; j < 256; j += paso)
                h->rapido[j] = (len << 9) | sim;
        } else {
            /* inserta bit a bit (LSB primero) */
            int nodo = 0, j;
            for (j = 0; j < len; j++) {
                int bit = (int)((c >> j) & 1u), sig;
                if (j == len - 1) {
                    h->arbol[nodo * 2 + bit] = ~sim;
                    break;
                }
                sig = h->arbol[nodo * 2 + bit];
                if (sig == -9999) {
                    if (h->nodos >= IMH_TNODOS) return -1;
                    sig = h->nodos++;
                    h->arbol[nodo * 2 + bit] = sig;
                    h->arbol[sig * 2] = -9999;
                    h->arbol[sig * 2 + 1] = -9999;
                } else if (sig < 0) {
                    return -1;  /* prefijo de otro */
                }
                nodo = sig;
            }
        }
    }
    /* Sin inicializar raices ausentes: quedan -9999 y el camino
       lento las rechaza (incompleto solo vale si el flujo no
       usa esos prefijos). */
    (void)k;
    return 0;
}

/* decodifica un simbolo; -1 error */
static int imh_dec(IMBR *b, IMH *h) {
    unsigned x;
    int v;
    if (imbr_ver(b, 8)) return -1;
    x = imbr_verbits(b, 8);
    v = h->rapido[x];
    if (v >= 0) {
        imbr_quita(b, v >> 9);
        return v & 511;
    }
    /* lento: camina el arbol */
    {
        int nodo = 0;
        for (;;) {
            int bit, sig;
            if (imbr_ver(b, 1)) return -1;
            bit = (int)imbr_verbits(b, 1);
            imbr_quita(b, 1);
            if (nodo < 0 || nodo >= IMH_TNODOS) return -1;
            sig = h->arbol[nodo * 2 + bit];
            if (sig == -9999) return -1;
            if (sig < 0) {
                int s = ~sig;
                if (s < 0 || s >= IMH_MAXS) return -1;
                return s;
            }
            nodo = sig;
        }
    }
}

/* ---------- inflate a bufer exacto ---------- */

static const short IM_LBASE[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const unsigned char IM_LEXT[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const short IM_DBASE[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};
static const unsigned char IM_DEXT[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static int im_inflate(const unsigned char *d, int n,
                      unsigned char *sal, int cap) {
    IMBR b;
    int fin = 0, o = 0;
    IMH fijo_l = { { 0 } }, fijo_d = { { 0 } };
    unsigned char lf[288], df[32];
    int i, hecho = 0;
    /* tablas fijas (una vez) */
    for (i = 0; i < 288; i++)
        lf[i] = (unsigned char)(i < 144 ? 8 : i < 256 ? 9 :
                               i < 280 ? 7 : 8);
    for (i = 0; i < 32; i++) df[i] = 5;
    if (imh_construye(&fijo_l, lf, 288) ||
        imh_construye(&fijo_d, df, 32))
        return -1;
    imbr_ini(&b, d, n);
    while (!fin) {
        int tipo, ultimo;
        IMH *hl = 0, *hd = 0;
        IMH din_l = { { 0 } }, din_d = { { 0 } };
        if (imbr_ver(&b, 3)) return -1;
        ultimo = (int)imbr_verbits(&b, 1);
        imbr_quita(&b, 1);
        tipo = (int)imbr_verbits(&b, 2);
        imbr_quita(&b, 2);
        fin = ultimo;
        if (tipo == 0) {
            /* almacenado: alinea a byte */
            int len, nlen;
            b.bits = 0;
            b.nbits = 0;
            if (b.pos + 4 > b.n) return -1;
            len = b.d[b.pos] | (b.d[b.pos + 1] << 8);
            nlen = b.d[b.pos + 2] | (b.d[b.pos + 3] << 8);
            b.pos += 4;
            if ((len ^ nlen) != 0xFFFF) return -1;
            if (b.pos + len > b.n) return -1;
            if (o + len > cap) return -1;
            for (i = 0; i < len; i++) sal[o++] = b.d[b.pos++];
            continue;
        }
        if (tipo == 1) {
            hl = &fijo_l;
            hd = &fijo_d;
        } else if (tipo == 2) {
            static const unsigned char ORD[19] = {
                16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                11, 4, 12, 3, 13, 2, 14, 1, 15
            };
            unsigned char lc[19], ll[288], dd[32], tmp[320];
            int hlit, hdist, hclen, k = 0;
            if (imbr_ver(&b, 14)) return -1;
            hlit = (int)imbr_verbits(&b, 5) + 257;
            imbr_quita(&b, 5);
            hdist = (int)imbr_verbits(&b, 5) + 1;
            imbr_quita(&b, 5);
            hclen = (int)imbr_verbits(&b, 4) + 4;
            imbr_quita(&b, 4);
            if (hlit > 288 || hdist > 32) return -1;
            for (i = 0; i < 19; i++) lc[i] = 0;
            for (i = 0; i < hclen; i++) {
                if (imbr_ver(&b, 3)) return -1;
                lc[ORD[i]] = (unsigned char)imbr_verbits(&b, 3);
                imbr_quita(&b, 3);
            }
            {
                IMH hc = { { 0 } };
                if (imh_construye(&hc, lc, 19)) return -1;
                while (k < hlit + hdist) {
                    int s = imh_dec(&b, &hc);
                    if (s < 0 || s > 18) return -1;
                    if (s < 16) {
                        if (k >= hlit + hdist) return -1;
                        tmp[k++] = (unsigned char)s;
                    } else {
                        int rep = 0, val = 0;
                        if (s == 16) {
                            if (!k || imbr_ver(&b, 2)) return -1;
                            rep = (int)imbr_verbits(&b, 2) + 3;
                            imbr_quita(&b, 2);
                            val = tmp[k - 1];
                        } else if (s == 17) {
                            if (imbr_ver(&b, 3)) return -1;
                            rep = (int)imbr_verbits(&b, 3) + 3;
                            imbr_quita(&b, 3);
                        } else {
                            if (imbr_ver(&b, 7)) return -1;
                            rep = (int)imbr_verbits(&b, 7) + 11;
                            imbr_quita(&b, 7);
                        }
                        if (k + rep > hlit + hdist) return -1;
                        while (rep--) tmp[k++] = (unsigned char)val;
                    }
                }
            }
            for (i = 0; i < hlit; i++) ll[i] = tmp[i];
            for (i = 0; i < hdist; i++) dd[i] = tmp[hlit + i];
            if (imh_construye(&din_l, ll, hlit) ||
                imh_construye(&din_d, dd, hdist))
                return -1;
            hl = &din_l;
            hd = &din_d;
        } else {
            return -1;
        }
        for (;;) {
            int s = imh_dec(&b, hl);
            if (s < 0) return -1;
            if (s < 256) {
                if (o >= cap) return -1;
                sal[o++] = (unsigned char)s;
            } else if (s == 256) {
                break;
            } else {
                int li = s - 257, di, lon, dis, k2;
                unsigned e;
                if (li < 0 || li > 28) return -1;
                lon = IM_LBASE[li];
                if (IM_LEXT[li]) {
                    if (imbr_ver(&b, IM_LEXT[li])) return -1;
                    e = imbr_verbits(&b, IM_LEXT[li]);
                    imbr_quita(&b, IM_LEXT[li]);
                    lon += (int)e;
                }
                di = imh_dec(&b, hd);
                if (di < 0 || di > 29) return -1;
                dis = IM_DBASE[di];
                if (IM_DEXT[di]) {
                    if (imbr_ver(&b, IM_DEXT[di])) return -1;
                    e = imbr_verbits(&b, IM_DEXT[di]);
                    imbr_quita(&b, IM_DEXT[di]);
                    dis += (int)e;
                }
                if (dis <= 0 || dis > o) return -1;
                if (o + lon > cap) return -1;
                for (k2 = 0; k2 < lon; k2++)
                    sal[o] = sal[o - dis], o++;
            }
        }
    }
    hecho = o;
    (void)hecho;
    return o;
}

static unsigned im_adler(const unsigned char *d, int n) {
    unsigned a = 1, m = 0;
    int i;
    for (i = 0; i < n; i++) {
        a += d[i];
        m += a;
        if ((i & 1023) == 1023) {
            a %= 65521u;
            m %= 65521u;
        }
    }
    return ((m % 65521u) << 16) | (a % 65521u);
}

/* ---------- PNG ---------- */

static int im_paeth(int a, int b, int c) {
    int p = a + b - c, pa = p - a, pb = p - b, pc = p - c;
    if (pa < 0) pa = -pa;
    if (pb < 0) pb = -pb;
    if (pc < 0) pc = -pc;
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

int imgdec_png(const unsigned char *d, int n, int maxpx,
               int *w, int *h, unsigned char **rgb) {
    static const unsigned char firma[8] = {
        137, 80, 78, 71, 13, 10, 26, 10
    };
    int i, pos = 8, ancho = 0, alto = 0, prof = 0, tipo = -1;
    int entrel = -1, idlen = 0, idcap = 0;
    unsigned char *idat = 0;
    unsigned char pal[256 * 3];
    unsigned char trns[256];
    int npal = 0, ntrns = 0;
    int stride, fil;
    unsigned char *crudo = 0, *prev = 0, *dst = 0;
    *w = 0;
    *h = 0;
    *rgb = 0;
    if (!d || n < 8) return -1;
    for (i = 0; i < 8; i++)
        if (d[i] != firma[i]) return -1;
    for (i = 0; i < 256; i++) trns[i] = 255;
    while (pos + 8 <= n) {
        unsigned len = im_u32(d + pos);
        unsigned tp = im_u32(d + pos + 4);
        if (len > (unsigned)(n - pos - 12)) return -1;
        if (tp == 0x49484452u) {           /* IHDR */
            if (len != 13) return -1;
            ancho = (int)im_u32(d + pos + 8);
            alto = (int)im_u32(d + pos + 12);
            prof = d[pos + 16];
            tipo = d[pos + 17];
            entrel = d[pos + 20];
            if (ancho <= 0 || alto <= 0 || ancho > 2000 ||
                alto > 2000)
                return -1;
            if ((long)ancho * (long)alto > (long)maxpx) return -1;
            if (d[pos + 18] != 0 || d[pos + 19] != 0) return -1;
            if (entrel != 0) return -1;    /* sin Adam7 */
            if (!((tipo == 0 && (prof == 1 || prof == 2 ||
                                 prof == 4 || prof == 8 ||
                                 prof == 16)) ||
                  (tipo == 2 && (prof == 8 || prof == 16)) ||
                  (tipo == 3 && (prof == 1 || prof == 2 ||
                                 prof == 4 || prof == 8)) ||
                  (tipo == 4 && prof == 8) ||
                  (tipo == 6 && (prof == 8 || prof == 16))))
                return -1;
        } else if (tp == 0x504C5445u) {    /* PLTE */
            if (len > 768 || (len % 3)) return -1;
            for (i = 0; i < (int)len; i++)
                pal[i] = d[pos + 8 + i];
            npal = (int)len / 3;
        } else if (tp == 0x74424E53u) {    /* tRNS */
            if (tipo == 3) {
                if (len > 256) return -1;
                for (i = 0; i < (int)len; i++)
                    trns[i] = d[pos + 8 + i];
                ntrns = (int)len;
            }
            /* tipos 0/2: se ignoran (fondo blanco) */
        } else if (tp == 0x49444154u) {    /* IDAT */
            if (!idat) {
                idcap = (int)len + 16;
                if (idcap < 1024) idcap = 1024;
                idat = (unsigned char *)IMALLOC(idcap);
                if (!idat) return -1;
            } else if (idlen + (int)len > idcap) {
                int nc = idcap * 2 + (int)len;
                unsigned char *q;
                if (nc > (1 << 20)) {
                    IFREE(idat);
                    return -1;
                }
                q = (unsigned char *)IMALLOC(nc);
                if (!q) {
                    IFREE(idat);
                    return -1;
                }
                for (i = 0; i < idlen; i++) q[i] = idat[i];
                IFREE(idat);
                idat = q;
                idcap = nc;
            }
            for (i = 0; i < (int)len; i++)
                idat[idlen++] = d[pos + 8 + i];
        } else if (tp == 0x49454E44u) {    /* IEND */
            break;
        }
        pos += 12 + (int)len;
    }
    if (!ancho || !alto || tipo < 0 || !idat || !idlen) {
        if (idat) IFREE(idat);
        return -1;
    }
    if (tipo == 3 && !npal) {
        IFREE(idat);
        return -1;
    }
    /* bytes de muestra por fila */
    {
        int bpp;  /* bits por pixel */
        if (tipo == 0) bpp = prof;
        else if (tipo == 2) bpp = prof * 3;
        else if (tipo == 3) bpp = prof;
        else if (tipo == 4) bpp = prof * 2;
        else bpp = prof * 4;
        stride = (ancho * bpp + 7) / 8;
    }
    fil = stride + 1;
    if ((long)fil * (long)alto > (1 << 24)) {
        IFREE(idat);
        return -1;
    }
    crudo = (unsigned char *)IMALLOC(fil * alto);
    if (!crudo) {
        IFREE(idat);
        return -1;
    }
    /* zlib: cabecera de 2 + datos + adler32 */
    if (idlen < 6) {
        IFREE(idat);
        IFREE(crudo);
        return -1;
    }
    {
        int rc = im_inflate(idat + 2, idlen - 6, crudo, fil * alto);
        unsigned ad;
        if (rc != fil * alto) {
            IFREE(idat);
            IFREE(crudo);
            return -1;
        }
        ad = ((unsigned)idat[idlen - 4] << 24) |
             ((unsigned)idat[idlen - 3] << 16) |
             ((unsigned)idat[idlen - 2] << 8) |
             (unsigned)idat[idlen - 1];
        if (ad != im_adler(crudo, fil * alto)) {
            IFREE(idat);
            IFREE(crudo);
            return -1;
        }
    }
    IFREE(idat);
    idat = 0;
    prev = (unsigned char *)IMALLOC(stride > 0 ? stride : 1);
    dst = (unsigned char *)IMALLOC((unsigned)(ancho * alto * 3));
    if (!prev || !dst) {
        if (prev) IFREE(prev);
        if (dst) IFREE(dst);
        IFREE(crudo);
        return -1;
    }
    for (i = 0; i < stride; i++) prev[i] = 0;
    /* desfiltra + convierte por fila */
    {
        int y, x;
        for (y = 0; y < alto; y++) {
            unsigned char *f = crudo + y * fil;
            int ft = f[0], xx;
            unsigned char *cur = f + 1;
            int bppb;  /* bytes por pixel para Sub (min 1) */
            {
                int bpp;
                if (tipo == 0) bpp = prof;
                else if (tipo == 2) bpp = prof * 3;
                else if (tipo == 3) bpp = prof;
                else if (tipo == 4) bpp = prof * 2;
                else bpp = prof * 4;
                bppb = (bpp + 7) / 8;
                if (bppb < 1) bppb = 1;
            }
            if (ft > 4) {
                IFREE(prev);
                IFREE(dst);
                IFREE(crudo);
                return -1;
            }
            for (xx = 0; xx < stride; xx++) {
                int a = xx >= bppb ? cur[xx - bppb] : 0;
                int b2 = prev[xx], c = xx >= bppb ? prev[xx - bppb] : 0;
                int v = cur[xx];
                if (ft == 1) v += a;
                else if (ft == 2) v += b2;
                else if (ft == 3) v += (a + b2) >> 1;
                else if (ft == 4) v += im_paeth(a, b2, c);
                cur[xx] = (unsigned char)(v & 0xFF);
            }
            for (xx = 0; xx < stride; xx++) prev[xx] = cur[xx];
            /* convierte fila a RGB */
            for (x = 0; x < ancho; x++) {
                int r = 0, g = 0, bb = 0, al = 255;
                if (tipo == 0) {
                    if (prof == 16) {
                        r = g = bb = cur[x * 2];
                    } else if (prof == 8) {
                        r = g = bb = cur[x];
                    } else {
                        int m = (1 << prof) - 1, v2;
                        v2 = (cur[x * prof / 8] >>
                              (8 - prof - (x % (8 / prof)) * prof)) &
                             m;
                        r = g = bb = (v2 * 255) / m;
                    }
                } else if (tipo == 2) {
                    if (prof == 16) {
                        r = cur[x * 6];
                        g = cur[x * 6 + 2];
                        bb = cur[x * 6 + 4];
                    } else {
                        r = cur[x * 3];
                        g = cur[x * 3 + 1];
                        bb = cur[x * 3 + 2];
                    }
                } else if (tipo == 3) {
                    int idx;
                    if (prof == 8) {
                        idx = cur[x];
                    } else {
                        int m = (1 << prof) - 1;
                        idx = (cur[x * prof / 8] >>
                               (8 - prof - (x % (8 / prof)) * prof)) &
                              m;
                    }
                    if (idx >= npal) {
                        IFREE(prev);
                        IFREE(dst);
                        IFREE(crudo);
                        return -1;
                    }
                    r = pal[idx * 3];
                    g = pal[idx * 3 + 1];
                    bb = pal[idx * 3 + 2];
                    if (idx < ntrns) al = trns[idx];
                } else if (tipo == 4) {
                    r = g = bb = cur[x * 2];
                    al = cur[x * 2 + 1];
                } else { /* tipo 6 */
                    if (prof == 16) {
                        r = cur[x * 8];
                        g = cur[x * 8 + 2];
                        bb = cur[x * 8 + 4];
                        al = cur[x * 8 + 6];
                    } else {
                        r = cur[x * 4];
                        g = cur[x * 4 + 1];
                        bb = cur[x * 4 + 2];
                        al = cur[x * 4 + 3];
                    }
                }
                if (al != 255) {
                    /* mezcla sobre blanco */
                    r = (al * r + (255 - al) * 255) / 255;
                    g = (al * g + (255 - al) * 255) / 255;
                    bb = (al * bb + (255 - al) * 255) / 255;
                }
                dst[(y * ancho + x) * 3] = (unsigned char)r;
                dst[(y * ancho + x) * 3 + 1] = (unsigned char)g;
                dst[(y * ancho + x) * 3 + 2] = (unsigned char)bb;
            }
        }
    }
    IFREE(prev);
    IFREE(crudo);
    *w = ancho;
    *h = alto;
    *rgb = dst;
    return 0;
}

/* ---------- JPEG baseline ---------- */

/* T[x][u] = round(a(u)*cos((2x+1)*u*pi/16)*512), a(0)=1/sqrt2 */
static const short IM_T[64] = {
    362, 502, 473, 426, 362, 284, 196, 100,
    362, 426, 196, -100, -362, -502, -473, -284,
    362, 284, -196, -502, -362, 100, 473, 426,
    362, 100, -473, -284, 362, 426, -196, -502,
    362, -100, -473, 284, 362, -426, -196, 502,
    362, -284, -196, 502, -362, -100, 473, -426,
    362, -426, 196, 100, -362, 502, -473, 284,
    362, -502, 473, -426, 362, -284, 196, -100
};

static const unsigned char IM_ZZ[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/* lector MSB con relleno FF00 */
typedef struct {
    const unsigned char *d;
    int n, pos;
    unsigned bitbuf;
    int nbits;
    int marca;   /* -1 o byte tras FF */
} IJBR;

static void ijbr_ini(IJBR *b, const unsigned char *d, int n, int pos) {
    b->d = d;
    b->n = n;
    b->pos = pos;
    b->bitbuf = 0;
    b->nbits = 0;
    b->marca = -1;
}

static int ijbr_mas(IJBR *b) {
    while (b->nbits <= 24) {
        unsigned c;
        if (b->pos >= b->n) return -1;
        c = b->d[b->pos++];
        if (c == 0xFF) {
            unsigned e;
            if (b->pos >= b->n) {
                b->pos--;
                return -1;
            }
            e = b->d[b->pos];
            if (e == 0x00) {
                b->pos++;
            } else if (e == 0xFF) {
                continue;  /* byte de relleno */
            } else {
                b->marca = (int)e;
                b->pos--;
                return -1;
            }
        }
        b->bitbuf = (b->bitbuf << 8) | (c & 0xFFu);
        b->nbits += 8;
    }
    return 0;
}

static int ijbr_ver(IJBR *b, int k) {
    /* mas puede parar en un marcador con bits validos en el
       bufer (p. ej. 2 bytes + EOI): solo importa si hay >= k. */
    if (b->nbits < k) ijbr_mas(b);
    return b->nbits < k ? -1 : 0;
}

static unsigned ijbr_verbits(IJBR *b, int k) {
    return (b->bitbuf >> (b->nbits - k)) & (k >= 32 ? 0xFFFFFFFFu :
           ((1u << k) - 1u));
}

static void ijbr_quita(IJBR *b, int k) {
    b->nbits -= k;
    if (b->nbits)
        b->bitbuf &= (b->nbits >= 32 ? 0xFFFFFFFFu :
                      ((1u << b->nbits) - 1u));
    else
        b->bitbuf = 0;
}

/* Huffman MSB: rapido 8 bits + arbol */
typedef struct {
    int rapido[256];
    int arbol[600 * 2];
    int nodos;
    int valido;
} IJH;

static int ijh_construye(IJH *h, const unsigned char *cnt,
                         const unsigned char *sim, int nsim) {
    int i, len, cod = 0, k = 0;
    for (i = 0; i < 256; i++) h->rapido[i] = -1;
    for (i = 0; i < 600 * 2; i++) h->arbol[i] = -9999;
    h->nodos = 1;
    h->valido = 0;
    {   /* nunca sobre-suscrito (incompleto vale) */
        int izq = 1, ln;
        for (ln = 0; ln < 16; ln++) {
            izq <<= 1;
            izq -= cnt[ln];
            if (izq < 0) return -1;
        }
    }
    for (len = 1; len <= 16; len++) {
        int j;
        for (j = 0; j < cnt[len - 1]; j++) {
            int s;
            if (k >= nsim) return -1;
            s = sim[k++];
            if (len <= 8) {
                int paso = 1 << (8 - len), q;
                for (q = cod << (8 - len);
                     q < (cod << (8 - len)) + paso; q++)
                    h->rapido[q] = (len << 9) | s;
            } else {
                int nodo = 0, m;
                for (m = 0; m < len; m++) {
                    int bit = (cod >> (len - 1 - m)) & 1, sig;
                    if (m == len - 1) {
                        if (h->arbol[nodo * 2 + bit] != -9999)
                            return -1;
                        h->arbol[nodo * 2 + bit] = ~s;
                        break;
                    }
                    sig = h->arbol[nodo * 2 + bit];
                    if (sig == -9999) {
                        if (h->nodos >= 600) return -1;
                        sig = h->nodos++;
                        h->arbol[nodo * 2 + bit] = sig;
                        h->arbol[sig * 2] = -9999;
                        h->arbol[sig * 2 + 1] = -9999;
                    } else if (sig < 0) {
                        return -1;
                    }
                    nodo = sig;
                }
            }
            cod++;
        }
        cod <<= 1;  /* sin tope aqui: el pre-chequeo puff ya cubre */
    }
    h->valido = 1;
    return 0;
}

static int ijh_dec(IJBR *b, IJH *h) {
    unsigned x;
    int v, disp;
    if (!h->valido) return -1;
    /* al final del barrido puede haber < 8 bits (p. ej. EOB de 4
       + relleno a 1 + EOI): JPEG rellena con 1, asi que se
       completa a 8 con unos y solo vale si el codigo cabe en
       los bits reales. */
    disp = 0;
    if (b->nbits < 8) ijbr_mas(b);
    disp = b->nbits < 8 ? b->nbits : 8;
    if (!disp) return -1;
    x = ijbr_verbits(b, disp);
    x = (x << (8 - disp)) | (disp >= 8 ? 0u : ((1u << (8 - disp)) - 1u));
    v = h->rapido[x];
    if (v >= 0 && (v >> 9) <= disp) {
        ijbr_quita(b, v >> 9);
        return v & 511;
    }
    if (disp < 8) return -1;  /* codigo partido por el final */
    {
        int nodo = 0;
        for (;;) {
            int bit, sig;
            if (ijbr_ver(b, 1)) return -1;
            bit = (int)ijbr_verbits(b, 1);
            ijbr_quita(b, 1);
            if (nodo < 0 || nodo >= 600) return -1;
            sig = h->arbol[nodo * 2 + bit];
            if (sig == -9999) return -1;
            if (sig < 0) return ~sig;
            nodo = sig;
        }
    }
}

static int ij_recibe(IJBR *b, int s) {
    unsigned v;
    if (!s) return 0;
    if (s > 16 || ijbr_ver(b, s)) return 0x80000000;
    v = ijbr_verbits(b, s);
    ijbr_quita(b, s);
    if (v < (1u << (s - 1)))
        return (int)v - (int)((1u << s) - 1u);
    return (int)v;
}

/* IDCT 1D entera: sal[x] = (sum F*IM_T)/1024 con redondeo */
static void ij_idct1(const int *ent, int *sal) {
    int x, u;
    for (x = 0; x < 8; x++) {
        long s = 0;
        for (u = 0; u < 8; u++)
            s += (long)ent[u] * (long)IM_T[x * 8 + u];
        sal[x] = (int)((s >= 0 ? (s + 512) : (s - 512)) / 1024);
    }
}

int imgdec_jpg(const unsigned char *d, int n, int maxpx,
               int *w, int *h, unsigned char **rgb) {
    int pos = 0, ancho = 0, alto = 0, ncomp = 0;
    int maxH = 0, maxV = 0, ri = 0;
    int qtab[4][64], qok[4] = { 0, 0, 0, 0 };
    /* ~46 KB de tablas: a estatico (la pila de usuario son 128 KB
       y el layout ya anida hondo). Se reconstruyen por llamada. */
    static IJH hdc[4], hac[4];
    int dcok[4] = { 0, 0, 0, 0 }, acok[4] = { 0, 0, 0, 0 };
    int cid[3], cH[3], cV[3], cTq[3], i, k;
    int nscan = 0, sc_comp[3], sc_td[3], sc_ta[3];
    int sos_pos = -1;
    unsigned char *sal = 0;
    IJBR br;
    short *qb = 0;
    unsigned char *px = 0;
    int *f1 = 0, *f2 = 0;
    int nmcu_w, nmcu_h, mx, my, nmcu = 0;
    int pred[3] = { 0, 0, 0 };
    for (i = 0; i < 4; i++) {
        hdc[i].valido = 0;
        hac[i].valido = 0;
    }
    *w = 0;
    *h = 0;
    *rgb = 0;
    if (!d || n < 4 || d[0] != 0xFF || d[1] != 0xD8) return -1;
    pos = 2;
    /* cabeceras */
    while (pos + 4 <= n) {
        unsigned m, len;
        if (d[pos] != 0xFF) return -1;
        m = d[pos + 1];
        pos += 2;
        while (m == 0xFF) {  /* relleno */
            if (pos >= n) return -1;
            m = d[pos++];
        }
        if (m == 0xD9) break;            /* EOI */
        if (m == 0xDA) {                 /* SOS */
            if (pos + 2 > n) return -1;
            len = im_u16(d + pos);
            if (len < 8 || pos + (int)len > n) return -1;
            nscan = d[pos + 2];
            if (nscan < 1 || nscan > 3 ||
                len != (unsigned)(6 + 2 * nscan)) return -1;
            for (i = 0; i < nscan; i++) {
                int cs = d[pos + 3 + 2 * i], ta = d[pos + 4 + 2 * i];
                int f;
                sc_comp[i] = -1;
                for (f = 0; f < ncomp; f++)
                    if (cid[f] == cs) sc_comp[i] = f;
                if (sc_comp[i] < 0) return -1;
                sc_td[i] = (ta >> 4) & 15;
                sc_ta[i] = ta & 15;
                if (sc_td[i] > 3 || sc_ta[i] > 3 ||
                    !dcok[sc_td[i]] || !acok[sc_ta[i]])
                    return -1;
            }
            if (d[pos + 3 + 2 * nscan] != 0 ||
                d[pos + 4 + 2 * nscan] != 63 ||
                d[pos + 5 + 2 * nscan] != 0)
                return -1;  /* solo espectral completo */
            sos_pos = pos + (int)len;
            if (sos_pos < 0 || sos_pos > n) return -1;
            break;  /* un solo barrido (documentado) */
        }
        if (m == 0xD0 || m == 0xD1 || m == 0xD2 || m == 0xD3 ||
            m == 0xD4 || m == 0xD5 || m == 0xD6 || m == 0xD7 ||
            m == 0x01)
            continue;  /* sin longitud */
        if (pos + 2 > n) return -1;
        len = im_u16(d + pos);
        if (len < 2 || pos + (int)len > n) return -1;
        if (m == 0xC0 || m == 0xC1) {    /* SOF0/C1 8 bits */
            int p = d[pos + 2];
            if (p != 8) return -1;
            alto = (d[pos + 3] << 8) | d[pos + 4];
            ancho = (d[pos + 5] << 8) | d[pos + 6];
            ncomp = d[pos + 7];
            if (!ancho || !alto || ancho > 2000 || alto > 2000)
                return -1;
            if ((long)ancho * (long)alto > (long)maxpx) return -1;
            if (ncomp != 1 && ncomp != 3) return -1;
            if (len != (unsigned)(8 + 3 * ncomp)) return -1;
            maxH = maxV = 0;
            for (i = 0; i < ncomp; i++) {
                cid[i] = d[pos + 8 + 3 * i];
                cH[i] = (d[pos + 9 + 3 * i] >> 4) & 15;
                cV[i] = d[pos + 9 + 3 * i] & 15;
                cTq[i] = d[pos + 10 + 3 * i];
                if (cH[i] < 1 || cH[i] > 4 || cV[i] < 1 ||
                    cV[i] > 4 || cTq[i] > 3)
                    return -1;
                if (!qok[cTq[i]]) return -1;
                if (cH[i] > maxH) maxH = cH[i];
                if (cV[i] > maxV) maxV = cV[i];
            }
        } else if (m == 0xC2) {
            return -1;  /* progresivo: no soportado */
        } else if (m == 0xC4) {         /* DHT */
            int q = pos + 2, fin = pos + (int)len;
            while (q + 17 <= fin) {
                int tc = (d[q] >> 4) & 15, th = d[q] & 15;
                unsigned char cnt[16], sim[256];
                int ns2 = 0, t;
                if (tc > 1 || th > 3) return -1;
                for (t = 0; t < 16; t++) {
                    cnt[t] = d[q + 1 + t];
                    ns2 += cnt[t];
                }
                if (ns2 > 256 || q + 17 + ns2 > fin) return -1;
                for (t = 0; t < ns2; t++)
                    sim[t] = d[q + 17 + t];
                if (tc == 0) {
                    if (ijh_construye(&hdc[th], cnt, sim, ns2))
                        return -1;
                    dcok[th] = 1;
                } else {
                    if (ijh_construye(&hac[th], cnt, sim, ns2))
                        return -1;
                    acok[th] = 1;
                }
                q += 17 + ns2;
            }
        } else if (m == 0xDB) {        /* DQT */
            int q = pos + 2, fin = pos + (int)len;
            while (q + 65 <= fin) {
                int prec = (d[q] >> 4) & 15, tq = d[q] & 15;
                if (tq > 3) return -1;
                if (prec == 0) {
                    for (k = 0; k < 64; k++)
                        qtab[tq][IM_ZZ[k]] = d[q + 1 + k];
                    q += 65;
                } else if (prec == 1) {
                    if (q + 129 > fin) return -1;
                    for (k = 0; k < 64; k++)
                        qtab[tq][IM_ZZ[k]] = (d[q + 1 + 2 * k] << 8) |
                                            d[q + 2 + 2 * k];
                    q += 129;
                } else {
                    return -1;
                }
                qok[tq] = 1;
            }
        } else if (m == 0xDD) {        /* DRI */
            if (len != 4) return -1;
            ri = im_u16(d + pos + 2);
        }
        /* APPn/COM/DNL/otros: se saltan */
        pos += (int)len;
    }
    if (!ancho || !alto || !ncomp || sos_pos < 0 || !nscan)
        return -1;
    /* solo un barrido: tras los datos debe venir EOI (u otra
       marca que no sea SOS) */
    nmcu_w = (ancho + 8 * maxH - 1) / (8 * maxH);
    nmcu_h = (alto + 8 * maxV - 1) / (8 * maxV);
    sal = (unsigned char *)IMALLOC((unsigned)(ancho * alto * 3));
    qb = (short *)IMALLOC(16 * 64 * 2);
    px = (unsigned char *)IMALLOC(16 * 64);
    f1 = (int *)IMALLOC(64 * 4);
    f2 = (int *)IMALLOC(64 * 4);
    if (!sal || !qb || !px || !f1 || !f2) {
        if (sal) IFREE(sal);
        if (qb) IFREE(qb);
        if (px) IFREE(px);
        if (f1) IFREE(f1);
        if (f2) IFREE(f2);
        return -1;
    }
    ijbr_ini(&br, d, n, sos_pos);
    for (i = 0; i < 3; i++) pred[i] = 0;
    for (my = 0; my < nmcu_h; my++) {
        for (mx = 0; mx < nmcu_w; mx++) {
            /* decodifica los bloques de esta MCU */
            {
                int boff = 0, tot = 0, ci;
                for (ci = 0; ci < nscan; ci++)
                    tot += cH[sc_comp[ci]] * cV[sc_comp[ci]];
                if (tot > 16) goto mal;  /* muestreo extremo */
                for (i = 0; i < nscan; i++) {
                    int c = sc_comp[i], nb, bq;
                    IJH *hd = &hdc[sc_td[i]], *ha = &hac[sc_ta[i]];
                    nb = cH[c] * cV[c];
                for (bq = 0; bq < nb; bq++) {
                    short *q = qb + ((boff + bq) * 64);
                    int s, kk, kk2;
                    unsigned char *o;
                    for (kk = 0; kk < 64; kk++) q[kk] = 0;
                    /* DC */
                    s = ijh_dec(&br, hd);
                    if (s < 0 || s > 11) goto mal;
                    if (s) {
                        int v = ij_recibe(&br, s);
                        if (v == (int)0x80000000) goto mal;
                        pred[c] += v;
                    }
                    {
                        int dc = (int)pred[c] * qtab[cTq[c]][0];
                        if (dc > 32767) dc = 32767;
                        if (dc < -32767) dc = -32767;
                        q[0] = (short)dc;
                    }
                    /* AC */
                    kk = 1;
                    while (kk < 64) {
                        int rs = ijh_dec(&br, ha);
                        int r, sz, v;
                        if (rs < 0) goto mal;
                        if (rs == 0) break;       /* EOB */
                        r = (rs >> 4) & 15;
                        sz = rs & 15;
                        if (sz > 10) goto mal;
                        if (r == 15 && sz == 0) { /* ZRL */
                            kk += 16;
                            continue;
                        }
                        kk += r;
                        if (kk >= 64) goto mal;
                        v = ij_recibe(&br, sz);
                        if (v == (int)0x80000000) goto mal;
                        {
                            int vv = v * qtab[cTq[c]][IM_ZZ[kk]];
                            if (vv > 32767) vv = 32767;
                            if (vv < -32767) vv = -32767;
                            q[IM_ZZ[kk]] = (short)vv;
                        }
                        kk++;
                    }
                    /* IDCT 2D separable (o atajo DC) */
                    {
                        int solo = 1;
                        for (kk2 = 1; kk2 < 64; kk2++)
                            if (q[kk2]) {
                                solo = 0;
                                break;
                            }
                        if (solo) {
                            int v = (q[0] >= 0 ? (q[0] + 4) :
                                     (q[0] - 4)) / 8 + 128;
                            if (v < 0) v = 0;
                            if (v > 255) v = 255;
                            for (kk2 = 0; kk2 < 64; kk2++)
                                px[(boff + bq) * 64 + kk2] =
                                    (unsigned char)v;
                        } else {
                            /* filas */
                            for (kk2 = 0; kk2 < 8; kk2++) {
                                int e2[8], s2[8], t;
                                for (t = 0; t < 8; t++) {
                                    e2[t] = q[kk2 * 8 + t];
                                    if (e2[t] > 32767)
                                        e2[t] = 32767;
                                    if (e2[t] < -32767)
                                        e2[t] = -32767;
                                }
                                ij_idct1(e2, s2);
                                for (t = 0; t < 8; t++) {
                                    int w2 = s2[t];
                                    if (w2 > 32767) w2 = 32767;
                                    if (w2 < -32767) w2 = -32767;
                                    f1[kk2 * 8 + t] = w2;
                                }
                            }
                            /* columnas */
                            for (kk2 = 0; kk2 < 8; kk2++) {
                                int e2[8], s2[8], t;
                                for (t = 0; t < 8; t++)
                                    e2[t] = f1[t * 8 + kk2];
                                ij_idct1(e2, s2);
                                for (t = 0; t < 8; t++) {
                                    int w2 = s2[t] + 128;
                                    if (w2 < 0) w2 = 0;
                                    if (w2 > 255) w2 = 255;
                                    o = px + (boff + bq) * 64;
                                    o[t * 8 + kk2] =
                                        (unsigned char)w2;
                                }
                            }
                        }
                    }
                }
                boff += nb;
            }
            /* vuelca la MCU a la imagen (bases = orden del SOS) */
                {
                    int yy, xx;
                    for (yy = 0; yy < 8 * maxV; yy++) {
                        for (xx = 0; xx < 8 * maxH; xx++) {
                            int gx = mx * 8 * maxH + xx;
                            int gy = my * 8 * maxV + yy;
                            int yv, cb, cr, r2, g2, b2;
                            if (gx >= ancho || gy >= alto)
                                continue;
                            if (ncomp == 1) {
                                int cc = sc_comp[0];
                                int bl = (yy / 8) * cH[cc] +
                                         (xx / 8);
                                int v = px[bl * 64 +
                                           (yy % 8) * 8 +
                                           (xx % 8)];
                                sal[(gy * ancho + gx) * 3] =
                                    (unsigned char)v;
                                sal[(gy * ancho + gx) * 3 + 1] =
                                    (unsigned char)v;
                                sal[(gy * ancho + gx) * 3 + 2] =
                                    (unsigned char)v;
                            } else {
                                int c0 = sc_comp[0], c1 = sc_comp[1],
                                    c2 = sc_comp[2];
                                int cx0 = xx * cH[c0] / maxH;
                                int cy0 = yy * cV[c0] / maxV;
                                int cx1 = xx * cH[c1] / maxH;
                                int cy1 = yy * cV[c1] / maxV;
                                int cx2 = xx * cH[c2] / maxH;
                                int cy2 = yy * cV[c2] / maxV;
                                /* bloques dentro de la MCU */
                                int nbx0 = cH[c0] * cV[c0];
                                int base1 = nbx0, base2;
                                int bb0, bb1, bb2;
                                (void)base2;
                                bb0 = (cy0 / 8) * cH[c0] +
                                      (cx0 / 8);
                                base2 = base1 + cH[c1] * cV[c1];
                                bb1 = (cy1 / 8) * cH[c1] +
                                      (cx1 / 8);
                                bb2 = (cy2 / 8) * cH[c2] +
                                      (cx2 / 8);
                                yv = px[(bb0) * 64 + (cy0 % 8) * 8 +
                                        (cx0 % 8)];
                                cb = px[(base1 + bb1) * 64 +
                                        (cy1 % 8) * 8 + (cx1 % 8)];
                                cr = px[(base2 + bb2) * 64 +
                                        (cy2 % 8) * 8 + (cx2 % 8)];
                                {
                                    int tcb = cb - 128;
                                    int tcr = cr - 128;
                                    r2 = yv + (1436 * tcr +
                                               (tcr >= 0 ? 512 :
                                                -512)) / 1024;
                                    g2 = yv - (352 * tcb +
                                                (tcb >= 0 ? 512 :
                                                 -512)) / 1024 -
                                         (731 * tcr +
                                          (tcr >= 0 ? 512 :
                                           -512)) / 1024;
                                    b2 = yv + (1814 * tcb +
                                               (tcb >= 0 ? 512 :
                                                -512)) / 1024;
                                    if (r2 < 0) r2 = 0;
                                    if (r2 > 255) r2 = 255;
                                    if (g2 < 0) g2 = 0;
                                    if (g2 > 255) g2 = 255;
                                    if (b2 < 0) b2 = 0;
                                    if (b2 > 255) b2 = 255;
                                    sal[(gy * ancho + gx) * 3] =
                                        (unsigned char)r2;
                                    sal[(gy * ancho + gx) * 3 + 1] =
                                        (unsigned char)g2;
                                    sal[(gy * ancho + gx) * 3 + 2] =
                                        (unsigned char)b2;
                                }
                            }
                        }
                    }
                }
            }
            nmcu++;
            /* reinicio */
            if (ri > 0 && (nmcu % ri) == 0 &&
                (mx + 1 < nmcu_w || my + 1 < nmcu_h)) {
                int esp = 0xD0 + ((nmcu / ri - 1) & 7);
                /* alinea a byte */
                br.pos -= br.nbits / 8;
                br.nbits = 0;
                br.bitbuf = 0;
                if (br.pos + 2 > br.n || br.d[br.pos] != 0xFF)
                    goto mal;
                br.pos++;
                while (br.pos < br.n && br.d[br.pos] == 0xFF)
                    br.pos++;
                if (br.pos >= br.n || br.d[br.pos] != esp)
                    goto mal;
                br.pos++;
                pred[0] = pred[1] = pred[2] = 0;
            }
        }
    }
    /* tras el barrido: debe seguir EOI u otra marca (no SOS) */
    br.pos -= br.nbits / 8;
    br.nbits = 0;
    br.bitbuf = 0;
    if (br.pos + 2 <= br.n && br.d[br.pos] == 0xFF) {
        int q = br.pos + 1;
        while (q < br.n && br.d[q] == 0xFF) q++;
        if (q < br.n && br.d[q] == 0xDA) goto mal;  /* 2o barrido */
    }
    IFREE(qb);
    IFREE(px);
    IFREE(f1);
    IFREE(f2);
    *w = ancho;
    *h = alto;
    *rgb = sal;
    return 0;
mal:
    if (sal) IFREE(sal);
    if (qb) IFREE(qb);
    if (px) IFREE(px);
    if (f1) IFREE(f1);
    if (f2) IFREE(f2);
    return -1;
}
