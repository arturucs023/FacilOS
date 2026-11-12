#include "cdcat.h"
#include "cd.h"
#include "libc.h"
#include "bootinfo.h"

typedef struct {
    char nom[CD_CAT_NOM + 1];
    uint8_t tipo;
    uint8_t lba[4];
    uint8_t tam[4];
} ent_t;

static uint8_t cab[2048];
static uint8_t hdr_f32x[20];
static ent_t ents[CD_CAT_MAX];
static int n_ents = 0;
static int listo = 0;
static uint32_t cat_lba = 40, dat_lba = 41;

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void cdcat_inicia(void) {
    listo = 0;
    n_ents = 0;
    /* LBA del catalogo: lo deja el arranque (0x7010). */
    cat_lba = *BI_CAT ? *BI_CAT : CD_CAT_LBA;
    dat_lba = *BI_DAT ? *BI_DAT : CD_CAT_DAT;
    const uint8_t *c = cd_sector(cat_lba);
    if (!c) return;
    libc_memcpy(cab, c, 2048);
    if (cab[0] != 'F' || cab[1] != '3' || cab[2] != '2' || cab[3] != 'C')
        return;
    if (cab[4] != 1) return;
    n_ents = cab[5];
    if (n_ents > CD_CAT_MAX) n_ents = CD_CAT_MAX;
    for (int i = 0; i < n_ents; i++) {
        const uint8_t *e = cab + 12 + i * CD_CAT_ENT;
        for (int k = 0; k < CD_CAT_NOM; k++)
            ents[i].nom[k] = (char)e[k];
        ents[i].nom[CD_CAT_NOM] = 0;
        ents[i].tipo = e[15];
        for (int k = 0; k < 4; k++) ents[i].lba[k] = e[16 + k];
        for (int k = 0; k < 4; k++) ents[i].tam[k] = e[20 + k];
    }
    listo = 1;
}

int cdcat_cuantos(void) { return listo ? n_ents : 0; }

int cdcat_lba(int i) {
    if (!listo || i < 0 || i >= n_ents) return -1;
    return (int)rd32(ents[i].lba);
}

const char *cdcat_nombre(int i) {
    if (!listo || i < 0 || i >= n_ents) return 0;
    return ents[i].nom;
}

int cdcat_tam(int i) {
    if (!listo || i < 0 || i >= n_ents) return 0;
    return (int)rd32(ents[i].tam);
}

int cdcat_buscar(const char *nombre) {
    if (!listo) return -1;
    for (int i = 0; i < n_ents; i++) {
        if (libc_strcmp(ents[i].nom, nombre) == 0) return i;
    }
    return -1;
}

int cdcat_leer(int i, uint32_t off, void *buf, uint32_t n) {
    int pedido;
    if (!listo || i < 0 || i >= n_ents) return 0;
    uint32_t tam = rd32(ents[i].tam);
    /* F8: tam del catalogo sin verificar (ISO manipulada): el mismo
     * tope que el cargador (plano+bss<=0x300000 + 20 de cabecera). */
    if (tam < 16 || tam > 0x300014u) return 0;
    if (off >= tam) return 0;
    if (n > tam - off) n = tam - off;
    pedido = (int)n;
    uint8_t *dest = (uint8_t *)buf;
    uint32_t lba0 = rd32(ents[i].lba);
    while (n) {
        uint32_t pos = off;
        uint32_t dentro = pos % 512;
        uint32_t trozo = 512 - dentro;
        const uint8_t *s = cd_sector(lba0 + pos / 512);
        if (!s) return pedido - (int)n;
        if (trozo > n) trozo = n;
        libc_memcpy(dest, s + dentro, trozo);
        dest += trozo;
        off += trozo;
        n -= trozo;
    }
    return pedido;
}

/* Copia el programa i a 0x200000 SIN la cabecera F32X, igual que hace el
   cargador del disco: el plano esta enlazado para cargarse en 0x200000
   (prog.ld) y la entrada es ent (offset sobre el plano). La cabecera leida
   se guarda en hdr_f32x para que la valide cdcat_prepara. */
int cdcat_carga(int i) {
    static uint8_t trozo[512];
    if (!listo || i < 0 || i >= n_ents) return -1;
    uint32_t tam = rd32(ents[i].tam);
    uint32_t ver, hc;
    if (tam < 16 || tam > 65535) return -2;
    if (cdcat_leer(i, 0, hdr_f32x, 20) != 20) return -3;
    if (hdr_f32x[0] != 'F' || hdr_f32x[1] != '3' ||
        hdr_f32x[2] != '2' || hdr_f32x[3] != 'X') return -2;
    ver = rd32(hdr_f32x + 4);
    hc = 16;
    if (ver == 2 || ver == 3) hc = 20;
    else if (ver != 1) return -2;
    if (tam <= hc) return -2;
    uint8_t *dest = (uint8_t *)0x200000;
    uint32_t off = hc;
    while (off < tam) {
        uint32_t n = tam - off > 512 ? 512 : tam - off;
        if (cdcat_leer(i, off, trozo, n) != (int)n) return -3;
        libc_memcpy(dest + (off - hc), trozo, n);
        off += n;
    }
    return 0;
}

/* Valida la cabecera que leyo cdcat_carga, pone el BSS a cero y devuelve
   el offset de entrada en el plano (-1 si no es un F32X valido). */
int cdcat_prepara(void) {
    uint8_t *d = (uint8_t *)0x200000;
    if (hdr_f32x[0] != 'F' || hdr_f32x[1] != '3' ||
        hdr_f32x[2] != '2' || hdr_f32x[3] != 'X') return -1;
    uint32_t ver = rd32(hdr_f32x + 4);
    uint32_t ent = rd32(hdr_f32x + 8);
    uint32_t tam = rd32(hdr_f32x + 12);
    uint32_t hc = 16, bss = 0;
    if (ver == 2 || ver == 3) {
        bss = rd32(hdr_f32x + 16);
        hc = 20;
    } else if (ver != 1) {
        return -1;
    }
    uint32_t plano = tam < hc ? tam : tam - hc;
    /* v2.4.4: mismo tope que exec.c (3 MB). */
    if (ent >= plano || bss > 0x40000 || plano + bss > 0x300000)
        return -1;
    for (uint32_t k = 0; k < bss; k++) d[plano + k] = 0;
    return (int)ent;
}
