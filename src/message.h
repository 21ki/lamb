
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
#include "account.h"
#include "company.h"

int lamb_write_message(lamb_db_t *db, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message);
int lamb_cache_message(lamb_cache_t *cache, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message);

#endif
