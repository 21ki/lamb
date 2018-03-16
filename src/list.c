
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
 */

#include <stdlib.h>
#include "list.h"

/* Allocates a new lamb_node_t. NULL on failure */
lamb_node_t *lamb_node_new(void *val) {
    lamb_node_t *self;

    if (!(self = malloc(sizeof(lamb_node_t)))) {
        return NULL;
    }

    self->prev = NULL;
    self->next = NULL;
    self->val = val;
    
    return self;
}

/*
 * Allocate a new lamb_list_iterator_t. NULL on failure.
 * Accepts a direction, which may be LIST_HEAD or LIST_TAIL.
 */

lamb_list_iterator_t *lamb_list_iterator_new(lamb_list_t *list, lamb_list_direction_t direction) {
    lamb_node_t *node = direction == LIST_HEAD ? list->head : list->tail;
    return lamb_list_iterator_new_from_node(node, direction);
}

/*
 * Allocate a new lamb_list_iterator_t with the given start
 * node. NULL on failure.
 */

lamb_list_iterator_t *lamb_list_iterator_new_from_node(lamb_node_t *node, lamb_list_direction_t direction) {
    lamb_list_iterator_t *self;
    
    if (!(self = malloc(sizeof(lamb_list_iterator_t)))) {
        return NULL;
    }

    self->next = node;
    self->direction = direction;

    return self;
}

/*
 * Return the next lamb_node_t or NULL when no more
 * nodes remain in the list.
 */

lamb_node_t *lamb_list_iterator_next(lamb_list_iterator_t *self) {
    lamb_node_t *curr = self->next;
    
    if (curr) {
        self->next = self->direction == LIST_HEAD ? curr->next : curr->prev;
    }

    return curr;
}

/* Free the list iterator */
void lamb_list_iterator_destroy(lamb_list_iterator_t *self) {
    free(self);
    self = NULL;
    return;
}

/* Allocate a new lamb_list_t. NULL on failure */
lamb_list_t *lamb_list_new(void) {
    lamb_list_t *self;

    if (!(self = malloc(sizeof(lamb_list_t)))) {
        return NULL;
    }

    self->head = NULL;
    self->tail = NULL;
    self->free = NULL;
    self->match = NULL;
    self->len = 0;
    pthread_mutex_init(&self->lock, NULL);

    return self;
}

/*
 * Free the list.
 */

void lamb_list_destroy(lamb_list_t *self) {
    unsigned int len = self->len;
    lamb_node_t *next;
    lamb_node_t *curr = self->head;

    while (len--) {
        next = curr->next;
        if (self->free) {
            self->free(curr->val);
        }
        free(curr);
        curr = next;
    }

    pthread_mutex_destroy(&self->lock);
    free(self);

    return;
}

/*
 * Append the given node to the list
 * and return the node, NULL on failure.
 */

lamb_node_t *lamb_list_rpush(lamb_list_t *self, lamb_node_t *node) {
    if (!node) {
        return NULL;
    }

    pthread_mutex_lock(&self->lock);
    if (self->len) {
        node->prev = self->tail;
        node->next = NULL;
        self->tail->next = node;
        self->tail = node;
    } else {
        self->head = self->tail = node;
        node->prev = node->next = NULL;
    }

    ++self->len;
    pthread_mutex_unlock(&self->lock);

    return node;
}

/*
 * Return / detach the last node in the list, or NULL.
 */

lamb_node_t *lamb_list_rpop(lamb_list_t *self) {
    if (!self->len) {
        return NULL;
    }

    pthread_mutex_lock(&self->lock);
    lamb_node_t *node = self->tail;

    if (--self->len) {
        (self->tail = node->prev)->next = NULL;
    } else {
        self->tail = self->head = NULL;
    }

    node->next = node->prev = NULL;
    pthread_mutex_unlock(&self->lock);
    
    return node;
}

/*
 * Return / detach the first node in the list, or NULL.
 */

lamb_node_t *lamb_list_lpop(lamb_list_t *self) {
    pthread_mutex_lock(&self->lock);
    if (!self->len) {
        pthread_mutex_unlock(&self->lock);
        return NULL;
    }

    lamb_node_t *node = self->head;

    if (--self->len) {
        (self->head = node->next)->prev = NULL;
    } else {
        self->head = self->tail = NULL;
    }

    node->next = node->prev = NULL;
    pthread_mutex_unlock(&self->lock);

    return node;
}

/*
 * Prepend the given node to the list
 * and return the node, NULL on failure.
 */

lamb_node_t *lamb_list_lpush(lamb_list_t *self, lamb_node_t *node) {
    if (!node) {
        return NULL;
    }

    pthread_mutex_lock(&self->lock);
    if (self->len) {
        node->next = self->head;
        node->prev = NULL;
        self->head->prev = node;
        self->head = node;
    } else {
        self->head = self->tail = node;
        node->prev = node->next = NULL;
    }

    ++self->len;
    pthread_mutex_unlock(&self->lock);

    return node;
}

/*
 * Return the node associated to val or NULL.
 */

lamb_node_t *lamb_list_find(lamb_list_t *self, void *val) {
    lamb_list_iterator_t *it = lamb_list_iterator_new(self, LIST_HEAD);
    lamb_node_t *node;

    while ((node = lamb_list_iterator_next(it))) {
        if (self->match) {
            if (self->match(val, node->val)) {
                lamb_list_iterator_destroy(it);
                return node;
            }
        } else {
            if (val == node->val) {
                lamb_list_iterator_destroy(it);
                return node;
            }
        }
    }

    lamb_list_iterator_destroy(it);

    return NULL;
}

/*
 * Return the node at the given index or NULL.
 */

lamb_node_t *lamb_list_at(lamb_list_t *self, int index) {
    lamb_list_direction_t direction = LIST_HEAD;

    if (index < 0) {
        direction = LIST_TAIL;
        index = ~index;
    }

    if ((unsigned)index < self->len) {
        lamb_list_iterator_t *it = lamb_list_iterator_new(self, direction);
        lamb_node_t *node = lamb_list_iterator_next(it);
        while (index--) {
            node = lamb_list_iterator_next(it);
        }
        lamb_list_iterator_destroy(it);
        return node;
    }

    return NULL;
}

/*
 * Remove the given node from the list, freeing it and it's value.
 */

void lamb_list_remove(lamb_list_t *self, lamb_node_t *node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        self->head = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        self->tail = node->prev;
    }
    
    if (self->free) {
        self->free(node->val);
    }

    free(node);
    --self->len;
    
    return;
}

