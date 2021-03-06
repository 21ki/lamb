
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_COMPANY_H
#define _LAMB_COMPANY_H

#include "db.h"
#include "cache.h"

#define LAMB_MAX_COMPANY 1024

typedef struct {
    int id;
    long long money;
    int status;
} lamb_company_t;

typedef struct {
    int len;
    lamb_company_t *list[LAMB_MAX_COMPANY];
} lamb_companys_t;

int lamb_company_get(lamb_db_t *db, int id, lamb_company_t *company);
int lamb_company_get_all(lamb_db_t *db, lamb_companys_t *companys, int size);
int lamb_company_billing(lamb_cache_t *cache, int company, long long money);

#endif
