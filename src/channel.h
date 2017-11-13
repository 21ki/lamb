
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_CHANNEL_H
#define _LAMB_CHANNEL_H

#include "db.h"

#define LAMB_MAX_CHANNEL 1024

typedef struct {
    int id;
    int rid;
    int weight;
    int operator;
} lamb_channel_t;

typedef struct {
    int len;
    lamb_channel_t *list[LAMB_MAX_CHANNEL];
} lamb_channels_t;

int lamb_get_channels(lamb_db_t *db, int gid, lamb_channels_t *channels, int size);

#endif
