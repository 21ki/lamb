
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
    char msg_host[16];
    int msg_port;
    char msg_user[64];
    char msg_password[64];
    char msg_name[64];
    char *nodes[7];
} lamb_config_t;

typedef struct {
    unsigned long long id;
    int status;
} lamb_report_pack;

typedef struct {
    int company;
    int money;
} lamb_charge_pack;

typedef struct {
    int type;
    int account;
    int company;
    void *data;
} lamb_delivery_pack;

void lamb_event_loop(void);
void *lamb_deliver_worker(void *data);
void *lamb_report_loop(void *data);
void *lamb_delivery_loop(void *data);
int lamb_update_report(lamb_db_t *db, lamb_report_pack *report);
int lamb_write_report(lamb_db_t *db, int account, int company, lamb_report_t *report);
int lamb_write_deliver(lamb_db_t *db, lamb_deliver_t *deliver);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
