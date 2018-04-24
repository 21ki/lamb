
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_DAEMON_H
#define _LAMB_DAEMON_H

#include "db.h"
#include "common.h"
#include "config.h"

typedef struct {
    int id;
    bool debug;
    long long timeout;
    char logfile[128];
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
    char module[255];
    char config[255];
} lamb_config_t;

typedef struct {
    long long id;
    int eid;
    char mod[255];
    char config[255];
    char argv[255];
} lamb_task_t;

void lamb_event_loop(void);
int lamb_fetch_taskqueue(lamb_db_t *db, lamb_task_t *task);
int lamb_del_taskqueue(lamb_db_t *db, long long id);
void lamb_start_program(lamb_task_t *task);
int lamb_component_initialization(lamb_config_t *cfg);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
