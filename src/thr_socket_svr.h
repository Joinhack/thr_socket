#ifndef THR_SOCKET_SVR_H
#define THR_SOCKET_SVR_H

#include "common.h"
#include "atomic.h"
#include "log.h"
#include "jmalloc.h"
#include "code.h"
#include "cevent.h"
#include "cnet.h"
#include "cio.h"
#include "cthread.h"
#include "network.h"
#include "obj.h"

typedef struct {
	int in_fd;
	int un_fd;
	int logfd;
	pid_t pid;
	uint32_t connections;
	long last_info_time;
	cevents *evts;
	dict *commands;
	volatile int running;
	cthr_pool *thr_pool;
} thr_socket_svr;

#ifdef __cplusplus
extern "C" {
#endif

thr_socket_svr *create_thr_server();

int server_init(thr_socket_svr *svr);

void *mainLoop(void *p);

void destroy_thr_server(thr_socket_svr *svr);

long long thrs_used_mem();

#ifdef __cplusplus
}
#endif

extern int threads;

extern int port;

#endif
