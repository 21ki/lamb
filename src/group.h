
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_GROUP_H
#define _LAMB_GROUP_H

#include "db.h"
#include "channel.h"

typedef struct {
    int id;
    int len;
    lamb_channel_t *channels[1024];
} lamb_group_t;

int lamb_group_get(lamb_db_t *db, int id, lamb_group_t *group);
int lamb_group_get_all(lamb_db_t *db, lamb_group_t *groups[], size_t size);

#endif
