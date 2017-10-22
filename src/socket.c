
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

int lamb_nn_connect(int *sock, lamb_nn_option *opt, const char *host, int port) {
    int fd, rc;
    char *buf;
    char url[128];
    lamb_req_t req;
    lamb_rep_t *rep;
    long long timeout;
    
    fd = nn_socket(AF_SP, NN_REQ);
    if (fd < 0) {
        return -1;
    }

    timeout = opt->timeout;
    nn_setsockopt(fd, NN_SOL_SOCKET, NN_SNDTIMEO, &timeout, sizeof(timeout));
    
    sprintf(url, "tcp://%s:%d", host, port);
    if (nn_connect(fd, url) < 0) {
        nn_close(fd);
        return 1;
    }

    memset(&req, 0, sizeof(req));
    req.id = opt->id;
    req.type = opt->type;
    memcpy(req.addr, opt->addr, 16);

    rc = nn_send(fd, (char *)&req, sizeof(req), 0);
    if (rc < 0) {
        nn_close(fd);
        return 2;
    }

    buf = NULL;
    rc = nn_recv(fd, &buf, NN_MSG, 0);
    if (rc < 0) {
        nn_close(fd);
        return 3;
    }
    
    if (rc != sizeof(lamb_rep_t)) {
        nn_close(fd);
        nn_freemsg(buf);
        return 4;
    }

    nn_close(fd);
    rep = (lamb_rep_t *)buf;

    fd = nn_socket(AF_SP, opt->type);
    if (fd < 0) {
        return 5;
    }

    nn_setsockopt(fd, NN_SOL_SOCKET, NN_SNDTIMEO, &timeout, sizeof(timeout));

    sprintf(url, "tcp://%s:%d", rep->addr, rep->port);
    if (nn_connect(fd, url) < 0) {
        nn_close(fd);
        return 6;
    }

    *sock = fd;
    nn_freemsg(buf);

    return 0;
}
