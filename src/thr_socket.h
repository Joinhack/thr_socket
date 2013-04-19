#ifndef THR_SOCKET_H
#define THR_SOCKET_H

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

#endif
