
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
 * Update: 2017-07-14
 */

#ifndef _LAMB_LIST_H
#define _LAMB_LIST_H

#include <stdlib.h>
#include <pthread.h>

/* list iterator direction */
typedef enum {
    LIST_HEAD,
    LIST_TAIL,
} lamb_list_direction_t;

/*  list node struct */
typedef struct lamb_node {
    struct lamb_node *prev;
    struct lamb_node *next;
    void *val;
} lamb_node_t;

/* list struct */
typedef struct {
    lamb_node_t *head;
    lamb_node_t *tail;
    unsigned int len;
    void (*free)(void *val);
    int (*match)(void *a, void *b);
    pthread_mutex_t lock;
} lamb_list_t;

/* list iterator struct */
typedef struct {
    lamb_node_t *next;
    lamb_list_direction_t direction;
} lamb_list_iterator_t;

/* node prototypes */
lamb_node_t *lamb_node_new(void *val);

/* lamb_list_t prototypes */
lamb_list_t *lamb_list_new(void);
lamb_node_t *lamb_list_rpush(lamb_list_t *self, lamb_node_t *node);
lamb_node_t *lamb_list_lpush(lamb_list_t *self, lamb_node_t *node);
lamb_node_t *lamb_list_find(lamb_list_t *self, void *val);
lamb_node_t *lamb_list_at(lamb_list_t *self, int index);
lamb_node_t *lamb_list_rpop(lamb_list_t *self);
lamb_node_t *lamb_list_lpop(lamb_list_t *self);
void lamb_list_remove(lamb_list_t *self, lamb_node_t *node);
void lamb_list_destroy(lamb_list_t *self);

/* lamb_list_t iterator prototypes */
lamb_list_iterator_t *lamb_list_iterator_new(lamb_list_t *list, lamb_list_direction_t direction);
lamb_list_iterator_t *lamb_list_iterator_new_from_node(lamb_node_t *node, lamb_list_direction_t direction);
lamb_node_t *lamb_list_iterator_next(lamb_list_iterator_t *self);
void lamb_list_iterator_destroy(lamb_list_iterator_t *self);

#endif
