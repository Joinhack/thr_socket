#ifndef LOCK_H
#define LOCK_H

#include <pthread.h>

typedef pthread_mutex_t cmutex_t;

#define cmutex_init(m, a) pthread_mutex_init(m, a)

#define cmutex_lock(m) pthread_mutex_lock(m)

#define cmutex_unlock(m) pthread_mutex_unlock(m)

#define cmutex_destroy(m) pthread_mutex_destroy(m)

#ifdef USE_MUTEX
#define LOCK_T cmutex_t
#define LOCK_INIT(l) cmutex_init(l, NULL)
#define LOCK_DESTROY(l) cmutex_destroy(l)
#define LOCK(l) cmutex_lock(l)
#define UNLOCK(l) cmutex_unlock(l)
#else
#include "spinlock.h"
#define LOCK_T spinlock_t
#define LOCK_INIT(l) (*l)=SL_UNLOCK
#define LOCK_DESTROY(l) (*l)=SL_UNLOCK
#define LOCK(l) spinlock_lock(l);
#define UNLOCK(l) spinlock_unlock(l);
#endif


#endif /**end define lock*/
