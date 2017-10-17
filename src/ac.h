
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_AC_H
#define _LAMB_AC_H

#include <stdbool.h>

typedef struct {
    int id;
    char listen[16];
    int port;
    int connections;
    long long timeout;
    long long send_timeout;
    long long recv_timeout;
    char queue[64];
    char redis_host[16];
    int redis_port;
    int redis_db;
    char logfile[128];
    bool debug;
    bool daemon;
} lamb_config_t;

int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
