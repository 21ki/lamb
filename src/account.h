
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ACCOUNT_H
#define _LAMB_ACCOUNT_H

#include "cache.h"

typedef struct {
    char username[8];
    int company;
    int charge_type;
    char ip_addr[16];
    int route;
    char spcode[24];
    int concurrent;
    bool extended;
    int policy;
    bool check_template;
    bool check_keyword;
} lamb_account_t;

int lamb_account_get(lamb_cache_t *cache, char *user, lamb_account_t *account);

#endif
