#include "vfs.h"
#include "fs.h"

/* Backend ramfs: el FS en RAM actual (fs.c) tal cual, sin cambios. */

static int r_hijo(int d, const char *n) { return fs_hijo(d, n); }
static int r_tipo(int i) { return fs_tipo(i); }
static uint16_t r_tam(int i) { return fs_tam(i); }
static int r_crear(int t, int p, const char *n) { return fs_crear(t, p, n); }
static int r_leer(int i, uint32_t o, uint8_t *b, uint32_t n) {
    return fs_leer_parcial(i, o, b, n);
}
static int r_escribir(int i, uint32_t o, const uint8_t *b, uint32_t n) {
    return fs_escribir_parcial(i, o, b, n);
}
static int r_truncar(int i, uint32_t t) { return fs_truncar(i, t); }
static int r_borrar(int i) { return fs_borrar(i); }
static int r_sinc(void) { return fs_guardar(); }
static const char *r_nombre(int i) { return fs_nombre(i); }
static int r_padre(int i) { return fs_padre(i); }
static int r_renombrar(int i, const char *n) { return fs_renombrar(i, n); }

static const vfs_ops_t ramfs = {
    r_hijo, r_tipo, r_tam, r_crear, r_leer, r_escribir, r_truncar, r_borrar,
    r_sinc, r_nombre, r_padre, r_renombrar
};

const vfs_ops_t *vfs_actual = &ramfs;
