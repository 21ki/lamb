
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */


#ifndef _LAMB_QUEUE_H
#define _LAMB_QUEUE_H

typedef struct lamb_node_t {
    void *val;
    struct lamb_node_t *next;
} lamb_node_t;

typedef struct lamb_node_t {
    int id;
    long long len;
    pthread_mutex_t lock;
    struct lamb_node_t *head;
    struct lamb_node_t *tail;
    struct lamb_queue_t *next;
} lamb_queue_t;

lamb_queue_t *lamb_queue_new(int id);
lamb_node_t *lamb_node_push(lamb_queue_t *self, void *val);
lamb_node_t *lamb_node_pop(lamb_queue_t *self);

#endif
