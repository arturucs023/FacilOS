#ifndef F32_FS_H
#define F32_FS_H
#include <stdint.h>

#define FS_N 64
#define FS_TAM_ENT 42
#define FS_MAX_TAM 512
/* v1.6 multisector: tam hasta 65535; ver fs.c (campos seg/suma). */
#define FS_MAX_TAM16 65535
#define FS_SEG_MIN 64
#define FS_LBA_MAX 20480

/* disco_ok: 0 nada, 1 v1.2/vacio, 2 v1.1 migrado en RAM */
extern int fs_disco_ok;
extern int fs_migrar;
extern int fs_inicializado;

void fs_init(void);
int fs_guardar(void);
/* resolver ruta -> indice (0..63) o -1. Acepta abs/rel/../. */
int fs_resuelve(const char *ruta);
/* buscar hijo por nombre en dir -> indice o -1 */
int fs_hijo(int dir, const char *nom);
int fs_crear(int tipo, int padre, const char *nom);
int fs_leer(int idx, uint8_t *buf);
int fs_escribir(int idx, const uint8_t *buf, uint32_t tam);
int fs_borrar(int idx);
/* v1.6: E/S parcial por offset (0 ok). leer: devuelve leidos (0=EOF).
   escribir: crece si hace falta (off<=tam viejo); 0 ok. truncar: fija
   tam (crece con ceros); 0 ok. Requieren disco para tam>512. */
int fs_leer_parcial(int idx, uint32_t off, uint8_t *buf, uint32_t n);
int fs_escribir_parcial(int idx, uint32_t off, const uint8_t *buf,
                         uint32_t n);
int fs_truncar(int idx, uint32_t tam);int fs_libres(void);
int fs_actual(void);
void fs_pon_actual(int idx);
int fs_tipo(int i);
int fs_padre(int i);
const char *fs_nombre(int i);
uint16_t fs_tam(int i);
int fs_renombrar(int idx, const char *nuevo);
int fs_mover(int idx, int nuevo_padre, const char *nuevo_nom);

#endif
