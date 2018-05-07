
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_SERVER_H
#define _LAMB_SERVER_H

#include "db.h"
#include "common.h"
#include "list.h"
#include "cache.h"
#include "config.h"
#include "routing.h"
#include "account.h"
#include "company.h"
#include "template.h"
#include "keyword.h"
#include "message.h"

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
    char ac[128];
    char mt[128];
    char mo[128];
    char scheduler[128];
    char deliver[128];
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
    unsigned long long usb;
    unsigned long long limt;
    unsigned long long rejt;
    unsigned long long tmp;
    unsigned long long key;
} lamb_status_t;

typedef struct {
    lamb_db_t db;
    lamb_db_t mdb;
    long long money;
    lamb_cache_t rdb;
    lamb_list_t *storage;
    lamb_list_t *billing;
    lamb_list_t *unsubscribe;
    lamb_account_t account;
    lamb_company_t company;
    lamb_list_t *templates;
    lamb_list_t *keywords;
    pthread_mutex_t lock;
} lamb_global_t;
    
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
void *lamb_stat_loop(void *data);
void *lamb_unsubscribe_loop(void *arg);
int lamb_write_report(lamb_db_t *db, lamb_report_t *message);
int lamb_write_message(lamb_db_t *db, lamb_submit_t *message);
int lamb_write_deliver(lamb_db_t *db, lamb_deliver_t *message);
void lamb_get_today(const char *pfx, char *val);
void lamb_new_table(lamb_db_t *db);
bool lamb_check_content(lamb_template_t *template, char *content, int len);
bool lamb_check_keyword(lamb_keyword_t *key, char *content);
bool lamb_check_blacklist(lamb_caches_t *cache, char *number);
bool lamb_check_unsubscribe(lamb_caches_t *cache, int id, char *number);
bool lamb_check_frequency(lamb_caches_t *cache, int id, char *number);
bool lamb_check_unsubval(char *content, int len);
bool lamb_check_arrears(lamb_cache_t *rdb, int company);
void lamb_direct_response(int sock, Report *resp, Submit *message, int cause);
int lamb_component_initialization(lamb_config_t *cfg);
int lamb_check_signal(lamb_cache_t *cache, int id);
void lamb_clear_signal(lamb_cache_t *cache, int id);
void lamb_stat_update(lamb_cache_t *cache, int id, int stat);
void lamb_sync_status(lamb_cache_t *cache, int id, lamb_status_t *stat, int store, int bill);
void lamb_exit_cleanup(void);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
