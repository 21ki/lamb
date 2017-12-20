
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_GATEWAY_H
#define _LAMB_GATEWAY_H

#include "db.h"

typedef struct {
    int id;
    int type;
    char host[16];
    int port;
    char username[32];
    char password[64];
    char spid[32];
    char spcode[24];
    int encoded;
    int concurrent;
} lamb_gateway_t;

#endif
