
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_MO_H
#define _LAMB_MO_H

#include <stdbool.h>
#include <pthread.h>
#include "queue.h"

typedef struct {
    int id;
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    char redis_host[16];
    int redis_port;
    int redis_db;
    char logfile[128];
} lamb_config_t;

void lamb_event_loop(void);
void *lamb_work_loop(void *arg);
int lamb_server_init(int *sock, const char *addr, int port);
int lamb_child_server(int *sock, const char *listen, unsigned short *port);
void *lamb_stat_loop(void *arg);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
