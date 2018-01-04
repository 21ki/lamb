
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_DELIVER_H
#define _LAMB_DELIVER_H

#include <stdbool.h>
#include "list.h"
#include "queue.h"
#include "db.h"

typedef struct {
    int id;
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    char ac_host[16];
    int ac_port;
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
    char logfile[128];
} lamb_config_t;

typedef struct {
    int id;
    char rexp[128];
    int target;
} lamb_delivery_t;

void lamb_event_loop(void);
void *lamb_push_loop(void *arg);
void *lamb_pull_loop(void *arg);
int lamb_server_init(int *sock, const char *addr, int port);
int lamb_child_server(int *sock, const char *listen, unsigned short *port, int protocol);
void *lamb_stat_loop(void *arg);
void *lamb_store_loop(void *data);
int lamb_get_delivery(lamb_db_t *db, lamb_list_t *deliverys);
bool lamb_check_delivery(lamb_delivery_t *d, char *spcode, size_t len);
int lamb_write_deliver(lamb_db_t *db, lamb_deliver_t *message);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
