
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */


#include <stdlib.h>
#include "pool.h"

lamb_queue_t *lamb_pool_add(lamb_pool_t *self, lamb_queue_t *queue) {
    if (!queue) {
        return NULL;
    }

    if (self->len) {
        self->tail->next = queue;
        self->tail = queue;
    } else {
        self->head = self->tail = queue;
    }
    ++self->len;

    return queue;
}

void lamb_pool_del(lamb_pool_t *self, int id) {
    lamb_queue_t *prev;
    lamb_queue_t *curr;

    prev = NULL;
    curr = self->head;

    while (curr != NULL) {
        if (curr->id == id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                self->head = curr->next;
            }

            --self->len;
            free(curr);
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    return;
}

lamb_queue_t *lamb_pool_find(lamb_pool_t *self, int id) {
    lamb_queue_t *queue;

    queue = self->head;
    
    while (queue != NULL) {
        if (queue->id == id) {
            break;
        }
        queue = queue->next;
    }

    return queue;
}

lamb_pool_t *lamb_pool_new(void) {
    lamb_pool_t *self;
    
    self = (lamb_pool_t *)malloc(sizeof(lamb_pool_t));
    if (!self) {
        return NULL;
    }

    self->len = 0;
    self->head = NULL;
    self->tail = NULL;

    return self;
}
