
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
#include "command.h"

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
    void *buf;
    char url[128];
    int timeo;
        
    err = lamb_nn_request(&fd, host, port, timeout);
    if (err || fd < 0) {
        return -1;
    }

    int *method;
    void buff[8192];

    method = buff;
    *method = LAMB_REQUEST;

    unsigned int len;
    Request req = LAMB_REQUEST_INIT;
    req.id = opt->id;
    req.type = opt->type;
    req.addr = opt->addr;

    len = lamb_request_get_packed_size(&req) + sizeof(int);

    lamb_request_pack(&req, buff + sizeof(int));

    /* old code */
    /* 
    memset(&req, 0, sizeof(req));
    req.id = opt->id;
    req.type = opt->type;
    memcpy(req.addr, opt->addr, 16);
    */

    rc = nn_send(fd, buff, len, 0);

    if (rc < 0) {
        nn_close(fd);
        return 1;
    }

    rc = nn_recv(fd, &buf, NN_MSG, 0);
    if (rc < 4) {
        nn_close(fd);
        return 2;
    }

    /* 
    if (rc != sizeof(lamb_rep_t)) {
        nn_close(fd);
        nn_freemsg(buf);
        return 3;
    }
 */

    nn_close(fd);

    // rep = (lamb_rep_t *)buf;

    method = (int *)buf;

    /* Check method */
    if (method != LAMB_RESPONSE) {
        nn_freemsg(buf);
        return 3;
    }

    Response *rep;
    rep = lamb_response_unpack(NULL, rc - sizeof(int), buf + sizeof(int));

    if (!rep) {
        nn_freemsg(buf);
        return 4;
    }
    
    fd = nn_socket(AF_SP, protocol);
    if (fd < 0) {
        nn_freemsg(buf);
        lamb_response_free_unpacked(rep, NULL);
        return 4;
    }

    timeo = timeout;
    nn_setsockopt(fd, NN_SOL_SOCKET, NN_SNDTIMEO, &timeo, sizeof(timeo));

    sprintf(url, "tcp://%s:%d", rep->addr, rep->port);

    /* free response packet */
    lamb_response_free_unpacked(rep, NULL);

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
