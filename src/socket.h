
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SOCKET_H
#define _LAMB_SOCKET_H

#include <stdbool.h>

#define LAMB_SOCK_SEND 1
#define LAMB_SOCK_RECV 2

int lamb_sock_create(void);
int lamb_sock_bind(int fd, const char *addr, unsigned short port, int backlog);
int lamb_sock_connect(int fd, const char *addr, unsigned short port, unsigned long long timeout);
int lamb_sock_nonblock(int fd, bool enable);
int lamb_sock_tcpnodelay(int fd, bool enable);
int lamb_sock_send(int fd, const char *buff, size_t len, unsigned long long timeout);
int lamb_sock_recv(int fd, char *buff, size_t len, unsigned long long timeout);
int lamb_sock_readable(int fd, unsigned long long millisecond);
int lamb_sock_writable(int fd, unsigned long long millisecond);
int lamb_sock_timeout(int fd, int type, unsigned long long millisecond);

#endif
