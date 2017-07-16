
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_QUEUE_H
#define _LAMB_QUEUE_H

#include <pthread.h>
#include "list.h"

typedef struct {
    list_t *list;
    pthread_mutex_t lock;
} lamb_queue_t;

void lamb_queue_init(lamb_queue_t *queue);

#endif
