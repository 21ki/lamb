
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_QUEUE_H
#define _LAMB_QUEUE_H

#include <mqueue.h>

#define LAMB_QUEUE_SEND 1
#define LAMB_QUEUE_RECV 2

typedef struct {
    mqd_t mqd;
} lamb_queue_t;

typedef struct {
    int flag;
    struct mq_attr *attr;
} lamb_queue_opt;

int lamb_queue_open(lamb_queue_t *queue, char *name, lamb_queue_opt *opt);

#endif
