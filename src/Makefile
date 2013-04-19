MYSQL_PATH=/usr/local


CC=clang
CFLAGS= -g -std=gnu99 -DUSE_TRACE  -I $(MYSQL_PATH)/include
OPTS = -lpthread

.c.o:
	$(CC) -c $<  $(CFLAGS)

all: thr_socket.so

clean:
	rm -rf *.o
	rm -rf thr_socket.so

thr_socket.so: thr_socket.o jmalloc.o spinlock.o log.o cstr.o common.o clist.o cio.o network.o cevent.o cnet.o obj.o dict.o cthread.o
	$(CC) -o $@ $^ -shared $(OPTS)
