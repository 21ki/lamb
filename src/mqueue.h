
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_MQUEUE_H
#define _LAMB_MQUEUE_H

#include <mqueue.h>

#define LAMB_QUEUE_SEND 1
#define LAMB_QUEUE_RECV 2

#define LAMB_MAX_QUEUE 1024

typedef struct {
    mqd_t mqd;
} lamb_mq_t;

typedef struct {
    int len;
    lamb_mq_t *queues[LAMB_MAX_QUEUE];
} lamb_mqs_t;

typedef struct {
    int flag;
    struct mq_attr *attr;
} lamb_mq_opt;

typedef struct {
    long len;
} lamb_mq_attr;

int lamb_mq_open(lamb_mq_t *queue, char *name, lamb_mq_opt *opt);
int lamb_mq_send(lamb_mq_t *queue, const char *val, size_t len, unsigned int prio);
ssize_t lamb_mq_receive(lamb_mq_t *queue, char *buff, size_t len, unsigned int *prio);
int lamb_mq_getattr(lamb_mq_t *queue, lamb_mq_attr *qattr);
int lamb_mq_close(lamb_mq_t *queue);

#endif
