
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SOCKET_H
#define _LAMB_SOCKET_H

#include <stdbool.h>

int lamb_sock_nonblock(int fd, bool enable);
int lamb_sock_tcpnodelay(int fd, bool enable);
int lamb_sock_send(int fd, const char *buff, size_t len, long long millisecond);
int lamb_sock_recv(int fd, char *buff, size_t len, long long millisecond);
int lamb_sock_readable(int fd, long long millisecond);
int lamb_sock_writable(int fd, long long millisecond);

#endif
