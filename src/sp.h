
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>

#define LAMB_UPDATE  1
#define LAMB_REPORT  2
#define LAMB_DELIVER 3

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
    char logfile[128];
} lamb_config_t;

typedef struct {
    unsigned int sequenceId;
    unsigned long long msgId;
} lamb_seqtable_t;

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
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
