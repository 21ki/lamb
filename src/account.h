
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ACCOUNT_H
#define _LAMB_ACCOUNT_H

#include "db.h"
#include "cache.h"

#define LAMB_MAX_CLIENT 1024

#define LAMB_CHARGE_SUBMIT  1
#define LAMB_CHARGE_SUCCESS 2

typedef struct {
    int id;
    char username[8];
    char password[64];
    char spcode[24];
    int company;
    char address[16];
    int concurrent;
    int dbase;
    bool template;
    bool keyword;
} lamb_account_t;

typedef struct {
    int len;
    lamb_account_t *list[LAMB_MAX_CLIENT];
} lamb_accounts_t;

int lamb_account_get(lamb_cache_t *cache, char *username, lamb_account_t *account);
int lamb_account_fetch(lamb_db_t *db, int id, lamb_account_t *account);
int lamb_account_get_all(lamb_db_t *db, lamb_accounts_t *accounts, int size);

#endif
