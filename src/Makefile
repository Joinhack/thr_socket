TARGET=64
MYSQL_SOURCE_PATH=
MYSQL_BIN_PATH=

CC=gcc
CPP=g++
MYSQL_INC= -I $(MYSQL_SOURCE_PATH)/sql -I $(MYSQL_SOURCE_PATH)/include -I $(MYSQL_SOURCE_PATH)/regex -I $(MYSQL_SOURCE_PATH)/ 

MYSQL_LIBS=-L$(MYSQL_BIN_PATH)/lib -lmysqlclient_r
CFLAGS= -g -fPIC -DUSE_TRACE 
CXXFLAGS=-I  $(MYSQL_BIN_PATH)/include $(MYSQL_INC) -g -fPIC -DUSE_TRACE 
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
