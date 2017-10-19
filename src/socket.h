
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SOCKET_H
#define _LAMB_SOCKET_H

#pragma pack(1)

typedef struct {
    int id;
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
    char addr[16];
    char token[40];
    long long timeout;
} lamb_nn_option;

#pragma pack()

int lamb_nn_connect(int *sock, lamb_nn_option *opt, const char *host, int port);

#endif
