#include "fs.h"
#include "ata.h"
#include "blk.h"
#include "libc.h"

#define LBA_CAB 0
#define LBA_TAB 1
#define LBA_DAT 7

static uint8_t tabla[FS_N * FS_TAM_ENT];
static uint8_t datos[FS_N][512];
static uint8_t cab[512];
static uint8_t tabsec[6 * 512];
int fs_disco_ok = 0;
int fs_migrar = 0;
int fs_inicializado = 0;
static int dir_actual = 0;
extern int teclado_es;

static uint8_t *ent(int i) { return tabla + i * FS_TAM_ENT; }
static uint16_t e_tam(int i) {
    return (uint16_t)(ent(i)[34] | (ent(i)[35] << 8));
}
static void e_pon_tam(int i, uint16_t t) {
    ent(i)[34] = (uint8_t)t;
    ent(i)[35] = (uint8_t)(t >> 8);
}
/* v1.6: [36..37] seg_ini (0 = heredado: un sector en idx), [38..41] suma.
   Los ficheros heredados tienen estos bytes a cero (fechas sin usar). */
static uint16_t e_seg(int i) {
    return (uint16_t)(ent(i)[36] | (ent(i)[37] << 8));
}
static void e_pon_seg(int i, uint16_t s) {
    ent(i)[36] = (uint8_t)s;
    ent(i)[37] = (uint8_t)(s >> 8);
}
static uint32_t e_suma(int i) {
    return (uint32_t)ent(i)[38] | ((uint32_t)ent(i)[39] << 8) |
           ((uint32_t)ent(i)[40] << 16) | ((uint32_t)ent(i)[41] << 24);
}
static void e_pon_suma(int i, uint32_t s) {
    ent(i)[38] = (uint8_t)s;
    ent(i)[39] = (uint8_t)(s >> 8);
    ent(i)[40] = (uint8_t)(s >> 16);
    ent(i)[41] = (uint8_t)(s >> 24);
}
static uint32_t nsec_de(uint32_t tam) { return (tam + 511) / 512; }

/* v2.5: sin disco de datos (la ISO es de solo lectura) el area de datos
   extendida tambien vive en RAM, para que los ficheros de varios sectores
   funcionen igual que con disco, sin persistencia. 512 sectores = 256 KB
   en 0x1A00000 (hace falta ~32 MB de RAM; QEMU arranca con 32 MB). */
#define POOL_SEC 512
#define POOL_DIR ((uint8_t *)0x1A00000)

/* Sectores de datos disponibles: con disco, hasta el final de la imagen;
   sin disco, los 64 del espejo + los 512 del pool. */
static uint32_t sec_max(void) {
    return fs_disco_ok ? (uint32_t)(FS_LBA_MAX - LBA_DAT)
                       : (uint32_t)(FS_N + POOL_SEC);
}

/* Tamano maximo por fichero. Con disco no hay limite extra (el del formato
   es FS_MAX_TAM16 y lo comprueba cada escritura); sin disco, el pool RAM
   permite multi-sector hasta 256 KB. */
static uint32_t fs_tam_max(void) {
    return fs_disco_ok ? (uint32_t)FS_MAX_TAM16 : 512u * POOL_SEC;
}

/* sector de datos ds (0..) <-> medio. <64: espejo RAM (write-back);
   >=64: disco directo (write-through) o pool RAM si no hay disco. 0 ok. */
static uint8_t sec_tmp[512];
static int sec_lee(uint16_t ds, uint8_t *b) {
    if (ds < FS_N) {
        libc_memcpy(b, datos[ds], 512);
        return 0;
    }
    if (!fs_disco_ok) {
        uint32_t k = (uint32_t)ds - FS_N;
        if (k >= POOL_SEC) return -1;
        libc_memcpy(b, POOL_DIR + k * 512, 512);
        return 0;
    }
    /* Etapa3: por la capa de bloques (valida rangos con IDENTIFY). */
    return blk_leer(LBA_DAT + ds, b, 1);
}
static int sec_pon(uint16_t ds, const uint8_t *b) {
    if (ds < FS_N) {
        libc_memcpy(datos[ds], b, 512);
        return 0;
    }
    if (!fs_disco_ok) {
        uint32_t k = (uint32_t)ds - FS_N;
        if (k >= POOL_SEC) return -1;
        libc_memcpy(POOL_DIR + k * 512, b, 512);
        return 0;
    }
    return blk_escribir(LBA_DAT + ds, b, 1);
}
/* sector k-esimo del fichero -> ds. 0 ok. */
static int sec_de(int idx, uint32_t k, uint16_t *ds) {
    uint16_t seg = e_seg(idx);
    if (!seg) {
        if (k) return -1;
        *ds = (uint16_t)idx;
        return 0;
    }
    uint32_t d = (uint32_t)seg + k;
    if (d >= sec_max()) return -1;
    *ds = (uint16_t)d;
    return 0;
}
/* suma de los tam primeros bytes del fichero (verificacion). */
static uint32_t suma_fich(int idx, uint16_t tam) {
    uint32_t s = 0;
    uint32_t k = 0;
    while (k * 512 < tam) {
        uint16_t ds;
        if (sec_de(idx, k, &ds)) break;
        if (sec_lee(ds, sec_tmp)) break;
        uint32_t n = tam - k * 512;
        if (n > 512) n = 512;
        for (uint32_t j = 0; j < n; j++) s += sec_tmp[j];
        k++;
    }
    return s;
}
/* primer tramo libre de nsec en [64, limite). -1 si no hay. excluye yo. */
static int tramo_libre(uint32_t nsec, int yo) {
    uint32_t lim = sec_max();
    for (uint32_t base = FS_SEG_MIN; base + nsec <= lim; base++) {
        int choca = 0;
        for (int i = 0; i < FS_N && !choca; i++) {
            if (i == yo || ent(i)[0] != 2) continue;
            uint16_t sg = e_seg(i);
            if (!sg) continue;
            uint32_t n2 = nsec_de(e_tam(i));
            if (base < sg + n2 && sg < base + nsec) choca = 1;
        }
        if (!choca) return (int)base;
    }
    return -1;
}

static void fs_limpia(void) {
    fs_disco_ok = 0;
    libc_memset(tabla, 0, sizeof(tabla));
    ent(0)[0] = 1;
    ent(0)[2] = '/';
}

static uint32_t checksum_tabla(void) {
    uint32_t s = 0;
    for (uint32_t i = 0; i < sizeof(tabla); i++) s += tabla[i];
    return s;
}

static int nombre_ok(const char *n) {
    uint32_t l = libc_strlen(n);
    if (!l || l > 31) return 0;
    if (n[0] == '.' && (l == 1 || (l == 2 && n[1] == '.'))) return 0;
    for (uint32_t i = 0; i < l; i++) {
        /* v2.2: se admiten espacios y UTF-8 (bytes opacos, NUL y '/'
           siguen prohibidos). Longitud en BYTES (menos caracteres si
           hay multibyte). Compatible: todo nombre v1.x sigue valiendo. */
        if (n[i] == '/' || n[i] == 0) return 0;
    }
    return 1;
}

void fs_init(void) {
    if (fs_inicializado) return;    fs_inicializado = 1;
    fs_limpia();
    fs_disco_ok = 0;
    fs_migrar = 0;
    dir_actual = 0;
    if (blk_leer(LBA_CAB, cab, 1)) return;    if (libc_memcmp(cab, "FACILOS2", 8) != 0) {
        if (libc_memcmp(cab, "FACILOS1", 8) == 0) {
            /* v1.1: convertir a RAM, sin tocar disco */
            if (blk_leer(1, tabsec, 5)) return;
            uint8_t *tab11 = tabsec;
            uint16_t lo = cab[8] | (cab[9] << 8);
            uint16_t hi = cab[10] | (cab[11] << 8);
            uint32_t s = 0;
            for (uint32_t i = 0; i < 64 * 36; i++) s += tab11[i];
            for (int i = 0; i < 64; i++) {
                if (blk_leer(6 + i, datos[i], 1)) return;
                for (int j = 0; j < 512; j++) s += datos[i][j];
            }
            if ((s & 0xFFFF) != lo || (s >> 16) != hi) return;
            for (int i = 0; i < 64; i++) {
                uint8_t *o = tab11 + i * 36;
                uint8_t *d = ent(i);
                d[0] = o[0];
                d[1] = o[1];
                for (int j = 0; j < 32; j++) d[2 + j] = o[2 + j];
                d[34] = o[34];
                d[35] = o[35];
                d[36] = d[37] = d[38] = d[39] = d[40] = d[41] = 0;
            }
            fs_disco_ok = 2;
            fs_migrar = 1;
        }
        return;
    }
    /* v1.2 */
    if (blk_leer(LBA_TAB, tabsec, 6)) { fs_limpia(); return; }
    libc_memcpy(tabla, tabsec, sizeof(tabla));
    uint16_t lo = cab[8] | (cab[9] << 8);
    uint16_t hi = cab[10] | (cab[11] << 8);
    if (cab[12] != 2) { fs_limpia(); return; }
    uint32_t s = checksum_tabla();
    for (int i = 0; i < 64; i++) {
        if (blk_leer(LBA_DAT + i, datos[i], 1)) { fs_limpia(); return; }
        for (int j = 0; j < 512; j++) s += datos[i][j];
    }
    if ((s & 0xFFFF) != lo || (s >> 16) != hi) { fs_limpia(); return; }
    if (ent(0)[0] != 1) { fs_limpia(); return; }
    /* A partir de aqui sec_lee/sec_pon usan el disco: sin marcarlo, el pool
       RAM de v1.8 responderia y las sumas de los ficheros no saldrian. */
    fs_disco_ok = 1;
    /* v1.6: valida tramos extendidos (seg/suma). Sin esto, disco malo. */
    for (int i = 0; i < FS_N; i++) {
        if (ent(i)[0] != 2) continue;
        uint16_t t = e_tam(i);
        uint16_t sg = e_seg(i);
        if (!t) continue;
        if (!sg) {
            if (t > FS_MAX_TAM) { fs_limpia(); return; }
            continue;
        }
        if (sg < FS_SEG_MIN) { fs_limpia(); return; }
        if ((uint32_t)sg + nsec_de(t) > (uint32_t)(FS_LBA_MAX - LBA_DAT)) {
            fs_limpia();
            return;
        }
        if (suma_fich(i, t) != e_suma(i)) { fs_limpia(); return; }
    }
    if (cab[14] <= 1) teclado_es = cab[14];
    {
        extern void distribucion_pon(int d);
        if (cab[15] >= 1 && cab[15] <= 2) distribucion_pon((int)cab[15]);
        else distribucion_pon(teclado_es ? 1 : 0);
    }
}

/* Entrada para el registro de drivers (v2.0): el FS RAM siempre inicia. */
int ramfs_init(void) {
    fs_init();
    return 0;
}

int fs_guardar(void) {
    /* v1.8: sin disco de datos el sistema de archivos vive solo en RAM
       (tabla y datos ya son arrays en memoria): guardar es un no-op con
       exito, asi el usuario puede crear/editar ficheros aunque el sistema
       no sea persistente. El comando "guardar" de la shell avisa de que
       no hay disco (fs_disco_ok == 0). */
    if (!fs_disco_ok) return 0;
    uint32_t s = checksum_tabla();
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 512; j++) s += datos[i][j];
        if (blk_escribir(LBA_DAT + i, datos[i], 1)) return -1;
    }
    libc_memcpy(tabsec, tabla, sizeof(tabla));
    libc_memset(tabsec + sizeof(tabla), 0, sizeof(tabsec) - sizeof(tabla));
    if (blk_escribir(LBA_TAB, tabsec, 6)) return -1;
    uint8_t cab2[512];
    libc_memset(cab2, 0, 512);
    libc_memcpy(cab2, "FACILOS2", 8);
    cab2[8] = (uint8_t)s;
    cab2[9] = (uint8_t)(s >> 8);
    cab2[10] = (uint8_t)(s >> 16);
    cab2[11] = (uint8_t)(s >> 24);
    cab2[12] = 2;
    cab2[14] = (uint8_t)(teclado_es ? 1 : 0);
    {
        extern int distribucion_actual(void);
        cab2[15] = (uint8_t)distribucion_actual();
    }
    if (blk_escribir(LBA_CAB, cab2, 1)) return -1;
    /* Etapa3: vacia la cache del dispositivo; sin esto, un corte antes de
       que QEMU escriba deja el volumen a medio sincronizar. El error se
       propaga (no se oculta): los datos pueden seguir solo en cache. */
    if (blk_flush()) return -1;
    fs_disco_ok = 1;
    fs_migrar = 0;
    return 0;
}

int fs_actual(void) { return dir_actual; }
void fs_pon_actual(int idx) { dir_actual = idx; }
int fs_tipo(int i) { return (i < 0 || i >= FS_N) ? 0 : ent(i)[0]; }
int fs_padre(int i) { return (i < 0 || i >= FS_N) ? 0 : ent(i)[1]; }
const char *fs_nombre(int i) {
    static char nbs[3][33];
    static int q = 0;
    char *b = nbs[q = (q + 1) % 3];
    if (i < 0 || i >= FS_N) { b[0] = 0; return b; }
    for (int j = 0; j < 32; j++) b[j] = (char)ent(i)[2 + j];
    b[32] = 0;
    return b;
}
uint16_t fs_tam(int i) { return (i < 0 || i >= FS_N) ? 0 : e_tam(i); }
int fs_renombrar(int idx, const char *nuevo) {
    if (idx <= 0 || idx >= FS_N || !ent(idx)[0]) return -1;
    char tmp[33];
    for (int j = 0; j < 32; j++) tmp[j] = (char)ent(idx)[2 + j];
    tmp[32] = 0;
    if (!libc_strcmp(tmp, nuevo)) return 0;
    if (!nombre_ok(nuevo)) return -1;
    if (fs_hijo(ent(idx)[1], nuevo) >= 0) return -2;
    libc_memset(ent(idx) + 2, 0, 32);
    for (int j = 0; j < 32 && nuevo[j]; j++) ent(idx)[2 + j] = (uint8_t)nuevo[j];
    return 0;
}
int fs_mover(int idx, int nuevo_padre, const char *nuevo_nom) {
    if (idx <= 0 || idx >= FS_N || !ent(idx)[0]) return -1;
    if (nuevo_padre < 0 || nuevo_padre >= FS_N) return -1;
    if (ent(nuevo_padre)[0] != 1 && nuevo_padre != 0) return -1;
    if (ent(idx)[0] == 1) {
        int p = nuevo_padre;
        while (p) {
            if (p == idx) return -2;
            p = ent(p)[1];
        }
    }
    if (nuevo_nom) {
        int r = fs_renombrar(idx, nuevo_nom);
        if (r == -2) return -2;
        if (r) return -1;
    } else {
        char tmp[33];
        for (int j = 0; j < 32; j++) tmp[j] = (char)ent(idx)[2 + j];
        tmp[32] = 0;
        if (fs_hijo(nuevo_padre, tmp) >= 0 && fs_hijo(nuevo_padre, tmp) != idx)
            return -2;
    }
    ent(idx)[1] = (uint8_t)nuevo_padre;
    return 0;
}

int fs_hijo(int dir, const char *nom) {
    for (int i = 0; i < FS_N; i++) {
        if (!ent(i)[0] || ent(i)[1] != dir) continue;
        char tmp[33];
        for (int j = 0; j < 32; j++) tmp[j] = (char)ent(i)[2 + j];
        tmp[32] = 0;
        if (libc_strcmp(tmp, nom) == 0) return i;
    }
    return -1;
}

int fs_resuelve(const char *ruta) {
    int dir = (ruta[0] == '/') ? 0 : dir_actual;
    char tok[32];
    uint32_t ti = 0;
    for (uint32_t i = 0; ; i++) {
        char c = ruta[i];
        if (c == '/' || !c) {
            if (ti) {
                tok[ti] = 0;
                if (libc_strcmp(tok, ".") == 0) { }
                else if (libc_strcmp(tok, "..") == 0) {
                    if (dir) dir = ent(dir)[1];
                } else {
                    int h = fs_hijo(dir, tok);
                    if (h < 0 || ent(h)[0] != 1) return -1;
                    dir = h;
                }
                ti = 0;
            }
            if (!c) break;
        } else {
            if (ti < 31) tok[ti++] = c;
            else return -1;
        }
    }
    return dir;
}

int fs_libres(void) {
    int n = 0;
    for (int i = 0; i < FS_N; i++) if (!ent(i)[0]) n++;
    return n;
}

int fs_crear(int tipo, int padre, const char *nom) {
    if (!nombre_ok(nom)) return -1;
    /* F8: valida tipo y padre (antes un padre/tipo arbitrario se
     * almacenaba truncado y ensuciaba el indice). */
    if (tipo != 1 && tipo != 2) return -1;
    if (padre < 0 || padre >= FS_N || ent(padre)[0] != 1) return -1;
    if (fs_hijo(padre, nom) >= 0) return -2;
    for (int i = 1; i < FS_N; i++) {
        if (ent(i)[0]) continue;
        ent(i)[0] = (uint8_t)tipo;
        ent(i)[1] = (uint8_t)padre;
        libc_memset(ent(i) + 2, 0, 32);
        for (int j = 0; j < 32 && nom[j]; j++) ent(i)[2 + j] = (uint8_t)nom[j];
        e_pon_tam(i, 0);
        for (int j = 36; j < 42; j++) ent(i)[j] = 0;
        libc_memset(datos[i], 0, 512);
        return i;
    }
    return -3;
}

int fs_leer_parcial(int idx, uint32_t off, uint8_t *buf, uint32_t n) {
    if (idx < 0 || idx >= FS_N || ent(idx)[0] != 2) return -1;
    uint16_t t = e_tam(idx);
    if (off > t) return -1;
    if (off == t || !n) return 0;
    if (n > (uint32_t)(t - off)) n = t - off;
    uint32_t hechas = 0;
    while (hechas < n) {
        uint32_t pos = off + hechas;
        uint16_t ds;
        if (sec_de(idx, pos / 512, &ds)) return -1;
        if (sec_lee(ds, sec_tmp)) return -1;
        uint32_t ini = pos % 512, trozo = 512 - ini;
        if (trozo > n - hechas) trozo = n - hechas;
        libc_memcpy(buf + hechas, sec_tmp + ini, trozo);
        hechas += trozo;
    }
    return (int)hechas;
}

int fs_leer(int idx, uint8_t *buf) {
    if (idx < 0 || idx >= FS_N || ent(idx)[0] != 2) return -1;
    return fs_leer_parcial(idx, 0, buf, e_tam(idx));
}

/* reubica el fichero a un tramo nuevo (copia el contenido viejo). */
static int reubica(int idx, uint32_t oldn, uint32_t need, uint16_t oseg) {
    int base = tramo_libre(need, idx);
    if (base < 0) return -1;
    for (uint32_t k = 0; k < oldn; k++) {
        uint16_t dsv;
        if (!oseg) {
            if (k) return -1;
            dsv = (uint16_t)idx;
        } else dsv = (uint16_t)(oseg + k);
        if (sec_lee(dsv, sec_tmp)) return -1;
        if (sec_pon((uint16_t)(base + k), sec_tmp)) return -1;
    }
    e_pon_seg(idx, (uint16_t)base);
    return 0;
}

static int parche(int idx, uint32_t off, const uint8_t *buf, uint32_t n) {
    uint32_t hechas = 0;
    while (hechas < n) {
        uint32_t pos = off + hechas;
        uint16_t ds;
        if (sec_de(idx, pos / 512, &ds)) return -1;
        if (sec_lee(ds, sec_tmp)) return -1;
        uint32_t ini = pos % 512, trozo = 512 - ini;
        if (trozo > n - hechas) trozo = n - hechas;
        libc_memcpy(sec_tmp + ini, buf + hechas, trozo);
        if (sec_pon(ds, sec_tmp)) return -1;
        hechas += trozo;
    }
    return 0;
}

int fs_escribir_parcial(int idx, uint32_t off, const uint8_t *buf,
                         uint32_t n) {
    if (idx < 0 || idx >= FS_N || ent(idx)[0] != 2) return -1;
    if (!n) return 0;
    uint16_t t = e_tam(idx);
    if (off > t) return -1;
    /* F8: fin en 64 bit: off+n podia envolver u32 y eludir el tope. */
    {
        uint64_t fin64 = (uint64_t)off + (uint64_t)n;
        if (fin64 > FS_MAX_TAM16) return -1;
    }
    uint32_t fin = off + n;
    uint16_t ntam = fin > t ? (uint16_t)fin : t;
    if (ntam > fs_tam_max()) return -1;
    uint32_t need = nsec_de(ntam), old = nsec_de(t);
    uint16_t seg = e_seg(idx);
    if (need > old || (!seg && need > 1)) {
        if (reubica(idx, old, need, seg)) return -1;
    }
    if (parche(idx, off, buf, n)) return -1;
    e_pon_tam(idx, ntam);
    e_pon_suma(idx, suma_fich(idx, ntam));
    return 0;
}

int fs_truncar(int idx, uint32_t tam) {
    if (idx < 0 || idx >= FS_N || ent(idx)[0] != 2) return -1;
    if (tam > FS_MAX_TAM16) return -1;
    uint16_t t = e_tam(idx);
    if (tam == t) return 0;
    if (tam > fs_tam_max()) return -1;
    if (tam < t) {
        e_pon_tam(idx, (uint16_t)tam);
        e_pon_suma(idx, suma_fich(idx, (uint16_t)tam));
        return 0;
    }
    uint32_t need = nsec_de(tam), old = nsec_de(t);
    uint16_t seg = e_seg(idx);
    if (need > old || (!seg && need > 1)) {
        if (reubica(idx, old, need, seg)) return -1;
    }
    /* ceros en [t, tam) */
    uint32_t pos = t;
    while (pos < tam) {
        uint16_t ds;
        if (sec_de(idx, pos / 512, &ds)) return -1;
        if (sec_lee(ds, sec_tmp)) return -1;
        uint32_t ini = pos % 512, trozo = 512 - ini;
        if (trozo > tam - pos) trozo = tam - pos;
        libc_memset(sec_tmp + ini, 0, trozo);
        if (sec_pon(ds, sec_tmp)) return -1;
        pos += trozo;
    }
    e_pon_tam(idx, (uint16_t)tam);
    return 0;
}

int fs_escribir(int idx, const uint8_t *buf, uint32_t tam) {
    if (idx < 0 || idx >= FS_N || ent(idx)[0] != 2) return -1;
    if (tam > FS_MAX_TAM16) return -1;
    if (fs_truncar(idx, tam)) return -1;
    if (tam && fs_escribir_parcial(idx, 0, buf, tam)) return -1;
    return 0;
}

int fs_borrar(int idx) {
    if (idx <= 0 || idx >= FS_N || !ent(idx)[0]) return -1;
    if (ent(idx)[0] == 1) {
        for (int i = 0; i < FS_N; i++)
            if (ent(i)[0] && ent(i)[1] == idx) return -2;
    }
    libc_memset(ent(idx), 0, FS_TAM_ENT);
    libc_memset(datos[idx], 0, 512);
    return 0;
}
