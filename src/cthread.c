#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include "jmalloc.h"
#include "cthread.h"
#include "log.h"

static cthread *cthread_create(cthr_pool *p);
static void cthread_destroy(cthread  *thr);

void *thread_loop(void *data) {
	cthread *thr = (cthread*)data;
	cthr_pool *pool = thr->pool;
	for(;;) {
		pthread_mutex_lock(&pool->mutex);
		thr->state = THR_IDLE;
		if(pthread_cond_wait(&thr->cond, &pool->mutex) < 0) {
			//TODO: log it.
			ERROR("wait error\n");
			return NULL;
		}
		pthread_mutex_unlock(&pool->mutex);
		if(pool->state == THRP_EXIT) {
			thr->state = THR_EXIT;
			return NULL;
		}
		thr->state = THR_BUSY;
		if(thr->proc)
			thr->proc(thr->proc_data);
		thr->proc = NULL;
		thr->proc_data = NULL;
	}
}

static int cthread_init(cthread *thr, cthr_pool *pool) {
	pthread_attr_t  thr_attr;
	thr->pool = pool;
	if(pthread_cond_init(&thr->cond, NULL) < 0) {
		ERROR("init thread cond error\n");
		return -1;
	}
	if(pthread_attr_init(&thr_attr) < 0) {
		ERROR("init thread attr error\n");
		return -1;
	}
	if(pthread_create(&thr->thrid, &thr_attr, thread_loop, thr) < 0) {
		//TODO: log it.
		ERROR("create thread error\n");
		return -1;
	}
	return 0;
}

void cthr_pool_destroy(cthr_pool *pool) {
	cthread *thr;
	void *thr_ret;
	int i, ret;
	pool->state = THRP_EXIT;
	for(i = 0; i < pool->size; i++) {
		thr = pool->thrs + i;
		pthread_mutex_lock(&pool->mutex);
		ret = pthread_cond_signal(&thr->cond);
		pthread_mutex_unlock(&pool->mutex);
		if(ret < 0) {
			ERROR("%s\n", strerror(errno));
		}
		pthread_join(thr->thrid, &thr_ret);
		pthread_cond_destroy(&thr->cond);
	}
	pthread_mutex_destroy(&pool->mutex);
	jfree(pool->thrs);
	jfree(pool);
}

cthr_pool *cthr_pool_create(size_t size) {
	int i, ret;
	cthread *thr;
	cthr_pool *pool = (cthr_pool *)jmalloc(sizeof(cthr_pool));
	pool->thrs = jmalloc(sizeof(cthread)*size);
	ret = pthread_mutex_init(&pool->mutex, NULL);
	if(ret < 0) {
		//TODO: log it.
		ERROR("create thread pool error\n");
		jfree(pool);
		return NULL;
	}
	pool->size = size;
	pool->state = THRP_WORK;
	for(i = 0; i <  size; i++) {
		thr = pool->thrs + i;
		if(cthread_init(thr, pool) < 0) {
			cthr_pool_destroy(pool);
			return NULL;
		}
	}
	//wait for idle queue fill
	for(i = 0; i < size; i++) {
		thr = pool->thrs + i;
		if(thr->state != THR_IDLE)
			continue;
	}
	pool->state = THRP_WORK;
	return pool;
}

//return -1, all thread is busy
int cthr_pool_run_task(cthr_pool *pool, cthread_proc *proc, void *proc_data) {
	cthread *thr = NULL;
	int i;
	pthread_mutex_lock(&pool->mutex);
	for(i = 0; i < pool->size; i++) {
		thr = pool->thrs + i;
		if(thr->state == THR_IDLE)
			break;
	}
	if(thr == NULL)
		return -1;
	thr->proc = proc;
	thr->proc_data = proc_data;
	pthread_cond_signal(&thr->cond);
	pthread_mutex_unlock(&pool->mutex);
	return 0;
}
