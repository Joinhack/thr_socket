#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#ifndef CINLINE
#define CINLINE static inline //USE C99 keyword.
#endif

typedef uint64_t msec_t;

#ifdef __linux__
#define USE_EPOLL
#define USE_PROCFILE
#endif

#if (defined(__APPLE__)) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#define USE_KQUEUE
#define USE_TASKINFO
#endif

#ifdef __cplusplus
extern "C" {
#endif

void time_now(long *s, int *ms);

int str2ll(char *p, size_t len, long long *l);

int lltostr(long long l, char *p, size_t len);

int str2l(char *p, size_t len, long *l);
#ifdef __cplusplus
}
#endif

#endif /*end define common head*/
