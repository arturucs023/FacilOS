#include "utext.h"

unsigned ut_dec(const char *s, int max, int *av) {
    unsigned char c0;
    unsigned cp;
    int n = 0;
    if (max <= 0) {
        if (av) *av = 0;
        return UT_REEMPLAZO;
    }
    c0 = (unsigned char)s[0];
    if (c0 < 0x80) {
        if (av) *av = 1;
        return c0;
    }
    if (c0 < 0xC2) n = 0;             /* continuacion o C0/C1: invalido */
    else if (c0 < 0xE0) n = 2;
    else if (c0 < 0xF0) n = 3;
    else if (c0 < 0xF5) n = 4;
    else n = 0;
    if (!n || n > max) {
        if (av) *av = 1;
        return UT_REEMPLAZO;
    }
    {
        int i;
        static const unsigned char msk[5] = { 0, 0, 0x1F, 0x0F, 0x07 };
        cp = c0 & msk[n];
        for (i = 1; i < n; i++) {
            unsigned char cc = (unsigned char)s[i];
            if ((cc & 0xC0) != 0x80) {
                if (av) *av = 1;
                return UT_REEMPLAZO;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
    }
    /* Sobre-largas, sustitutos y fuera de rango. */
    if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) ||
        (n == 4 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF) ||
        cp > 0x10FFFF) {
        if (av) *av = 1;
        return UT_REEMPLAZO;
    }
    if (av) *av = n;
    return cp;
}

int ut_enc(char *d, int cap, unsigned cp) {
    if (cp < 0x80) {
        if (cap < 1) return 0;
        d[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        if (cap < 2) return 0;
        d[0] = (char)(0xC0 | (cp >> 6));
        d[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        if (cp >= 0xD800 && cp <= 0xDFFF) return 0;
        if (cap < 3) return 0;
        d[0] = (char)(0xE0 | (cp >> 12));
        d[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        d[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        if (cap < 4) return 0;
        d[0] = (char)(0xF0 | (cp >> 18));
        d[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        d[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        d[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

int ut_valido(const char *s, int n) {
    int i = 0;
    while (i < n) {
        int av = 0;
        unsigned cp = ut_dec(s + i, n - i, &av);
        if (cp == UT_REEMPLAZO) return 0;
        if (av <= 0) return 0;
        i += av;
    }
    return 1;
}

int ut_largo(const char *s) {
    int n = 0;
    while (*s) {
        int av = 0;
        ut_dec(s, 4, &av);
        if (av <= 0) av = 1;
        s += av;
        n++;
    }
    return n;
}

int ut_anterior(const char *s, int pos) {
    int p = pos - 1;
    if (p < 0) return 0;
    while (p > 0 && ((unsigned char)s[p] & 0xC0) == 0x80) p--;
    return p;
}

int ut_siguiente(const char *s, int pos) {
    int av = 0;
    if (!s[pos]) return pos;
    ut_dec(s + pos, 4, &av);
    if (av <= 0) av = 1;
    return pos + av;
}

unsigned ut_compone(unsigned base, unsigned comb) {
    /* Acentos latinos habituales (ES/CA). Tabla explicita y acotada:
       no toca nada fuera de este repertorio. */
    unsigned b = base;
    if (b >= 'A' && b <= 'Z') b += 32;
    else if (b == 0xD1) b = 0xF1;   /* Ñ->ñ para virgulilla */
    if (comb == 0x301) {            /* agudo */
        switch (b) {
        case 'a': return base >= 'A' && base <= 'Z' ? 0xC1 : 0xE1;
        case 'e': return base >= 'A' && base <= 'Z' ? 0xC9 : 0xE9;
        case 'i': return base >= 'A' && base <= 'Z' ? 0xCD : 0xED;
        case 'o': return base >= 'A' && base <= 'Z' ? 0xD3 : 0xF3;
        case 'u': return base >= 'A' && base <= 'Z' ? 0xDA : 0xFA;
        case 'y': return base >= 'A' && base <= 'Z' ? 0xDD : 0xFD;
        }
    } else if (comb == 0x300) {     /* grave */
        switch (b) {
        case 'a': return base >= 'A' && base <= 'Z' ? 0xC0 : 0xE0;
        case 'e': return base >= 'A' && base <= 'Z' ? 0xC8 : 0xE8;
        case 'i': return base >= 'A' && base <= 'Z' ? 0xCC : 0xEC;
        case 'o': return base >= 'A' && base <= 'Z' ? 0xD2 : 0xF2;
        case 'u': return base >= 'A' && base <= 'Z' ? 0xD9 : 0xF9;
        }
    } else if (comb == 0x302) {     /* circunflejo */
        switch (b) {
        case 'a': return base >= 'A' && base <= 'Z' ? 0xC2 : 0xE2;
        case 'e': return base >= 'A' && base <= 'Z' ? 0xCA : 0xEA;
        case 'i': return base >= 'A' && base <= 'Z' ? 0xCE : 0xEE;
        case 'o': return base >= 'A' && base <= 'Z' ? 0xD4 : 0xF4;
        case 'u': return base >= 'A' && base <= 'Z' ? 0xDB : 0xFB;
        }
    } else if (comb == 0x308) {     /* dieresis */
        switch (b) {
        case 'a': return base >= 'A' && base <= 'Z' ? 0xC4 : 0xE4;
        case 'e': return base >= 'A' && base <= 'Z' ? 0xCB : 0xEB;
        case 'i': return base >= 'A' && base <= 'Z' ? 0xCF : 0xEF;
        case 'o': return base >= 'A' && base <= 'Z' ? 0xD6 : 0xF6;
        case 'u': return base >= 'A' && base <= 'Z' ? 0xDC : 0xFC;
        case 'y': return base >= 'A' && base <= 'Z' ? 0x178 : 0xFF;
        }
    } else if (comb == 0x303) {     /* virgulilla */
        switch (b) {
        case 'a': return base >= 'A' && base <= 'Z' ? 0xC3 : 0xE3;
        case 'n': return base >= 'A' && base <= 'Z' ? 0xD1 : 0xF1;
        case 'o': return base >= 'A' && base <= 'Z' ? 0xD5 : 0xF5;
        }
    } else if (comb == 0x327) {     /* cedilla */
        if (b == 'c') return base >= 'A' && base <= 'Z' ? 0xC7 : 0xE7;
    }
    return 0;
}

int ut_nfc(const char *s, char *d, int cap) {
    int t = 0;
    while (*s && t < cap - 1) {
        int av = 0;
        unsigned cp = ut_dec(s, 4, &av);
        int av2 = 0;
        unsigned nx = 0;
        if (av <= 0) {
            av = 1;
            cp = UT_REEMPLAZO;
        }
        if (s[av]) nx = ut_dec(s + av, 4, &av2);
        if (nx >= 0x300 && nx <= 0x308 && av2 > 0) {
            unsigned c = ut_compone(cp, nx);
            if (c) {
                int w = ut_enc(d + t, cap - t, c);
                if (!w) break;
                t += w;
                s += av + av2;
                continue;
            }
        }
        {
            int w = ut_enc(d + t, cap - t, cp);
            if (!w) break;
            t += w;
            s += av;
        }
    }
    if (t < cap) d[t] = 0;
    return t;
}

int ut_ancho(const char *s) {
    return ut_largo(s);
}
