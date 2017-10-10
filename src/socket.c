
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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

int lamb_sock_send(int fd, const char *buff, size_t len, long long millisecond) {
    int ret;
    int offset = 0;

    /* Start sending data */
    while (offset < len) {
        if (lamb_sock_writable(fd, millisecond) > 0) {
            ret = write(fd, buff + offset, len - offset);
            if (ret > 0) {
                offset += ret;
                continue;
            } else {
                if (ret == 0) {
                    offset = -1;
                    break;
                }

                if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                }
                break;
            }
        } else {
            offset = -1;
            break;
        }
    }

    return offset;
}

int lamb_sock_recv(int fd, char *buff, size_t len, long long millisecond) {
    int ret;
    int offset = 0;

    /* Begin to receive data */
    while (offset < len) {
        if (lamb_sock_readable(fd, millisecond) > 0) {
            ret = read(fd, buff + offset, len - offset);
            if (ret > 0) {
                offset += ret;
                continue;
            } else {
                if (ret == 0) {
                    return -1;
                }

                if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                }
                
                break;
            }
        } else {
            offset = -1;
            break;
        }
    }

    return offset;
}

int lamb_sock_readable(int fd, long long millisecond) {
    int ret;
    fd_set rset;
    struct timeval timeout;

    timeout.tv_sec = millisecond / 1000;
    timeout.tv_usec = (millisecond % 1000) * 1000;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    ret = select(fd + 1, &rset, NULL, NULL, &timeout);

    return ret;
}

int lamb_sock_writable(int fd, long long millisecond) {
    int ret;
    fd_set wset;
    struct timeval timeout;

    timeout.tv_sec = millisecond / 1000;
    timeout.tv_usec = (millisecond % 1000) * 1000;

    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    ret = select(fd + 1, NULL, &wset, NULL, &timeout);

    return ret;
}

