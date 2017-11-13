
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ROUTING_H
#define _LAMB_ROUTING_H

#include "db.h"
#include "queue.h"

typedef struct {
    int id;
    int account;
    char spcode[32];
} lamb_delivery_t;

int lamb_get_delivery(lamb_db_t *db, lamb_queue_t *delivery);

#endif
