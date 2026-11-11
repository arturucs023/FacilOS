#ifndef F32_CDCAT_H
#define F32_CDCAT_H
#include <stdint.h>

/* Catalogo de programas DENTRO de la ISO (solo lectura).
   Formato (sector CD_CAT_LBA del CD, 2048 B):
     byte 0..3  "F32C"   firma
     byte 4     version (1)
     byte 5     n_entries
     byte 6..7  reservado
     byte 8..11 primer sector de datos (LBA)
     byte 12+   entradas de 32 B: nombre[15], tipo, lba u32, tam u32
   Los datos de los programas van en sectores ISO consecutivos, alineados
   a 2048 B. Sin escritura: el CD es de solo lectura. */

#define CD_CAT_LBA 40
#define CD_CAT_DAT 41
/* v2.0-etapa5: 48 entradas (2048 B: 12 + 48x32 = 1548). El constructor
   reserva 4 sectores; con 32 se quedaban fuera menu/calculadora/reloj
   al crecer la lista a 37 programas. CD_CAT_ENT es el tamano de
   entrada (32 B, formato fijo); CD_CAT_MAX el conteo.
   Hito TLS: 63 entradas (12 + 63x32 = 2028 <= 2048, sin cambiar el
   formato ni los sectores). */
#define CD_CAT_MAX 63
#define CD_CAT_ENT 32
#define CD_CAT_NOM 15

int cdcat_cuantos(void);
const char *cdcat_nombre(int i);
int cdcat_tam(int i);
/* Busca por nombre -> indice o -1. */
int cdcat_buscar(const char *nombre);
/* Lee n bytes desde off -> bytes leidos (0 = fin). */
int cdcat_leer(int i, uint32_t off, void *buf, uint32_t n);
/* Carga el ejecutable completo (cabecera + plano) en 0x200000.
   Devuelve 0 ok, -1 no existe, -2 tamano excesivo, -3 error de lectura. */
int cdcat_carga(int i);

void cdcat_inicia(void);

#endif
