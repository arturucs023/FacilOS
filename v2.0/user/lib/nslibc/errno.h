/* nslibc/errno.h: solo los codigos que menciona el camino iconv
 * (compilado fuera con WITHOUT_ICONV_FILTER; existen para que el
 * codigo que los nombre compile). Sin variable errno: si algo la
 * usara de verdad, fallaria al enlazar (ruido, no silencio). */
#ifndef F32NS_ERRNO_H
#define F32NS_ERRNO_H

#define EILSEQ 84
#define E2BIG 7
#define EINVAL 22

#endif
