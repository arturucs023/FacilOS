/* ttf: rasterizador TrueType propio (sin hinting), solo enteros.
 *
 * Sin UNIX/BSD/Linux/Windows: aritmetica entera, sin FPU, sin libc
 * del anfitrion (u_malloc/u_free de ulib). Lee head/hhea/maxp/hmtx/
 * cmap(4/6/12)/loca/glyf; tesela quadraticas por subdivision y
 * rellena por scanline (paridad) con supermuestreo x2. Sin
 * GPOS/GSUB, sin kerning, sin bidireccional (fase 2 si hace falta).
 *
 * La fuente vive en el programa (objcopy --input binary); el nucleo
 * solo recibe la mascara ya rasterizada (sys_mezcla, sc_64).
 */
#include "ttf.h"
#include "ulib.h"

typedef unsigned int u32;
typedef unsigned short u16;
typedef int i32;

static const unsigned char *F;
static int Fn;
static int T_hhea, T_maxp, T_hmtx, T_cmap, T_loca, T_glyf;
static int UPM, NHMTX, NGLIF, LOCA_LARGA;
static int ASC, DESC, LGAP;
static int CM_fmt;
static int CM_tab;

static u32 U16(const unsigned char *p) {
    return ((u32)p[0] << 8) | (u32)p[1];
}

static i32 I16(const unsigned char *p) {
    int v = ((int)p[0] << 8) | (int)p[1];
    return v >= 0x8000 ? v - 0x10000 : v;
}

static u32 U32(const unsigned char *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) |
            ((u32)p[2] << 8) | (u32)p[3];
}

static int dentro(int off, int len) {
    if (off < 0 || len < 0 || off > Fn) return 0;
    if (len > Fn - off) return 0;
    return 1;
}

int ttf_abre(const unsigned char *d, int n) {
    u32 magic;
    int nt, i;
    int o_head = -1, o_hhea = -1, o_maxp = -1, o_hmtx = -1;
    int o_cmap = -1, o_loca = -1, o_glyf = -1;
    const unsigned char *Fold;
    int Fnold;
    if (!d || n < 12) return -1;
    /* No destruir la fuente abierta si esta falla (el KAT abre
       un recorte para probar el error y sigue usando la buena). */
    Fold = F;
    Fnold = Fn;
    F = d;
    Fn = n;
    magic = U32(d);
    if (magic != 0x00010000u && magic != 0x74727565u &&
        magic != 0x74797031u)
        { F = Fold; Fn = Fnold; return -1; }
    nt = (int)U16(d + 4);
    if (nt <= 0 || nt > 64) { F = Fold; Fn = Fnold; return -1; }
    if (!dentro(12, nt * 16)) { F = Fold; Fn = Fnold; return -1; }
    for (i = 0; i < nt; i++) {
        const unsigned char *r = d + 12 + (unsigned)i * 16;
        char tag[5];
        int k;
        for (k = 0; k < 4; k++) tag[k] = (char)r[k];
        tag[4] = 0;
        if (!u_strcmp(tag, "head")) o_head = (int)U32(r + 8);
        else if (!u_strcmp(tag, "hhea")) o_hhea = (int)U32(r + 8);
        else if (!u_strcmp(tag, "maxp")) o_maxp = (int)U32(r + 8);
        else if (!u_strcmp(tag, "hmtx")) o_hmtx = (int)U32(r + 8);
        else if (!u_strcmp(tag, "cmap")) o_cmap = (int)U32(r + 8);
        else if (!u_strcmp(tag, "loca")) o_loca = (int)U32(r + 8);
        else if (!u_strcmp(tag, "glyf")) o_glyf = (int)U32(r + 8);
    }
    if (o_head < 0 || o_hhea < 0 || o_maxp < 0 || o_hmtx < 0 ||
        o_cmap < 0 || o_loca < 0 || o_glyf < 0)
        { F = Fold; Fn = Fnold; return -1; }
    if (!dentro(o_head, 54) || !dentro(o_hhea, 36) ||
        !dentro(o_maxp, 6))
        { F = Fold; Fn = Fnold; return -1; }
    UPM = (int)U16(d + o_head + 18);
    if (UPM < 16 || UPM > 16384) { F = Fold; Fn = Fnold; return -1; }
    LOCA_LARGA = I16(d + o_head + 50) != 0;
    ASC = I16(d + o_hhea + 4);
    DESC = I16(d + o_hhea + 6);
    LGAP = I16(d + o_hhea + 8);
    NHMTX = (int)U16(d + o_hhea + 34);
    NGLIF = (int)U16(d + o_maxp + 4);
    if (NGLIF <= 0 || NGLIF > 65535) { F = Fold; Fn = Fnold; return -1; }
    if (NHMTX <= 0 || NHMTX > NGLIF) { F = Fold; Fn = Fnold; return -1; }
    T_hhea = o_hhea;
    T_maxp = o_maxp;
    T_hmtx = o_hmtx;
    T_cmap = o_cmap;
    T_loca = o_loca;
    T_glyf = o_glyf;
    {
        int nsub, s, f4 = -1, f6 = -1, f12 = -1;
        if (!dentro(o_cmap, 4)) { F = Fold; Fn = Fnold; return -1; }
        nsub = (int)U16(d + o_cmap + 2);
        if (nsub < 0 || nsub > 32) { F = Fold; Fn = Fnold; return -1; }
        if (!dentro(o_cmap + 4, nsub * 8)) { F = Fold; Fn = Fnold; return -1; }
        for (s = 0; s < nsub; s++) {
            const unsigned char *r = d + o_cmap + 4 + (unsigned)s * 8;
            int plat = (int)U16(r), enc = (int)U16(r + 2);
            int off = (int)U32(r + 4);
            int fmt = -1;
            if (off < 0 || o_cmap + off + 2 > Fn) continue;
            fmt = (int)U16(d + o_cmap + off);
            if (plat == 3 && enc == 10 && fmt == 12) f12 = off;
            else if (((plat == 3 && enc == 1) ||
                      (plat == 0 && enc == 3)) && fmt == 4 && f4 < 0)
                f4 = off;
            else if (plat == 1 && enc == 0 && fmt == 6 && f6 < 0)
                f6 = off;
        }
        if (f12 >= 0) {
            CM_fmt = 12;
            CM_tab = o_cmap + f12;
        } else if (f4 >= 0) {
            CM_fmt = 4;
            CM_tab = o_cmap + f4;
        } else if (f6 >= 0) {
            CM_fmt = 6;
            CM_tab = o_cmap + f6;
        } else {
            CM_fmt = 0;
            CM_tab = 0;
            { F = Fold; Fn = Fnold; return -1; }
        }
    }
    return 0;
}

static int cmap_gid(unsigned cp) {
    if (CM_fmt == 4) {
        const unsigned char *t = F + CM_tab;
        int seg, i;
        if (!dentro(CM_tab, 16)) return 0;
        seg = (int)U16(t + 6) / 2;
        if (seg <= 0 || seg > 256) return 0;
        if (!dentro(CM_tab, 16 + seg * 8)) return 0;
        if (cp > 0xFFFFu) return 0;
        for (i = 0; i < seg; i++) {
            unsigned fin = U16(t + 14 + (unsigned)i * 2);
            const unsigned char *ini, *dlt, *ran;
            unsigned ini_v, roff;
            int delta;
            if (cp > fin) continue;
            ini = t + 14 + (unsigned)seg * 2 + 2;
            ini_v = U16(ini + (unsigned)i * 2);
            if (cp < ini_v) return 0;
            dlt = ini + (unsigned)seg * 2;
            ran = dlt + (unsigned)seg * 2;
            delta = I16(dlt + (unsigned)i * 2);
            roff = U16(ran + (unsigned)i * 2);
            if (roff == 0)
                return (int)((cp + (unsigned)delta) & 0xFFFFu);
            {
                int base =
                    (int)(ran + (unsigned)i * 2 - F -
                          (unsigned)CM_tab);
                unsigned adr =
                    (unsigned)base + roff + (cp - ini_v) * 2u;
                unsigned g;
                if (CM_tab + (int)adr + 2 > Fn) return 0;
                g = U16(F + CM_tab + adr);
                if (!g) return 0;
                return (int)((g + (unsigned)delta) & 0xFFFFu);
            }
        }
        return 0;
    } else if (CM_fmt == 12) {
        const unsigned char *t = F + CM_tab;
        int ng, lo, hi;
        if (!dentro(CM_tab, 16)) return 0;
        ng = (int)U32(t + 12);
        if (ng < 0 || ng > 4096) return 0;
        if (!dentro(CM_tab, 16 + ng * 12)) return 0;
        lo = 0;
        hi = ng - 1;
        while (lo <= hi) {
            int m = (lo + hi) / 2;
            const unsigned char *g = t + 16 + (unsigned)m * 12;
            unsigned ini = U32(g), fin = U32(g + 4);
            if (cp < ini) {
                hi = m - 1;
            } else if (cp > fin) {
                lo = m + 1;
            } else {
                return (int)(U32(g + 8) + (cp - ini));
            }
        }
        return 0;
    } else if (CM_fmt == 6) {
        const unsigned char *t = F + CM_tab;
        unsigned primero, n;
        if (!dentro(CM_tab, 10)) return 0;
        primero = U16(t + 6);
        n = U16(t + 8);
        if (cp < primero || cp >= primero + n) return 0;
        if (!dentro(CM_tab, 10 + (int)(cp - primero) * 2 + 2))
            return 0;
        return (int)U16(t + 10 + (cp - primero) * 2);
    }
    return 0;
}

int ttf_cubre(unsigned cp) {
    if (!F) return 0;
    return cmap_gid(cp) != 0;
}

static int hmtx_avance(int gi) {
    int adv;
    if (gi < 0 || gi >= NGLIF) return 0;
    if (gi < NHMTX)
        adv = (int)U16(F + T_hmtx + gi * 4);
    else
        adv = (int)U16(F + T_hmtx + (NHMTX - 1) * 4);
    return adv;
}

int ttf_avance(unsigned cp, int px) {
    int gi, adv;
    if (!F || px <= 0) return 0;
    gi = cmap_gid(cp);
    if (!gi) return 0;
    adv = hmtx_avance(gi);
    if (adv < 0) adv = 0;
    return (adv * px + UPM / 2) / UPM;
}

int ttf_alto_linea(int px) {
    int h;
    if (!F || px <= 0) return 16;
    h = ASC - DESC + LGAP;
    if (h < UPM) h = UPM;
    h = (h * px + UPM / 2) / UPM;
    return h < 1 ? 1 : h;
}

int ttf_ascenso(int px) {
    if (!F || px <= 0) return 12;
    return (ASC * px + UPM / 2) / UPM;
}

int ttf_utf8(const char *s, int n, unsigned *cp) {
    unsigned char c;
    unsigned v;
    int m = 0;
    if (!s || n <= 0 || !cp) return 0;
    c = (unsigned char)s[0];
    if (c < 0x80) {
        *cp = c;
        return 1;
    } else if (c >= 0xC2 && c <= 0xDF) {
        m = 1;
        v = c & 0x1F;
    } else if (c >= 0xE0 && c <= 0xEF) {
        m = 2;
        v = c & 0x0F;
    } else if (c >= 0xF0 && c <= 0xF4) {
        m = 3;
        v = c & 0x07;
    } else {
        *cp = 0xFFFD;
        return 1;
    }
    if (m >= n) {
        *cp = 0xFFFD;
        return 1;
    }
    {
        int i;
        for (i = 1; i <= m; i++) {
            unsigned char d = (unsigned char)s[i];
            if ((d & 0xC0) != 0x80) {
                *cp = 0xFFFD;
                return 1;
            }
            v = (v << 6) | (d & 0x3F);
        }
    }
    if (v > 0x10FFFFu || (v >= 0xD800u && v <= 0xDFFFu))
        v = 0xFFFD;
    *cp = v;
    return m + 1;
}

int ttf_ancho(const char *s, int n, int px) {
    int i = 0, w = 0;
    unsigned cp;
    if (!s || n <= 0 || px <= 0) return 0;
    while (i < n) {
        int k = ttf_utf8(s + i, n - i, &cp);
        if (k <= 0) break;
        i += k;
        w += ttf_avance(cp, px);
    }
    return w;
}

/* ---- teselado y relleno ---- */

#define TT_MAXPT 2048
#define TT_MAXSEG 4096
#define TT_SS 2
#define TT_POOLD (384 * 1024)

/* puntos en font units (con signo), contornos por indices finales */
static long TT_fx[TT_MAXPT], TT_fy[TT_MAXPT];
static char TT_fon[TT_MAXPT];
static int TT_ffin[TT_MAXPT];
static int TT_np, TT_nfin;
/* segmentos en subpixeles */
static int TT_sx0[TT_MAXSEG], TT_sy0[TT_MAXSEG];
static int TT_sx1[TT_MAXSEG], TT_sy1[TT_MAXSEG];
static int TT_nseg;
static int TT_px_actual;
static int TT_ss;

typedef struct {
    int vale;
    unsigned gi;
    int px;
    int adv, xoff, ytop, w, h, off;
} tt_ent_t;
#define TT_NCACHE 256
static tt_ent_t TT_cache[TT_NCACHE];
static unsigned char *TT_pool;
static int TT_pool_usado;
static unsigned char TT_sub[(200 * 2) * (200 * 2)];

/* escala entera: font unit -> subpixel (con redondeo simetrico) */
static int tt_escala(long v) {
    long z = v * (long)TT_px_actual * (long)TT_SS;
    if (z >= 0)
        return (int)((z + UPM / 2) / (long)UPM);
    return -(int)((-z + UPM / 2) / (long)UPM);
}

static void tt_segmento(int x0, int y0, int x1, int y1) {
    if (TT_nseg >= TT_MAXSEG) return;
    if (x0 == x1 && y0 == y1) return;
    TT_sx0[TT_nseg] = x0;
    TT_sy0[TT_nseg] = y0;
    TT_sx1[TT_nseg] = x1;
    TT_sy1[TT_nseg] = y1;
    TT_nseg++;
}

/* subdivide la cuadratica p0,p1,p2 hasta aplanarla (<1 subpx) */
static void tt_cuad(int x0, int y0, int x1, int y1, int x2, int y2,
                    int prof) {
    int mx01x = (x0 + x1) / 2, mx01y = (y0 + y1) / 2;
    int mx12x = (x1 + x2) / 2, mx12y = (y1 + y2) / 2;
    int mx = (mx01x + mx12x) / 2, my = (mx01y + mx12y) / 2;
    int cx = (x0 + x2) / 2, cy = (y0 + y2) / 2;
    int dx = mx - cx, dy = my - cy;
    if (prof > 12 ||
        (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1)) {
        tt_segmento(x0, y0, x2, y2);
        return;
    }
    tt_cuad(x0, y0, mx01x, mx01y, mx, my, prof + 1);
    tt_cuad(mx, my, mx12x, mx12y, x2, y2, prof + 1);
}

/* offset/longitud de un glifo en glyf. 0 ok. */
static int glifo_off(int gi, int *off, int *len) {
    int a, b;
    if (gi < 0 || gi >= NGLIF) return -1;
    if (LOCA_LARGA) {
        if (!dentro(T_loca + gi * 4, 8)) return -1;
        a = (int)U32(F + T_loca + (unsigned)gi * 4);
        b = (int)U32(F + T_loca + (unsigned)(gi + 1) * 4);
    } else {
        if (!dentro(T_loca + gi * 2, 4)) return -1;
        a = (int)U16(F + T_loca + (unsigned)gi * 2) * 2;
        b = (int)U16(F + T_loca + (unsigned)(gi + 1) * 2) * 2;
    }
    if (b < a) return -1;
    if (!dentro(T_glyf + a, b - a)) return -1;
    *off = T_glyf + a;
    *len = b - a;
    return 0;
}

/* 1 si el glifo esta vacio (sin tinta: espacios). */
static int glifo_vacio(int gi) {
    int off, len;
    if (glifo_off(gi, &off, &len)) return 1;
    if (len < 12) return 1;
    return I16(F + off) == 0;
}

/* lee un glifo simple y lo anade transformado (M 2x2 en 2.14,
   O en font units). Anexa a TT_fx/fy/fon y TT_ffin. */
static int tt_simple(const unsigned char *g, int len, long axx,
                     long axy, long ayx, long ayy, long ox,
                     long oy) {
    int nc, i, np = 0, off;
    static long xs[TT_MAXPT], ys[TT_MAXPT];
    static unsigned char fl[TT_MAXPT];
    if (len < 12) return -1;
    nc = I16(g);
    if (nc <= 0 || nc > 64) return -1;
    if (len < 10 + nc * 2 + 2) return -1;
    for (i = 0; i < nc; i++) {
        int e = (int)U16(g + 10 + (unsigned)i * 2);
        if (e < 0 || e >= TT_MAXPT) return -1;
        if (i > 0 && e < np) return -1;
        np = e + 1;
    }
    if (np > TT_MAXPT) return -1;
    off = 10 + nc * 2;
    if (off + 2 > len) return -1;
    off += 2 + (int)U16(g + off);
    if (off < 0 || off > len) return -1;
    {
        int k = 0, rep = 0;
        unsigned char cf = 0;
        while (k < np) {
            if (rep > 0) {
                rep--;
            } else {
                if (off >= len) return -1;
                cf = g[off++];
                if (cf & 8) {
                    if (off >= len) return -1;
                    rep = g[off++];
                }
            }
            fl[k++] = cf;
            if (k >= TT_MAXPT) return -1;
        }
        {
            long x = 0, y = 0;
            for (k = 0; k < np; k++) {
                if (fl[k] & 2) {
                    int v;
                    if (off >= len) return -1;
                    v = g[off++];
                    x += (fl[k] & 16) ? v : -v;
                } else if (!(fl[k] & 16)) {
                    if (off + 1 >= len) return -1;
                    x += I16(g + off);
                    off += 2;
                }
                xs[k] = x;
            }
            for (k = 0; k < np; k++) {
                if (fl[k] & 4) {
                    int v;
                    if (off >= len) return -1;
                    v = g[off++];
                    y += (fl[k] & 32) ? v : -v;
                } else if (!(fl[k] & 32)) {
                    if (off + 1 >= len) return -1;
                    y += I16(g + off);
                    off += 2;
                }
                ys[k] = y;
            }
        }
        for (i = 0; i < nc; i++) {
            int fin = (int)U16(g + 10 + (unsigned)i * 2);
            int ini = i == 0 ? 0 :
                (int)U16(g + 10 + (unsigned)(i - 1) * 2) + 1;
            int base = TT_np, k;
            if (fin - ini + 1 <= 0) continue;
            if (base + (fin - ini + 1) > TT_MAXPT) return -1;
            if (TT_nfin >= TT_MAXPT) return -1;
            for (k = ini; k <= fin; k++) {
                long wx = (axx * xs[k] + ayx * ys[k]) / 16384L +
                           ox;
                long wy = (axy * xs[k] + ayy * ys[k]) / 16384L +
                           oy;
                TT_fx[base + (k - ini)] = wx;
                TT_fy[base + (k - ini)] = wy;
                TT_fon[base + (k - ini)] =
                    (fl[k] & 1) ? (char)1 : (char)0;
            }
            TT_ffin[TT_nfin++] = base + (fin - ini);
            TT_np = base + (fin - ini) + 1;
        }
    }
    return 0;
}

/* vuelca un compuesto (recursivo). M/O acumulan el contexto:
   x' = (xx*x + yx*y)/16384 + ox (2.14). */
static int tt_compuesto(const unsigned char *g, int len, int prof,
                        long axx, long axy, long ayx, long ayy,
                        long ox, long oy) {
    int off = 10;
    if (prof > 4) return -1;
    for (;;) {
        unsigned flags;
        int gi, arg1, arg2;
        long scx = 16384, scy = 16384, s2x = 0, s2y = 0;
        long bx = 0, by = 0;
        if (off + 4 > len) return -1;
        flags = U16(g + off);
        gi = (int)U16(g + off + 2);
        off += 4;
        if (flags & 1) {
            if (off + 4 > len) return -1;
            arg1 = I16(g + off);
            arg2 = I16(g + off + 2);
            off += 4;
        } else {
            if (off + 2 > len) return -1;
            arg1 = (int)(signed char)g[off];
            arg2 = (int)(signed char)g[off + 1];
            off += 2;
        }
        if (!(flags & 2)) return -1;
        if (flags & 8) {
            if (off + 2 > len) return -1;
            scx = I16(g + off);
            scy = scx;
            off += 2;
        } else if (flags & 64) {
            if (off + 4 > len) return -1;
            scx = I16(g + off);
            scy = I16(g + off + 2);
            off += 4;
        } else if (flags & 128) {
            if (off + 8 > len) return -1;
            scx = I16(g + off);
            s2x = I16(g + off + 2);
            s2y = I16(g + off + 4);
            scy = I16(g + off + 6);
            off += 8;
        }
        bx = arg1;
        by = arg2;
        if (flags & 0x100) {
            int ni;
            if (off + 2 > len) return -1;
            ni = (int)U16(g + off);
            off += 2;
            if (off + ni > len || off + ni < off) return -1;
            off += ni;
        }
        {
            int goff, glen;
            long nxx = (axx * scx + ayx * s2y) / 16384L;
            long nxy = (axy * scx + ayy * s2y) / 16384L;
            long nyx = (axx * s2x + ayx * scy) / 16384L;
            long nyy = (axy * s2x + ayy * scy) / 16384L;
            long nox = ox + (axx * bx + ayx * by) / 16384L;
            long noy = oy + (axy * bx + ayy * by) / 16384L;
            if (glifo_off(gi, &goff, &glen)) return -1;
            if (glen >= 12) {
                const unsigned char *gg = F + goff;
                int ncg = I16(gg);
                if (ncg < 0) {
                    if (tt_compuesto(gg, glen, prof + 1, nxx, nxy,
                                     nyx, nyy, nox, noy))
                        return -1;
                } else if (ncg > 0) {
                    if (tt_simple(gg, glen, nxx, nxy, nyx, nyy,
                                  nox, noy))
                        return -1;
                }
            }
        }
        if (flags & 0x20)
            continue;
        break;
    }
    return 0;
}

/* ordena insercion (pocos cruces por fila) */
static void tt_ordena(int *xs, int n) {
    int i, j;
    for (i = 1; i < n; i++) {
        int v = xs[i];
        j = i - 1;
        while (j >= 0 && xs[j] > v) {
            xs[j + 1] = xs[j];
            j--;
        }
        xs[j + 1] = v;
    }
}

/* rellena por paridad en el mapa de bits sub (1 B/px, 0/1).
   w,h en subpixeles. */
static void tt_rellena(unsigned char *sub, int w, int h) {
    int y, i, k, n;
    static int xs[256];
    for (y = 0; y < h; y++) {
        n = 0;
        for (i = 0; i < TT_nseg; i++) {
            int y0 = TT_sy0[i], y1 = TT_sy1[i];
            int lo = y0 < y1 ? y0 : y1;
            int hi = y0 < y1 ? y1 : y0;
            if (y + 1 <= lo || y >= hi) continue;
            if (n < 256) {
                int x0 = TT_sx0[i], x1 = TT_sx1[i];
                int xx = x0 + ((x1 - x0) * (2 * (y - lo) + 1)) /
                                  (2 * (hi - lo));
                xs[n++] = xx;
            }
        }
        if (!n) continue;
        tt_ordena(xs, n);
        for (k = 0; k + 1 < n; k += 2) {
            int a = xs[k], b = xs[k + 1], x;
            if (a < 0) a = 0;
            if (b > w) b = w;
            for (x = a; x < b; x++)
                sub[(unsigned)y * (unsigned)w + (unsigned)x] = 1;
        }
    }
}

/* baja x2 (promedia) a alfa 0..255 */
static void tt_baja(const unsigned char *sub, int w2, int h2,
                    unsigned char *alfa, int w, int h) {
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            unsigned s = 0;
            int yy, xx;
            for (yy = 0; yy < 2; yy++) {
                for (xx = 0; xx < 2; xx++) {
                    int sx = x * 2 + xx, sy = y * 2 + yy;
                    if (sx < w2 && sy < h2)
                        s += sub[(unsigned)sy * (unsigned)w2 +
                                 (unsigned)sx];
                }
            }
            alfa[(unsigned)y * (unsigned)w + (unsigned)x] =
                (unsigned char)((s * 255u + 2u) / 4u);
        }
    }
}

static void tt_cache_limpia(void) {
    int i;
    for (i = 0; i < TT_NCACHE; i++) TT_cache[i].vale = 0;
    TT_pool_usado = 0;
}

/* emite todos los contornos acumulados (escala a subpixeles) */
static void tt_emitir(void) {
    int c, k;
    static int sx[TT_MAXPT], sy[TT_MAXPT];
    static char so[TT_MAXPT];
    for (c = 0; c < TT_nfin; c++) {
        int ini = c == 0 ? 0 : TT_ffin[c - 1] + 1;
        int fin = TT_ffin[c];
        int n = fin - ini + 1, m = 0;
        if (n <= 0 || n > TT_MAXPT) continue;
        for (k = ini; k <= fin; k++) {
            sx[k] = tt_escala(TT_fx[k]);
            /* La Y TrueType crece hacia arriba; en pantalla crece
               hacia abajo: se niega al emitir. */
            sy[k] = -tt_escala(TT_fy[k]);
            so[k] = TT_fon[k];
            (void)n;
        }
        {
            int ox[TT_MAXPT], oy[TT_MAXPT];
            char oo[TT_MAXPT];
            if (so[fin]) {
                ox[0] = sx[fin];
                oy[0] = sy[fin];
                oo[0] = 1;
                m = 1;
            } else {
                ox[0] = (sx[fin] + sx[ini]) / 2;
                oy[0] = (sy[fin] + sy[ini]) / 2;
                oo[0] = 1;
                m = 1;
            }
            for (k = 0; k < n && m < TT_MAXPT; k++) {
                int j = ini + k;
                if (so[j]) {
                    ox[m] = sx[j];
                    oy[m] = sy[j];
                    oo[m] = 1;
                    m++;
                } else {
                    int j2 = ini + ((k + 1) % n);
                    int nx, ny;
                    if (so[j2]) {
                        nx = sx[j2];
                        ny = sy[j2];
                    } else {
                        nx = (sx[j] + sx[j2]) / 2;
                        ny = (sy[j] + sy[j2]) / 2;
                    }
                    ox[m] = sx[j];
                    oy[m] = sy[j];
                    oo[m] = 0;
                    m++;
                    if (m < TT_MAXPT) {
                        ox[m] = nx;
                        oy[m] = ny;
                        oo[m] = 1;
                        m++;
                    }
                }
            }
            {
                int q;
                for (q = 0; q < m; q++) {
                    int q2 = (q + 1) % m;
                    if (oo[q] && oo[q2]) {
                        tt_segmento(ox[q], oy[q], ox[q2],
                                        oy[q2]);
                    } else if (!oo[q] && oo[q2]) {
                        int q0 = (q + m - 1) % m;
                        tt_cuad(ox[q0], oy[q0], ox[q], oy[q],
                              ox[q2], oy[q2], 0);
                    }
                }
            }
        }
    }
}

/* rasteriza el glifo a la cache; devuelve entrada o 0. */
static tt_ent_t *tt_cache_glifo(unsigned gi, int px) {
    unsigned idx = ((unsigned)gi * 31u + (unsigned)px) & 255u;
    tt_ent_t *e = &TT_cache[idx];
    int off, len, nc, i;
    int x0, x1, yy0, yy1, w, h;
    int need;
    if (e->vale && e->gi == gi && e->px == px) return e;
    if (glifo_off((int)gi, &off, &len) || len < 12) return 0;
    nc = I16(F + off);
    if (nc == 0) return 0;   /* vacio (espacio): avance sin tinta */
    TT_px_actual = px;
    TT_ss = 2;
    TT_np = 0;
    TT_nfin = 0;
    TT_nseg = 0;
    if (nc > 0) {
        if (tt_simple(F + off, len, 16384, 0, 0, 16384, 0, 0))
            return 0;
    } else {
        if (tt_compuesto(F + off, len, 0, 16384, 0, 0, 16384, 0,
                         0))
            return 0;
    }
    if (TT_nfin > 0)
        tt_emitir();
    if (!TT_nseg) return 0;
    x0 = TT_sx0[0];
    x1 = TT_sx0[0];
    yy0 = TT_sy0[0];
    yy1 = TT_sy0[0];
    for (i = 0; i < TT_nseg; i++) {
        int xs[2], ys[2];
        xs[0] = TT_sx0[i];
        xs[1] = TT_sx1[i];
        ys[0] = TT_sy0[i];
        ys[1] = TT_sy1[i];
        {
            int k;
            for (k = 0; k < 2; k++) {
                if (xs[k] < x0) x0 = xs[k];
                if (xs[k] > x1) x1 = xs[k];
                if (ys[k] < yy0) yy0 = ys[k];
                if (ys[k] > yy1) yy1 = ys[k];
            }
        }
    }
    {
        int X0 = x0 / 2 - 1, X1 = (x1 + 1) / 2 + 1;
        int Y0 = yy0 / 2 - 1, Y1 = (yy1 + 1) / 2 + 1;
        int W = X1 - X0, H = Y1 - Y0;
        int W2, H2, ox, oy, k;
        unsigned char *alfa;
        if (W < 1) W = 1;
        if (H < 1) H = 1;
        if (W > 200 || H > 200) return 0;
        W2 = W * 2;
        H2 = H * 2;
        ox = X0 * 2;
        oy = Y0 * 2;
        for (k = 0; k < TT_nseg; k++) {
            TT_sx0[k] -= ox;
            TT_sx1[k] -= ox;
            TT_sy0[k] -= oy;
            TT_sy1[k] -= oy;
        }
        {
            unsigned total = (unsigned)W2 * (unsigned)H2;
            if (total > sizeof(TT_sub)) return 0;
            for (k = 0; (unsigned)k < total; k++) TT_sub[k] = 0;
        }
        tt_rellena(TT_sub, W2, H2);
        need = W * H;
        if (!TT_pool) {
            TT_pool = (unsigned char *)u_malloc(TT_POOLD);
            if (!TT_pool) return 0;
            tt_cache_limpia();
            e = &TT_cache[idx];
        }
        if (TT_pool_usado + need > TT_POOLD) {
            tt_cache_limpia();
            e = &TT_cache[idx];
        }
        alfa = TT_pool + TT_pool_usado;
        tt_baja(TT_sub, W2, H2, alfa, W, H);
        e->vale = 1;
        e->gi = gi;
        e->px = px;
        {
            int adv = hmtx_avance((int)gi);
            if (adv < 0) adv = 0;
            e->adv = (adv * px + UPM / 2) / UPM;
        }
        e->xoff = X0;
        /* Y0 es el borde superior (tras negar: -max_fuente). ytop =
           cuanto sube la tinta sobre la base. */
        e->ytop = -Y0;
        e->w = W;
        e->h = H;
        e->off = TT_pool_usado;
        TT_pool_usado += need;
        return e;
    }
}

int ttf_glifo(unsigned cp, int px, unsigned char *alfa, int cap,
              int *w, int *h, int *avance, int *dx, int *dy) {
    tt_ent_t *e;
    unsigned gi;
    int i, n;
    if (!F || !alfa || cap <= 0 || px <= 0) return -1;
    gi = cmap_gid(cp);
    if (!gi) return -1;
    if (glifo_vacio((int)gi)) return -2;
    e = tt_cache_glifo(gi, px);
    if (!e) return -1;
    n = e->w * e->h;
    if (n > cap) return -1;
    for (i = 0; i < n; i++)
        alfa[i] = TT_pool[e->off + i];
    if (w) *w = e->w;
    if (h) *h = e->h;
    if (avance) *avance = e->adv;
    if (dx) *dx = e->xoff;
    if (dy) *dy = e->ytop;
    return 0;
}

int ttf_texto(int x, int yb, const char *s, int n, int px,
              unsigned int color) {
    int i = 0, cx = x;
    unsigned cp;
    if (!F || !s || n <= 0 || px <= 0) return 0;
    while (i < n) {
        int k = ttf_utf8(s + i, n - i, &cp);
        unsigned gi;
        tt_ent_t *e;
        if (k <= 0) break;
        i += k;
        if (cp == ' ') {
            cx += ttf_avance(32, px);
            continue;
        }
        if (cp == '\n' || cp == '\t' || cp == '\r') continue;
        gi = (unsigned)cmap_gid(cp);
        if (!gi) {
            int ww = (px * 3 + 2) / 5, hh = (px * 2 + 2) / 3;
            int y0 = yb - (px * 4 + 4) / 5;
            u_rectangulo(cx, y0, ww, 1, color);
            u_rectangulo(cx, y0 + hh - 1, ww, 1, color);
            u_rectangulo(cx, y0, 1, hh, color);
            u_rectangulo(cx + ww - 1, y0, 1, hh, color);
            cx += ww + (px + 3) / 4;
            continue;
        }
        e = tt_cache_glifo(gi, px);
        if (!e) {
            cx += ttf_avance(cp, px);
            continue;
        }
        {
            int dx = cx + e->xoff, dy = yb - e->ytop;
            unsigned char *m = TT_pool + e->off;
            if (e->w > 0 && e->h > 0)
                u_mezcla(m, e->w, e->h, dx, dy, color);
            cx += e->adv;
        }
    }
    return cx - x;
}
