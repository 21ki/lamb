
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_MT_H
#define _LAMB_MT_H

#include <stdbool.h>
#include <pthread.h>
#include "queue.h"

typedef struct {
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    int worker_thread;
    char redis_host[16];
    int redis_port;
    int redis_db;
    char logfile[128];
} lamb_config_t;

typedef struct {
    int len;
    lamb_queue_t *head;
    lamb_queue_t *tail;
} lamb_pool_t;

void lamb_event_loop(void);
void *lamb_work_loop(void *arg);
int lamb_server_init(int *sock, const char *addr, int port);
int lamb_read_config(lamb_config_t *conf, const char *file);
lamb_pool_t *lamb_pool_new(void);
lamb_queue_t *lamb_queue_add(lamb_pool_t *self, lamb_queue_t *queue);
void lamb_queue_del(lamb_pool_t *self, int id);
lamb_queue_t *lamb_find_queue(lamb_pool_t *self, int id);
void *lamb_stat_loop(void *arg);

#endif
