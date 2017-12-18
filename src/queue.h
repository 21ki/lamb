
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */


#ifndef _LAMB_QUEUE_H
#define _LAMB_QUEUE_H

#include <pthread.h>

typedef lamb_node_t lamb_queue_node;

typedef lamb_list_t lamb_queue_t;

lamb_queue_t *lamb_queue_new(int id);
lamb_node_t *lamb_queue_push(lamb_queue_t *self, void *val);
lamb_node_t *lamb_queue_pop(lamb_queue_t *self);

#endif
