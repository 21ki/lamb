
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_POOL_H
#define _LAMB_POOL_H

#include "queue.h"

typedef struct {
    int len;
    lamb_queue_t *head;
    lamb_queue_t *tail;
} lamb_pool_t;


typedef struct lamb_thread_t {
    pthread_t id;
    time_t clock;
    struct lamb_thread_t *next;
} lamb_thread_t;

typedef struct {
    int len;
    lamb_thread_t *head;
    lamb_thread_t *tail;
} lamb_threads_t;

lamb_pool_t *lamb_pool_new(void);
lamb_queue_t *lamb_pool_add(lamb_pool_t *self, lamb_queue_t *queue);
void lamb_pool_del(lamb_pool_t *self, int id);
lamb_queue_t *lamb_pool_find(lamb_pool_t *self, int id);

#endif
