#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "jmalloc.h"
#include "cio.h"

int cio_set_noblock(int fd) {
	int val;
	val = 1;
	return ioctl(fd, FIONBIO, &val);
}

int cio_set_block(int fd) {
	int val;
	val = 0;
	return ioctl(fd, FIONBIO, &val);
}

int cio_write(int fd, char *ptr, size_t len) {
	int count = 0;
	int wn = 0;
	while(count < len ) {
		wn = write(fd, ptr, len - count);
		if(wn == 0) return count;
		if(wn == -1) return -1;
		ptr += wn;
		count += wn;
	}
	return count;
}

int cio_read(int fd, char *ptr, size_t len) {
	int count = 0;
	int rn = 0;
	while(count < len ) {
		rn = read(fd, ptr, len - count);
		if(rn == 0) return count;
		if(rn == -1) return -1;
		ptr += rn;
		count += rn;
	}
	return count;
}

cio *cio_create() {
	cio *io = jmalloc(sizeof(cio));
	memset(io, 0, sizeof(cio));
	io->rbuf = cstr_create(1024);
	io->wbuf = cstr_create(1024);
	io->wcount = 0;
	io->priv = NULL;
	io->flag = 0;
	io->argc = 0;
	io->tabidx = 0;
	io->bulk_len = 0;
	io->nbulk = 0;
	io->reqtype = REQ_TYPE_NORMAL;
	io->argv = NULL;
	return io;
}

void cio_destroy(cio *io) {
	size_t i;
	cstr_destroy(io->rbuf);
	cstr_destroy(io->wbuf);
	for(i = 0; i < io->argc; i++) {
		obj_decr(io->argv[i]);
	}
	if(io->argv != NULL) jfree(io->argv);
	jfree(io);
}

//don't clear tabidx
void cio_clear(cio *io) {
	size_t i;
	cstr_clear(io->rbuf);
	cstr_clear(io->wbuf);
	for(i = 0; i < io->argc; i++) {
		obj_decr(io->argv[i]);
	}
	if(io->argv != NULL) jfree(io->argv);
	io->mask = 0;
	io->reqtype = REQ_TYPE_NORMAL;
	io->argc = 0;
	io->nbulk = 0;
	io->bulk_len = 0;
	io->argv = NULL;
	io->flag = 0;
	io->wcount = 0;
}
