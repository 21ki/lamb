
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SCHEDULER_H
#define _LAMB_SCHEDULER_H

#include <stdbool.h>
#include <pthread.h>
#include "db.h"
#include "list.h"

typedef struct {
    int id;
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    char ac_host[16];
    int ac_port;
    char logfile[128];
} lamb_config_t;

void lamb_event_loop(void);
void *lamb_push_loop(void *arg);
void *lamb_pull_loop(void *arg);
int lamb_server_init(int *sock, const char *addr, int port);
int lamb_child_server(int *sock, const char *listen, unsigned short *port, int protocol);
void *lamb_stat_loop(void *arg);
void lamb_route_channel(lamb_db_t *db, int id, lamb_list_t *channels);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
