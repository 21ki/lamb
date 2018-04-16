
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_SOCKET_H
#define _LAMB_SOCKET_H

#include "command.h"

#pragma pack(1)

#define LAMB_OK       (1 << 1)
#define LAMB_REQ      (1 << 2)
#define LAMB_BYE      (1 << 3)
#define LAMB_PUSH     (1 << 4)
#define LAMB_PULL     (1 << 5)
#define LAMB_BUSY     (1 << 7)
#define LAMB_EMPTY    (1 << 6)
#define LAMB_REJECT   (1 << 8)
#define LAMB_NOROUTE  (1 << 9)
#define LAMB_REQUEST  (1 << 10)
#define LAMB_RESPONSE (1 << 11)
#define LAMB_PING     (1 << 12)
#define LAMB_TEST     (1 << 13)

#define HEAD (signed int)sizeof(int)
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


int lamb_nn_connect(int *sock, const char *host, int protocol, int timeout);
int lamb_nn_reqrep(const char *host, int id, int timeout);
int lamb_nn_pair(const char *host, int id, int timeout);
int lamb_nn_access(const char *host, int id, int type, int timeout);
Response *lamb_nn_request(const char *host, Request *req, int timeout);
int lamb_nn_server(int *sock, const char *listen, unsigned short port, int protocol);
size_t lamb_pack_assembly(char **buf, int method, void *pk, size_t len);
void lamb_nn_close(int sock);

#endif
