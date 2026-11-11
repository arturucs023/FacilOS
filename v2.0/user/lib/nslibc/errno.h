/* nslibc/errno.h: codigos que menciona el camino iconv
 * (compilado fuera con WITHOUT_ICONV_FILTER) mas ESPIPE para
 * nsutils/unistd. La variable existe (BSS+4, siempre 0 salvo que
 * codigo enlazado la toque); no hay perror/strerror. */
#ifndef F32NS_ERRNO_H
#define F32NS_ERRNO_H

#define EILSEQ 84
#define E2BIG 7
#define EINVAL 22
#define ESPIPE 29
#define EINTR 4

extern int errno;

#endif
