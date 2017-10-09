
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <fcntl.h>
#include <sys/socket.h>
#include "socket.h"

int lamb_sock_nonblock(int fd, bool enable) {
    int ret, flag;

    flag = fcntl(fd, F_GETFL, 0);

    if (flag == -1) {
        return 1;
    }

    if (enable) {
        flag |= O_NONBLOCK;
    } else {
        flag &= ~O_NONBLOCK;
    }

    ret = fcntl(fd, F_SETFL, flag);

    if (ret == -1) {
        return 2;
    }

    return 0;
}

int lamb_sock_tcpnodelay(int fd, bool enable) {
    int ret, flag;

    flag = enable ? 1 : 0;
    ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
    if (ret == -1) {
        return -1;
    }

    return 0;
}
