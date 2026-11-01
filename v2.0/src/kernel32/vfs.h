#ifndef F32_VFS_H
#define F32_VFS_H
#include <stdint.h>

/* VFS v2.0 (entrega 7): interfaz de servicio de ficheros para procesos.
 *
 * El backend actual (`vfs_ramfs`, en vfs.c) delega en fs.c sin cambiar su
 * comportamiento. Un futuro almacenamiento persistente implementara esta
 * misma tabla y el montaje elegira backend SIN tocar aplicaciones ni
 * syscalls. La shell usa fs.c directamente (cliente interno del backend
 * RAM, incluidas operaciones de directorios).
 */

typedef struct {
    int (*hijo)(int dir, const char *nombre);       /* idx o -1 */
    int (*tipo)(int idx);                            /* 0/1/2 */
    uint16_t (*tam)(int idx);
    int (*crear)(int tipo, int padre, const char *nombre);
    int (*leer_parcial)(int idx, uint32_t off, uint8_t *buf, uint32_t n);
    int (*escribir_parcial)(int idx, uint32_t off, const uint8_t *buf,
                            uint32_t n);
    int (*truncar)(int idx, uint32_t tam);
    int (*borrar)(int idx);
    int (*sincroniza)(void);                         /* persistencia */
} vfs_ops_t;

extern const vfs_ops_t *vfs_actual;   /* backend montado (ramfs) */

#endif
