
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_MT_H
#define _LAMB_MT_H

#include <stdbool.h>
#include <pthread.h>

typedef struct {
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    int worker_thread;
    char logfile[128];
} lamb_config_t;

typedef struct lamb_msg_t {
    void *val;
    struct lamb_msg_t *next;
} lamb_msg_t;

typedef struct lamb_node_t {
    int id;
    int len;
    pthread_mutex_t lock;
    struct lamb_msg_t *head;
    struct lamb_msg_t *tail;
    struct lamb_node_t *next;
} lamb_node_t;

typedef struct {
    int len;
    lamb_node_t *head;
    lamb_node_t *tail;
} lamb_pool_t;

void lamb_event_loop(void);
void *lamb_work_loop(void *arg);
int lamb_server_init(int *sock, const char *addr, int port);
int lamb_read_config(lamb_config_t *conf, const char *file);
lamb_node_t *lamb_node_new(int id);
lamb_msg_t *lamb_msg_push(lamb_node_t *self, void *val);
lamb_msg_t *lamb_msg_pop(lamb_node_t *self);
lamb_pool_t *lamb_pool_new(void);
lamb_node_t *lamb_pool_add(lamb_pool_t *self, lamb_node_t *node);
lamb_node_t *lamb_pool_del(lamb_pool_t *self, int id);
lamb_node_t *lamb_pool_find(lamb_pool_t *self, int id);
void *lamb_stat_loop(void *arg);

#endif
