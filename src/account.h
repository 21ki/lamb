
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ACCOUNT_H
#define _LAMB_ACCOUNT_H

#include "cache.h"

typedef struct {
    char spid[8];
    char account[8];
    char ip[16];
    int limit;
} lamb_account_t;

int lamb_account_get(lamb_cache_t *cache, char *user, lamb_account_t *account);

#endif
