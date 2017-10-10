
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


typedef struct lamb_node_t {
    void *val;
    struct lamb_node_t *next;
} lamb_node_t;

typedef struct lamb_list {
    int len;
    pthread_mutex_t lock;
    struct lamb_node_t *head;
    struct lamb_node_t *tail;
} lamb_list_t;

void lamb_event_loop(void);
void *lamb_work_loop(void *arg);
int lamb_server_init(int *sock, const char *addr, int port);
int lamb_read_config(lamb_config_t *conf, const char *file);
lamb_list_t *lamb_list_new(void);
lamb_node_t *lamb_list_push(lamb_list_t *self, void *val);
lamb_node_t *lamb_list_pop(lamb_list_t *self);

#endif
