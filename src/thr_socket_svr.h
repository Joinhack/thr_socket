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

struct shared_obj {
	obj *err;
	obj *pong;
	obj *ok;
	obj *cmd;
	obj *nullbulk;
};

#ifdef __cplusplus
extern "C" {
#endif

thr_socket_svr *create_thr_server();

int server_init(thr_socket_svr *svr);

void *mainLoop(void *p);

void destroy_thr_server(thr_socket_svr *svr);

long long thrs_used_mem();

void pong(cio *io);

void open_table_command(cio* io);

void mysql_thrs_destroy(clist *l);

void* mysql_thd_init(void *p);

void mysql_thds_destroy(thr_socket_svr *svr);

#ifdef __cplusplus
}
#endif

extern struct shared_obj shared;

extern int thread_num;

extern int port;

#endif
