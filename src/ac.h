
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_AC_H
#define _LAMB_AC_H

#include <stdbool.h>

typedef struct {
    int id;
    char listen[16];
    int port;
    int connections;
    long long timeout;
    char redis_host[16];
    int redis_port;
    int redis_db;
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
    char logfile[128];
} lamb_config_t;

typedef struct {
    int id;
    int sock;
    char addr[16];
} lamb_client_t;

void lamb_event_loop(void);
void *lamb_mt_loop(void *arg);
void *lamb_mo_loop(void *arg);
void *lamb_ismg_loop(void *arg);
void *lamb_server_loop(void *arg);
void *lamb_scheduler_loop(void *arg);
void *lamb_delivery_loop(void *arg);
void *lamb_gateway_loop(void *arg);
int lamb_component_initialization(lamb_config_t *cfg);
int lamb_child_server(int *sock, const char *host, unsigned short *port, int protocol);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
