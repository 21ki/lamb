
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

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
