
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

int lamb_queue_open(lamb_queue_t *queue, char *name, lamb_queue_opt *opt) {
    /* Open send queue */
    queue->mqd = mq_open(name, opt->flag, 0644, opt->attr);

    if (queue->mqd < 0) {
        return -1;
    }
    
    return 0;
}

int lamb_queue_send(lamb_queue_t *queue, const char *val, size_t len, unsigned int prio) {
    return mq_send(queue->mqd, val, len, prio);
}

ssize_t lamb_queue_receive(lamb_queue_t *queue, char *buff, size_t len, unsigned int *prio) {
    return mq_receive(queue->mqd, buff, len, prio);
}

int lamb_queue_getattr(lamb_queue_t *queue, lamb_queue_attr *qattr) {
    int err;
    struct mq_attr attr;

    err  = mq_getattr(queue->mqd, &attr);
    if (err == -1) {
        return -1;
    }

    qattr->len = attr.mq_curmsgs;

    return 0;
}
