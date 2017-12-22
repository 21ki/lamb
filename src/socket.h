
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SOCKET_H
#define _LAMB_SOCKET_H

#include "command.h"

#pragma pack(1)

#define LAMB_REQ     0
#define LAMB_NN_PUSH 1
#define LAMB_NN_PULL 2
#define LAMB_NN_PING 3
#define LAMB_BYE     4

#define LAMB_PUSH    1
#define LAMB_PULL    2

#define LAMB_REQUEST  0x1
#define LAMB_RESPONSE 0x2

#define LAMB_MAX_BUFF_LENGTH 8192

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

#endif
