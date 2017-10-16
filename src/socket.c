
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "socket.h"

int lamb_sock_create(void) {
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    return fd;
}

int lamb_sock_bind(int fd, const char *addr, unsigned short port, int backlog) {
    struct sockaddr_in servaddr;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &servaddr.sin_addr) < 1) {
        return 1;
    }

    /* socket bind */
    if (bind(fd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) == -1) {
        return 2;
    }

    /* socket listen */
    if (listen(fd, backlog) == -1) {
        return 3;
    }

    return 0;
}

int lamb_sock_connect(int fd, const char *addr, unsigned short port, unsigned long long timeout) {
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &servaddr.sin_addr) < 1) {
        return 1;
    }

    /* Set connection timeout */
    lamb_sock_timeout(fd, LAMB_SOCK_SEND, timeout);

    if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        return 2;
    }

    return 0;
}

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

int lamb_sock_send(int fd, const char *buff, size_t len, unsigned long long timeout) {
    int ret;
    int offset = 0;

    /* Start sending data */
    while (offset < len) {
        if (lamb_sock_writable(fd, timeout) > 0) {
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

int lamb_sock_recv(int fd, char *buff, size_t len, unsigned long long timeout) {
    int ret;
    int offset = 0;

    /* Begin to receive data */
    while (offset < len) {
        if (lamb_sock_readable(fd, timeout) > 0) {
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

int lamb_sock_readable(int fd, unsigned long long millisecond) {
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

int lamb_sock_writable(int fd, unsigned long long millisecond) {
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

int lamb_sock_timeout(int fd, int type, unsigned long long millisecond) {
    struct timeval timeout;

    timeout.tv_sec = millisecond / 1000;
    timeout.tv_usec = (millisecond % 1000) * 1000;
    type = (type == LAMB_SOCK_SEND) ? SO_SNDTIMEO : SO_RCVTIMEO;

    if (setsockopt(fd, SOL_SOCKET, type, (void *)&timeout, sizeof(timeout)) == -1) {
        return -1;
    }

    return 0;
}

