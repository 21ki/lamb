
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>

typedef struct {
    int id;
    bool debug;
    bool daemon;
    char host[16];
    int port;
    char user[64];
    char password[128];
    char spid[8];
    char spcode[16];
    char encoding[32];
    int retry;
    int interval;
    long long timeout;
    long long send_timeout;
    long long recv_timeout;
    bool extended;
    int concurrent;
    char backfile[128];
    char logfile[128];
    char redis_host[16];
    int redis_port;
    char redis_password[64];
    int redis_db;
    char cache[128];
    lamb_operator_t sp;
} lamb_config_t;

typedef struct {
    unsigned int sequenceId;
    unsigned long long id;
} lamb_confirmed_t;

typedef struct {
    int count;
    unsigned int sequenceId;
} lamb_heartbeat_t;

void lamb_event_loop(void);
void *lamb_sender_loop(void *data);
void *lamb_deliver_loop(void *data);
void *lamb_cmpp_keepalive(void *data);
void lamb_cmpp_reconnect(cmpp_sp_t *cmpp, lamb_config_t *config);
int lamb_cmpp_init(cmpp_sp_t *cmpp, lamb_config_t *config);
int lamb_save_logfile(char *file, void *data);
void *lamb_online_update(void *data);
void *lamb_recovery_loop(void *data);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
