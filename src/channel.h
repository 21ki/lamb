
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_CHANNEL_H
#define _LAMB_CHANNEL_H

#include "db.h"

typedef struct {
    int id;
    int gid;
    int weight;
} lamb_channel_t;

int lamb_channel_get(lamb_db_t *db, int gid, lamb_channel_t *channels[], size_t size);

#endif
