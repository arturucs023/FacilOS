/* nslibc/sys/time.h: timeval minimo freestanding para NetSurf en F32 OS.
 * MinGW-w64 14+ ya no define struct timeval (POSIX.1-2024 elimino
 * gettimeofday); se provee aqui para no tocar fuentes NetSurf.
 * Sin UNIX/BSD/Linux. */
#ifndef F32NS_SYS_TIME_H
#define F32NS_SYS_TIME_H

struct timeval {
    long tv_sec;
    long tv_usec;
};

int gettimeofday(struct timeval *tv, void *tz);

#endif
