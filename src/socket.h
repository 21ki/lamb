
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SOCKET_H
#define _LAMB_SOCKET_H

#include "command.h"

#pragma pack(1)


#define LAMB_REQ      1 << 1
#define LAMB_BYE      1 << 2
#define LAMB_EMPTY    1 << 3

#define LAMB_PUSH     1 << 1
#define LAMB_PULL     1 << 2

#define LAMB_REQUEST  1 << 1
#define LAMB_RESPONSE 1 << 2

#define HEAD sizeof(int)
#define CHECK_COMMAND(val) ntohl(*((int *)(val)))

typedef struct {
    int id;
    int type;
    char addr[16];
    char token[128];
} lamb_req_t;

typedef struct {
    int id;
    char addr[16];
    int port;
} lamb_rep_t;

typedef struct {
    int id;
    int type;
    char addr[16];
    char token[40];
} lamb_nn_option;

#pragma pack()


int lamb_nn_connect(int *sock, const char *host, int port, int protocol, int timeout);
int lamb_nn_reqrep(const char *host, int port, int id, int timeout);
int lamb_nn_pair(const char *host, int port, int id, int timeout);
Response *lamb_nn_request(const char *host, int port, Request *req, int timeout);
int lamb_nn_server(int *sock, const char *listen, unsigned short port, int protocol);
size_t lamb_pack_assembly(char **buf, int method, void *pk, size_t len);

#endif
