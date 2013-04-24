#ifndef NETWORK_H
#define NETWORK_H

#include "cthread.h"
#include "obj.h"

#define MAX_COMMAND_LEN_LIMIT 0x10240
#ifdef __cplusplus
extern "C" {
#endif

int tcp_accept_event_proc(cevents *cevts, int fd, void *priv, int mask);

int reply_str(cio *io, char *buff);

int reply_obj(cio *io, obj *obj);

int reply_cstr(cio *io, cstr s);

int reply_err(cio *io, const char *err);

int process_commond(cio *io);

void shared_obj_create();

void *process_event(void *priv);

int reply_bulk(cio *io, obj *obj);

int reply_len(cio *io, long long l);
#ifdef __cplusplus
}
#endif

#endif /*end define network*/
