
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ISMG_H
#define _LAMB_ISMG_H

#include <stdbool.h>
#include "account.h"

#define lamb_deliver 1
#define lamb_report 2

typedef struct {
    int id;
    char listen[16];
    int port;
    int connections;
    long long timeout;
    long long send_timeout;
    long long recv_timeout;
    char queue[64];
    char redis_host[16];
    int redis_port;
    int redis_db;
    char logfile[128];
    bool debug;
    bool daemon;
} lamb_config_t;
    
typedef struct {
    int type;
    unsigned long long id;
    char phone[24];
    char spcode[24];
    char content[160];
} lamb_message_t;

typedef struct {
    int type;
    unsigned long long id;
    char phone[24];
    char spcode[24];
    char content[160];
} lamb_deliver_t;

typedef struct {
    int type;
    unsigned long long id;
    char phone[24];
    char status[8];
    char submitTime[16];
    char doneTime[16];
} lamb_report_t;

void lamb_event_loop(cmpp_ismg_t *cmpp);
void lamb_work_loop(cmpp_sock_t *sock, lamb_account_t *account);
int lamb_read_config(lamb_config_t *conf, const char *file);
void *lamb_online(void *user);
void *lamb_deliver_loop(void *data);

#endif
