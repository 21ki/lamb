/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_QUEUE_H
#define _LAMB_QUEUE_H

#include "list.h"

typedef struct lamb_queue_t {
    int id;
    lamb_list_t *list;
} lamb_queue_t;

lamb_queue_t *lamb_queue_new(int id);
lamb_node_t * lamb_queue_push(lamb_queue_t *queue, void *val);
lamb_node_t *lamb_queue_pop(lamb_queue_t *queue);
int lamb_queue_compare(void *queue, void *id);
void lamb_queue_destroy(lamb_queue_t *queue);

#endif
