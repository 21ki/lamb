
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ROUTING_H
#define _LAMB_ROUTING_H

#include "db.h"
#include "list.h"

typedef struct {
    int id;
    char rexp[128];
    int target;
} lamb_delivery_t;

int lamb_get_delivery(lamb_db_t *db, lamb_list_t *deliverys);

#endif
