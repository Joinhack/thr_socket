#ifndef CNET_H
#define CNET_H

#include <sys/types.h>
#include "common.h"
#ifdef __cplusplus
extern "C" {
#endif

int cnet_tcp_accept(int fd, char *ip, int *port, char *ebuf, size_t len);

int cnet_unix_accept(int fd, char *ebuf, size_t len);

int cnet_create_sock(int domain, int type, char *ebuf, size_t len);

int cnet_tcp_server(char *ip, int port, char *ebuf, size_t len);

int cnet_unix_server(char *path, mode_t perm,char *ebuf, size_t len);
#ifdef __cplusplus
}
#endif

#endif /*end define cnet*/
