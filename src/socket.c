
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include "socket.h"

int lamb_nn_connect(int *sock, const char *host, int port, int protocol, int timeout) {
    int fd;
    int timeo;
    char url[128];
    
    fd = nn_socket(AF_SP, protocol);

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

int lamb_nn_reqrep(const char *host, int port, int id, int timeout) {
    int fd;
    int err;
    Request req;
    Response *resp;

    req.id = id;
    req.type = LAMB_PULL;
    req.addr = "0.0.0.0";

    resp = lamb_nn_request(host, port, &req, timeout);

    if (!resp) {
        return -1;
    }

    err = lamb_nn_connect(&fd, resp->addr, resp->port, NN_REQ, timeout);

    lamb_response_free_unpacked(resp, NULL);

    if (err) {
        return -1;
    }

    return fd;
}

int lamb_nn_pair(const char *host, int port, int id, int timeout) {
    int fd;
    int err;
    Request req;
    Response *resp;

    req.id = id;
    req.type = LAMB_PUSH;
    req.addr = "0.0.0.0";

    resp = lamb_nn_request(host, port, &req, timeout);

    if (!resp) {
        return -1;
    }

    err = lamb_nn_connect(&fd, resp->addr, resp->port, NN_PAIR, timeout);

    lamb_response_free_unpacked(resp, NULL);

    if (err) {
        return -1;
    }

    return fd;
}

Response *lamb_nn_request(const char *host, int port, Request *req, int timeout) {
    int rc;
    int err;
    int sock;
    char *buf;
    Response *resp;
    unsigned int len;
    unsigned int *method;

    buf = (char *)malloc(LAMB_MAX_BUFF_LENGTH);

    method = (unsigned int *)buf;
    *method = htonl(LAMB_REQUEST);
    
    len = lamb_request_get_packed_size(req) + sizeof(unsigned int);

    if (len > (LAMB_MAX_BUFF_LENGTH - sizeof(unsigned int))) {
        free(buf);
        return NULL;
    }

    len += sizeof(unsigned int);
    lamb_request_pack(req, (uint8_t *)(buf + sizeof(unsigned int)));

    err = lamb_nn_connect(&sock, host, port, NN_REQ, timeout);
    if (err) {
        free(buf);
        return NULL;
    }

    rc = nn_send(sock, buf, len, 0);

    if (rc != len) {
        free(buf);
        nn_close(sock);
        return NULL;
    }

    free(buf);
    buf = NULL;

    rc = nn_recv(sock, &buf, NN_MSG, 0);

    if (rc < 4) {
        nn_close(sock);
        return NULL;
    }

    nn_close(sock);
    
    method = (unsigned int *)buf;

    if (ntohl(*method) != LAMB_RESPONSE) {
        free(buf);
        return NULL;
    }
    
    resp = lamb_response_unpack(NULL, rc - sizeof(unsigned int), (uint8_t *)buf);

    nn_freemsg(buf);

    return resp;
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
