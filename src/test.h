
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_TEST_H
#define _LAMB_TEST_H

#include "db.h"
#include "common.h"
#include "config.h"
#include "message.h"

typedef struct {
    int id;
    bool debug;
    long long timeout;
    char logfile[128];
    char scheduler[128];
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
} lamb_config_t;

void lamb_event_loop(void);
int lamb_fetch_message(lamb_db_t *db, int *channel, lamb_submit_t *message);
int lamb_check_response(int sock);
int lamb_update_message(lamb_db_t *db, unsigned long long id, int status);
int lamb_nn_testsched(const char *host, int id, int timeout);
int lamb_component_initialization(lamb_config_t *cfg);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
