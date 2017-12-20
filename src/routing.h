
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
} lamb_routing_t;

int lamb_get_routing(lamb_db_t *db, lamb_list_t *routings);

#endif
