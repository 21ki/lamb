
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ACCOUNT_H
#define _LAMB_ACCOUNT_H

#include <sys/epoll.h>
#include "db.h"
#include "cache.h"
#include "queue.h"

#define LAMB_MAX_CLIENT 1024

#define LAMB_CHARGE_SUBMIT  1
#define LAMB_CHARGE_SUCCESS 2

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
    int len;
    lamb_account_t *list[LAMB_MAX_CLIENT];
} lamb_accounts_t;

typedef struct {
    int id;
    lamb_queue_t queue;
} lamb_account_queue_t;

typedef struct {
    int len;
    lamb_account_queue_t *list[LAMB_MAX_CLIENT];
} lamb_account_queues_t;

int lamb_account_get(lamb_db_t *db, char *username, lamb_account_t *account);
int lamb_account_fetch(lamb_db_t *db, int id, lamb_account_t *account);
int lamb_account_get_all(lamb_db_t *db, lamb_accounts_t *accounts, int size);
int lamb_account_queue_open(lamb_account_queues_t *queues, int qlen, lamb_accounts_t *accounts, int alen, lamb_queue_opt *opt, int type);
int lamb_account_epoll_add(int epfd, struct epoll_event *event, lamb_account_queues_t *queues, int len, int type);
int lamb_account_spcode_process(char *code, char *spcode, size_t size);

#endif
