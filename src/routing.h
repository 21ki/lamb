
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ROUTING_H
#define _LAMB_ROUTING_H

#include "db.h"
#include "queue.h"
#include "channel.h"

#define LAMB_MAX_ROUTING 1024

typedef struct {
    int id;
} lamb_routing_t;

int lamb_get_routing(lamb_db_t *db, int id, lamb_queue_t *routing);

#endif
