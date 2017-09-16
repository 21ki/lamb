
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SERVER_H
#define _LAMB_SERVER_H

#include "db.h"
#include "cache.h"
#include "utils.h"
#include "route.h"

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
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
    char *nodes[7];
} lamb_config_t;

void lamb_event_loop(void);
void *lamb_deliver_worker(void *data);
int lamb_update_msgid(lamb_cache_t *cache, unsigned long long id, unsigned long long msgId);
int lamb_report_update(lamb_db_t *db, lamb_report_t *report);
int lamb_save_deliver(lamb_db_t *db, lamb_deliver_t *deliver);
int lamb_read_config(lamb_config_t *conf, const char *file);
bool lamb_is_online(lamb_cache_t *cache, int id);

#endif
