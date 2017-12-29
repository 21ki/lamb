/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "queue.h"

lamb_queue_t *lamb_queue_new(int id) {
    lamb_queue_t *self;

    self = (lamb_queue_t *)malloc(sizeof(lamb_queue_t));

    if (self) {
        self->id = id;
        self->list = lamb_list_new();
        if (self->list) {
            pthread_mutex_init(&self->lock, NULL);
            return self;
        }
        free(self);
    }

    return NULL;
}

lamb_node_t *lamb_queue_push(lamb_queue_t *queue, void *val) {
    if (queue) {
        if (queue->list) {
            return lamb_list_rpush(queue->list, lamb_node_new(val));
        }
    }

    return NULL;
}

lamb_node_t *lamb_queue_pop(lamb_queue_t *queue) {
    if (queue) {
        if (queue->list) {
            return lamb_list_lpop(queue->list);
        }
    }

    return NULL;
}

int lamb_queue_compare(void *id, void *queue) {
    if (queue) {
        if (((lamb_queue_t *)queue)->id == (intptr_t)id) {
            return 1;
        }
    }

    return 0;
}

void lamb_queue_destroy(lamb_queue_t *queue) {
    if (queue) {
        queue->id = 0;
        lamb_list_destroy(queue->list);
        pthread_mutex_destroy(&queue->lock);
    }

    return;
}
