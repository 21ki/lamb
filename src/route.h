
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_TEMPLATE_H
#define _LAMB_TEMPLATE_H

#include "db.h"

#define LAMB_MAX_ROUTE 1024

typedef struct {
    int id;
    char spcode[32];
    char account[32];
} lamb_route_t;

int lamb_route_get(lamb_db_t *db, int id, lamb_route_t *route);
int lamb_route_get_all(lamb_db_t *db, lamb_route_t *routes[], size_t size);

#endif
