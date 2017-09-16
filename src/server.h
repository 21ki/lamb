
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SERVER_H
#define _LAMB_SERVER_H

#include "cache.h"
#include "queue.h"
#include "list.h"
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
    bool daemon;
    int work_queue;
    int work_threads;
    char logfile[128];
    char redis_host[16];
    int redis_port;
    char redis_password[64];
    int redis_db;
    char black_host[16];
    int black_port;
    char black_password[64];
    int black_db;
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
    lamb_db_t *db;
    lamb_cache_t *rdb;
    lamb_cache_t *cache;
    lamb_list_t *queue;
    lamb_list_t *storage;
    lamb_queues_t *channel;
    lamb_account_t *account;
    lamb_group_t *group;
    lamb_company_t *company;
    lamb_templates_t *template;
} lamb_work_object_t;

void lamb_event_loop(void);
void lamb_work_loop(lamb_account_t *account);
void *lamb_worker_loop(void *data);
lamb_gateway_queue_t *lamb_find_queue(int id, lamb_gateway_queues_t *queues);
void *lamb_save_message(void *data);
int lamb_each_queue(lamb_group_t *group, lamb_queue_opt *opt, lamb_queues_t *list, int size);
void *lamb_online_update(void *data);
void lamb_init_queues(lamb_account_t *account);
int lamb_init_node_cache(lamb_caches_t *cache, int len, char *nodes[], int size);
int lamb_read_config(lamb_config_t *conf, const char *file);
void lamb_reload(int signum);

#endif
