
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_SCHEDULER_H
#define _LAMB_SCHEDULER_H

#include <stdbool.h>
#include <pthread.h>
#include "db.h"
#include "list.h"
#include "channel.h"

typedef struct {
    int id;
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    char ac[128];
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
    char logfile[128];
} lamb_config_t;

void lamb_event_loop(void);
void *lamb_test_loop(void *arg);
void *lamb_push_loop(void *arg);
void *lamb_pull_loop(void *arg);
int lamb_server_init(int *sock, const char *addr, int port);
int lamb_child_server(int *sock, const char *listen, unsigned short *port, int protocol);
void *lamb_stat_loop(void *arg);
void lamb_route_channel(lamb_db_t *db, int id, lamb_list_t *channels);
bool lamb_check_operator(lamb_channel_t *channel, char *phone);
bool lamb_check_province(lamb_channel_t *channel, char *phone);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
