#ifndef CIO_H
#define CIO_H

#include "cstr.h"
#include "obj.h"

#define IO_TCP 0x1
#define IO_UN 0x1<<1
#define IO_UDP 0x1<<2

#define IOF_CLOSE_AFTER_WRITE 0x1

#define REQ_TYPE_NORMAL 0x0
#define REQ_TYPE_MBULK 0x1

typedef struct {
	int fd;
	int type;
	int mask;
	cstr rbuf;
	cstr wbuf;
	int wcount;
	char ip[128];
	int nbulk;
	int bulk_len;
	int reqtype;
	int port;
	int flag;
	obj **argv;
	size_t argc;
	int tabidx;
	void *priv;
} cio;

int cio_set_noblock(int fd);

int cio_set_block(int fd);

int cio_write(int fd, char *ptr, size_t len);

int cio_read(int fd, char *ptr, size_t len);

cio *cio_create();

void cio_destroy(cio *io);

void cio_clear(cio *io);

#endif /*end define common io**/
