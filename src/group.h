
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_GROUP_H
#define _LAMB_GROUP_H

#include "db.h"
#include "queue.h"
#include "channel.h"

#define LAMB_MAX_GROUP 1024

typedef struct {
    int id;
    int len;
    lamb_channel_t *channels[LAMB_MAX_CHANNEL];
} lamb_group_t;

typedef struct {
    int len;
    lamb_group_t *list[LAMB_MAX_GROUP];
} lamb_groups_t;

int lamb_group_get(lamb_db_t *db, int id, lamb_queue_t *channels);

#endif
