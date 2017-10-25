
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SERVER_H
#define _LAMB_SERVER_H

#include "utils.h"
#include "cache.h"
#include "mqueue.h"
#include "queue.h"
#include "db.h"
#include "group.h"
#include "account.h"
#include "company.h"
#include "template.h"
#include "gateway.h"

#define LAMB_CLIENT       1
#define LAMB_GATEWAY      2

#define LAMB_BLACKLIST 1
#define LAMB_WHITELIST 2

typedef struct {
    int id;
    bool debug;
    long long timeout;
    int work_threads;
    char logfile[128];
    char redis_host[16];
    int redis_port;
    char redis_password[64];
    int redis_db;
    char mt_host[16];
    int mt_port;
    char mo_host[16];
    int mo_port;
    char md_host[16];
    int md_port;
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
    char msg_host[16];
    int msg_port;
    char msg_user[64];
    char msg_password[64];
    char msg_name[64];
    char *nodes[7];
} lamb_config_t;

typedef struct {
    unsigned long long toal;
    unsigned long long sub;
    unsigned long long rep;
    unsigned long long delv;
    unsigned long long fmt;
    unsigned long long blk;
    unsigned long long tmp;
    unsigned long long key;
} lamb_status_t;

typedef struct {
    int id;
    int money;
} lamb_bill_t;

typedef struct {
    int type;
    void *val;
} lamb_store_t;

void lamb_event_loop(void);
void lamb_reload(int signum);
void *lamb_work_loop(void *data);
void *lamb_deliver_loop(void *data);
void *lamb_store_loop(void *data);
void *lamb_billing_loop(void *data);
int lamb_open_channel(lamb_group_t *group, lamb_queue_t *channels, lamb_mq_opt *opt);
void *lamb_stat_loop(void *data);
int lamb_cache_message(lamb_cache_t *cache, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message);
int lamb_cache_query(lamb_cache_t *cache, unsigned long long id, char *spid, char *spcode, int *account, int *company, int *charge);
int lamb_write_report(lamb_db_t *db, lamb_report_t *message);
int lamb_write_message(lamb_db_t *db, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message);
int lamb_write_deliver(lamb_db_t *db, lamb_account_t *account, lamb_company_t *company, lamb_deliver_t *message);
int lamb_spcode_process(char *code, char *spcode, size_t size);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
