
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ACCOUNT_H
#define _LAMB_ACCOUNT_H

#include "cache.h"
#include "queue.h"

typedef struct {
    int id;
    char user[8];
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

typedef struct {
    int id;
    lamb_queue_t send;
    lamb_queue_t recv;
} lamb_account_queue;

int lamb_account_get(lamb_cache_t *cache, char *user, lamb_account_t *account);
int lamb_account_get_all(lamb_db_t *db, lamb_account_t *accounts[], size_t size);
int lamb_account_queue_open(lamb_account_queue_t *queues[], size_t qlen, lamb_account_t *accounts[], size_t alen);

#endif
