TARGET=64
MYSQL_SOURCE_PATH=
MYSQL_BIN_PATH=

CC=gcc
CPP=g++
MYSQL_INC= -I $(MYSQL_SOURCE_PATH)/sql -I $(MYSQL_SOURCE_PATH)/include -I $(MYSQL_SOURCE_PATH)/regex -I $(MYSQL_SOURCE_PATH)/ -I $(MYSQL_SOURCE_PATH)/include/mysql $(shell $(MYSQL_BIN_PATH)/mysql_config --include)

MYSQL_LIBS= $(shell $(MYSQL_BIN_PATH)/mysql_config --libs_r) -L $(MYSQL_BIN_PATH)/../lib
CFLAGS= -g -fPIC -DPIC -DUSE_TRACE   -static-libgcc 
CXXFLAGS=$(MYSQL_INC) -g -static-libgcc  -O2  -fno-rtti -fno-exceptions -fPIC -DPIC  -DWITH_WSREP -DWSREP_PROC_INFO  -DMYSQL_MAX_VARIABLE_VALUE_LEN=2048 -DWITH_INNODB_DISALLOW_WRITES -DMY_PTHREAD_FASTMUTEX=1 -DDBUG_OFF
LIBS= -lpthread 


OBJ=thr_socket.o thr_socket_svr.o jmalloc.o spinlock.o log.o cstr.o common.o clist.o cio.o network.o cevent.o cnet.o obj.o dict.o cthread.o thrs_handler.o

all: thr_socket.so

%.o: %.c
	$(CC) -c $< $(CFLAGS)

%.o: %.cpp 
	$(CPP) -c $^ $(CXXFLAGS)

clean:
	rm -rf *.o
	rm -rf thr_socket.so

thr_socket.so: $(OBJ)
	$(CPP)  -shared -o $@ $^ $(LIBS) $(MYSQL_LIBS)
