
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>
#include "utils.h"
#include "cache.h"

typedef struct {
    int id;
    bool debug;
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
    char ac_host[16];
    int ac_port;
    char mt_host[16];
    int mt_port;
    char mo_host[16];
    int mo_port;
    char *nodes[7];
} lamb_config_t;

typedef struct {
    unsigned long long sub;
    unsigned long long ack;
    unsigned long long rep;
    unsigned long long delv;
    unsigned long long timeo;
    unsigned long long err;
} lamb_status_t;

typedef struct {
    int account;
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
void *lamb_work_loop(void *data);
void *lamb_cmpp_keepalive(void *data);
void lamb_cmpp_reconnect(cmpp_sp_t *cmpp, lamb_config_t *config);
int lamb_cmpp_init(cmpp_sp_t *cmpp, lamb_config_t *config);
int lamb_save_logfile(char *file, void *data);
void *lamb_stat_loop(void *data);
int lamb_read_config(lamb_config_t *conf, const char *file);
int lamb_set_cache(lamb_caches_t *caches, unsigned long long msgId, unsigned long long id, int account);
int lamb_get_cache(lamb_caches_t *caches, unsigned long long *id, int *account);
int lamb_del_cache(lamb_caches_t *caches, unsigned long long msgId);

#endif
