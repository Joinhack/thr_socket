#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "thr_socket_svr.h"


static int read_event_proc(cevents *cevts, int fd, void *priv, int mask);

static int write_event_proc(cevents *cevts, int fd, void *priv, int mask);

static int _response(cio *io);

static int try_process_command(cio *io);

static void install_read_event(cevents *cevts, cio *io) {
	cevents_add_event(cevts, io->fd, CEV_READ|CEV_PERSIST, read_event_proc, io);
}

void set_protocol_error(cio *io) {
	io->flag |= IOF_CLOSE_AFTER_WRITE;
}

//always return 0, don't push fired event queue
int tcp_accept_event_proc(cevents *cevts, int fd, void *priv, int mask) {
	char buff[2048];
	char ip[24];
	int port;
	int clifd;
	thr_socket_svr *svr;
	cio *io;
	memset(buff, 0, sizeof(buff));
	memset(ip, 0, sizeof(ip));
	if((clifd = cnet_tcp_accept(fd, ip, &port, buff, sizeof(buff))) < 0) {
		ERROR("%s\n", buff);
		return -1;
	}
	//TODO: maybe there add cio queue.
	io = cio_create();
	strcpy(io->ip, ip);
	io->port = port;
	io->fd = clifd;
	io->type = IO_TCP;
	io->priv = priv;
	install_read_event(cevts, io);
	svr = (thr_socket_svr*)priv;
	atomic_add_uint32(&svr->connections, 1);
	return 1;
}

int cio_close_destroy(cevents *cevts, int fd, void *priv, int mask) {
	thr_socket_svr *svr;
	cio *io = (cio*)priv;
	DEBUG("client[%d] %s:%d closed\n", fd, io->ip, io->port);
	cevents_del_event(cevts, io->fd, CEV_READ|CEV_WRITE|CEV_PERSIST);	
	svr = (thr_socket_svr*)io->priv;
	atomic_sub_uint32(&svr->connections, 1);
	close(io->fd);
	cio_destroy(io);
	return 1;
}

static void cio_close_destroy_install(cio *io) {
	cevents *cevts = ((thr_socket_svr*)io->priv)->evts;
	cevents_add_event(cevts, io->fd, CEV_WRITE|CEV_PERSIST,cio_close_destroy, io);
	cevents_del_event(cevts, io->fd, CEV_READ);
}

int response(cio *io) {
	cevents *cevts = ((thr_socket_svr*)io->priv)->evts;
	int persist = io->mask & CEV_PERSIST;
	int rs = _response(io);

	//if not use CEV_PERSIST, when the event fired, all events is removed, so should rebind write event.
	if(!persist && rs == 1) {
		DEBUG("rebind write event\n");
		cevents_add_event(cevts, io->fd, CEV_WRITE, write_event_proc, io);
		return rs;
	}
	if(rs < 0) {
		cio_clear(io);
		cio_close_destroy_install(io);
		return rs;
	}

	if(io->flag & IOF_CLOSE_AFTER_WRITE) {
		cio_clear(io);
		cio_close_destroy_install(io);
		return 0;
	}
	cio_clear(io);

	//if use CEV_PERSIST, we should disable write event fired.
	if(persist) {
		install_read_event(cevts, io);
		cevents_del_event(cevts, io->fd, CEV_WRITE);
	} else {
		install_read_event(cevts, io);
		return 0;
	}
	return 0;
}

static int _reply(cio *io) {
	int rs;
	cevents *cevts = ((thr_socket_svr*)io->priv)->evts;
	//if not persist we direct to reponse the result, else use main thread for io.
	if(!(io->mask & CEV_PERSIST)) {
		rs = response(io);
		if(rs) return rs;
	} else {
		cevents_add_event(cevts, io->fd, CEV_WRITE, write_event_proc, io);
	}
	return 0;
}

static int _reply_str(cio *io, char *buf) {
	size_t len;
	len = strlen(buf);
	io->wcount += len;
	cstr_ncat(io->wbuf, buf, len);
	return 0;
}

int reply_str(cio *io, char *buf) {
	_reply_str(io, buf);
	return _reply(io);
}

static inline int _reply_obj(cio *io, obj *obj) {
	size_t len;
	if(obj->type == OBJ_TYPE_STR) {
		cstr s = (cstr)obj->priv;
		len = cstr_used(s);
		io->wcount += len;
		cstr_ncat(io->wbuf, s, len);
		return 0;
	}
	return -1;
}

int reply_obj(cio *io, obj *obj) {
	_reply_obj(io, obj);
	return _reply(io);
}

static inline int _reply_prefix_len(cio *io, char prefix, long long l) {
	size_t len;
	char buf[64] = {0};
	buf[0] = prefix;
	lltostr(l, buf + 1, sizeof(buf) - 1);
	len = strlen(buf + 1);
	buf[len + 1] = '\r';
	buf[len + 2] = '\n';
	cstr_ncat(io->wbuf, buf, 3 + len);
	io->wcount += 3 + len;
	return 0;
}

int reply_len(cio *io, long long l) {
	 _reply_prefix_len(io, ':', l);
	return _reply(io);
}

int reply_bulk(cio *io, obj *obj) {
	long long len = 0;
	if(obj->type == OBJ_TYPE_STR) {
		cstr s = (cstr)obj->priv;
		len = cstr_used(s);
	}
	_reply_prefix_len(io, '$', len);
	_reply_obj(io, obj);
	reply_str(io, "\r\n");
	return 0;
}

int reply_cstr(cio *io, cstr s) {
	size_t len;
	len = cstr_used(s);
	io->wcount += len;
	cstr_ncat(io->wbuf, s, len);
	return _reply(io);
}

int reply_err(cio *io, const char *err) {
	size_t len;
	char buf[1024] = {0};
	snprintf(buf, sizeof(buf), "-ERR %s\r\n", err);
	len = strlen(buf);
	io->wcount += len;
	cstr_ncat(io->wbuf, buf, len);
	return _reply(io);
}

int write_event_proc(cevents *cevts, int fd, void *priv, int mask) {
	int rs;
	cio *io = (cio*)priv;
	io->mask = mask;
	return response(io);
}

int _response(cio *io) {
	int nwrite;
	size_t total = cstr_used(io->wbuf);
	while(io->wcount > 0) {	
		nwrite = write(io->fd, io->wbuf + (total - io->wcount), io->wcount);
		if(nwrite < 0) {
			//continue;
			if(errno == EAGAIN) {
				return 1;
			}
			return -1;
		}
		io->wcount -= nwrite;
	}
	return 0;
}

int _read_process(cio *io) {
	char buf[2048];
	int rs, nread;
	nread = read(io->fd, buf, sizeof(buf));
	if(nread < 0) {
		if(errno == EAGAIN) {
			return 1;
		}
		return -1;
	}
	if(nread == 0) {
		return -1;
	}
	cstr_ncat(io->rbuf, buf, nread);
	rs = try_process_command(io);
	return rs;
}

static int try_process_multibluk_command(cio *io) {
	char *ptr;
	int ret = 0;
	long long len;
	size_t nread = cstr_used(io->rbuf), pos = 0;
	if(io->nbulk == 0) {
		if(io->rbuf[0] != '*') {
			set_protocol_error(io);
			reply_str(io, "-ERR: unknown protocol\r\n");
			return -1;
		} 
		ptr = strstr(io->rbuf, "\r\n");
		if(ptr == NULL) {
			if(nread > MAX_COMMAND_LEN_LIMIT) {
				set_protocol_error(io);
				reply_str(io, "-ERR reach the max command recv limit\r\n");
				return -1;
			}
			return 1;
		}
		if(str2ll(io->rbuf + 1, ptr - io->rbuf - 1, &len) < 0 || (len > 1024 || len <= 0)) {
			set_protocol_error(io);
			reply_str(io, "-ERR: error number of bulk \r\n");
			return -1;
		}
		pos += ptr - io->rbuf + 2;
		io->nbulk = len;
		io->argc = 0;
		if(io->argv) jfree(io->argv);
		io->argv = jmalloc(io->nbulk*sizeof(struct obj*));
	}
	while(io->nbulk) {
		if(io->bulk_len == 0) {
			ptr = strstr(io->rbuf + pos, "\r\n");
			if(ptr == NULL) {
				if(nread > MAX_COMMAND_LEN_LIMIT) {
					set_protocol_error(io);
					reply_str(io, "-ERR reach the max command recv limit\r\n");
					return -1;
				}
				//need more data
				ret = 1;
				break;
			} else {
				if(io->rbuf[pos] != '$') {
					set_protocol_error(io);
					reply_str(io, "-ERR unknow protocol\r\n");
					return -1;
				}
				if(str2ll(io->rbuf + pos + 1, ptr - (io->rbuf + pos) - 1, &len) < 0 || (len > MAX_COMMAND_LEN_LIMIT || len <= 0)) {
					set_protocol_error(io);
					reply_str(io, "-ERR: error bulk length \r\n");
					return -1;
				}
				io->bulk_len = len;
				pos += ptr - (io->rbuf + pos) + 2;
			}
		}
		if(nread - pos < io->bulk_len + 2) {
			// read more data for bulk.
			ret = 1;
			break;
		}
		io->argv[io->argc++] = obj_create(OBJ_TYPE_STR, cstr_new(io->rbuf + pos, io->bulk_len));
		pos += io->bulk_len + 2;
		io->bulk_len = 0;
		io->nbulk--;
	}
	if(ret)
		cstr_range(io->rbuf, pos + io->bulk_len, -1);
	return ret;
}

static int try_process_command(cio *io) {
	size_t i, nread = cstr_used(io->rbuf);
	cstr s;
	char *end;
	cstr *argv;
	if(nread <= 0)
		return -1;

	if(io->reqtype || io->rbuf[0] == '*') {
		io->reqtype = REQ_TYPE_MBULK;
		return try_process_multibluk_command(io);
	}

	if(cstr_used(io->rbuf) > MAX_COMMAND_LEN_LIMIT) {
		set_protocol_error(io);
		reply_str(io, "-ERR reach the max command recv limit\r\n");
		return -1;
	}
	
	end = strstr(io->rbuf, "\r\n");
	if(end == NULL) {
		return 1;
	}
	argv = cstr_split(io->rbuf, nread, " ", 1, &io->argc);
	s = argv[io->argc - 1];
	//last already is 0
	cstr_range(s, 0, -2);
	io->argv = jmalloc(io->argc*sizeof(struct obj*));
	for(i = 0; i < io->argc; i++) {
		io->argv[i] = obj_create(OBJ_TYPE_STR, argv[i]);
	}
	jfree(argv);
	return 0;
}

//just for test.
int read_event_proc(cevents *cevts, int fd, void *priv, int mask) {
	cio *io = (cio*)priv;
	int rs, persist = (mask & CEV_PERSIST);
	io->mask = mask;
	rs = _read_process(io);
	if(!persist && rs == 1) {
		install_read_event(cevts, io);
		return 1;
	}
	if(rs < 0) {
		//if not after write, will be closed.
		if(!(io->flag & IOF_CLOSE_AFTER_WRITE))
			cio_close_destroy_install(io);
		return rs;
	}
	//if not CEV_PERSIST event, we should process command.
	if(!persist) {
		io->mask = mask;
		process_commond(io);
	}
	return rs;
}


