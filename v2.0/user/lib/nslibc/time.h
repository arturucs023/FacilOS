/* nslibc/time.h: reloj minimo freestanding para BearSSL en F32 OS.
 * Solo time() sobre u_reloj (RTC del nucleo). Sin UNIX/BSD/Linux.
 * time_t de 64 bits como el CRT de MinGW (solo la firma; la
 * implementacion es nsshim.c). */
#ifndef F32NS_TIME_H
#define F32NS_TIME_H

#ifndef _TIME_T_DEFINED
typedef long long time_t;
#define _TIME_T_DEFINED
#endif

time_t time(time_t *t);

#endif
