
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ISMG_H
#define _LAMB_ISMG_H

#include <stdbool.h>

typedef struct {
    int id;
    char listen[16];
    int port;
    int connections;
    long long timeout;
    long long send_timeout;
    long long recv_timeout;
    char amqp_host[16];
    int amqp_port;
    char amqp_user[64];
    char amqp_password[64];
    char queue[64];
    char logfile[128];
    bool debug;
    bool daemon;
} lamb_config_t;

void lamb_event_loop(cmpp_ismg_t *cmpp);
void lamb_work_loop(cmpp_sock_t *sock);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
