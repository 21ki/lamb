
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdio.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include "socket.h"

int lamb_nn_request(int *sock, const char *host, int port, int timeout) {
    int fd;
    char url[128];
    int timeo;
    
    fd = nn_socket(AF_SP, NN_REQ);
    if (fd < 0) {
        return -1;
    }

    timeo = timeout;
    nn_setsockopt(fd, NN_SOL_SOCKET, NN_SNDTIMEO, &timeo, sizeof(timeo));
    
    sprintf(url, "tcp://%s:%d", host, port);
    if (nn_connect(fd, url) < 0) {
        nn_close(fd);
        return -1;
    }

    *sock = fd;

    return 0;
}

int lamb_nn_connect(int *sock, lamb_nn_option *opt, const char *host, int port, int protocol, int timeout) {
    int err;
    int fd, rc;
    char *buf;
    char url[128];
    lamb_req_t req;
    lamb_rep_t *rep;
    int timeo;
    
    err = lamb_nn_request(&fd, host, port, timeout);
    if (err || fd < 0) {
        return -1;
    }

    memset(&req, 0, sizeof(req));
    req.id = opt->id;
    req.type = opt->type;
    memcpy(req.addr, opt->addr, 16);

    rc = nn_send(fd, (char *)&req, sizeof(req), 0);
    if (rc < 0) {
        nn_close(fd);
        return 1;
    }

    rc = nn_recv(fd, &buf, NN_MSG, 0);
    if (rc < 0) {
        nn_close(fd);
        return 2;
    }
    
    if (rc != sizeof(lamb_rep_t)) {
        nn_close(fd);
        nn_freemsg(buf);
        return 3;
    }

    nn_close(fd);
    rep = (lamb_rep_t *)buf;

    fd = nn_socket(AF_SP, protocol);
    if (fd < 0) {
        nn_freemsg(buf);
        return 4;
    }

    timeo = timeout;
    nn_setsockopt(fd, NN_SOL_SOCKET, NN_SNDTIMEO, &timeo, sizeof(timeo));

    sprintf(url, "tcp://%s:%d", rep->addr, rep->port);

    if (nn_connect(fd, url) < 0) {
        nn_freemsg(buf);
        nn_close(fd);
        return 5;
    }

    *sock = fd;
    nn_freemsg(buf);

    return 0;
}

int lamb_nn_server(int *sock, const char *listen, unsigned short port, int protocol) {
    int fd;
    char addr[128];
    
    fd = nn_socket(AF_SP, protocol);
    if (fd < 0) {
        return -1;
    }

    sprintf(addr, "tcp://%s:%u", listen, port);

    if (nn_bind(fd, addr) < 0) {
        nn_close(fd);
        return -1;
    }

    *sock = fd;

    return 0;
}
