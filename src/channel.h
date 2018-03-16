
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_CHANNEL_H
#define _LAMB_CHANNEL_H

#include "db.h"
#include "list.h"

typedef struct {
    int id;
    int gid;
    int weight;
    int operator;
} lamb_channel_t;

int lamb_get_channels(lamb_db_t *db, int gid, lamb_list_t *channels);

#endif
