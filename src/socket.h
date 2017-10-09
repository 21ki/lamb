
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SOCKET_H
#define _LAMB_SOCKET_H

int lamb_sock_nonblock(int fd, bool enable);
int lamb_sock_tcpnodelay(int fd, bool enable);

#endif
