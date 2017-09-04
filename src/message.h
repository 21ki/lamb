
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_MESSAGE_H
#define _LAMB_MESSAGE_H

#include "db.h"
#include "cache.h"
#include "utils.h"

int lamb_write_message(lamb_db_t *db, int account, int company, lamb_submit_t *message);
int lamb_cache_message(lamb_cache_t *cache, int account, int company, lamb_submit_t *message);
int lamb_update_message(lamb_db_t *db, lamb_update_t *message);
int lamb_cache_update_message(lamb_cache_t *cache, lamb_update_t *message);

#endif
