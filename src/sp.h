
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>
#include "common.h"
#include "db.h"
#include "cache.h"

typedef struct {
    int id;
    bool debug;
    int retry;
    int interval;
    long timeout;
    long send_timeout;
    long recv_timeout;
    long acknowledge_timeout;
    bool extended;
    int concurrent;
    char backfile[128];
    char logfile[128];
    char ac[128];
    char scheduler[128];
    char delivery[128];
    char redis_host[16];
    int redis_port;
    char redis_password[64];
    int redis_db;
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
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
    int gid;
    unsigned long long submit;
    unsigned long long delivrd;
    unsigned long long expired;
    unsigned long long deleted;
    unsigned long long undeliv;
    unsigned long long acceptd;
    unsigned long long unknown;
    unsigned long long rejectd;
    pthread_mutex_t lock;
} lamb_statistical_t;

typedef struct {
    int account;
    int company;
    char spcode[24];
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
void *lamb_stat_loop(void *data);
void *lamb_online_loop(void *arg);
int lamb_state_renewal(lamb_cache_t *cache, int id);
void lamb_clean_statistical(lamb_statistical_t *stat);
int lamb_read_config(lamb_config_t *conf, const char *file);
int lamb_set_cache(lamb_caches_t *caches, unsigned long long msgId, unsigned long long id, int account, int company, char *spcode);
int lamb_get_cache(lamb_caches_t *caches, unsigned long long id, unsigned long long *msgId, int *account, int *company, char *spcode, size_t size);
int lamb_del_cache(lamb_caches_t *caches, unsigned long long msgId);
void lamb_check_statistical(int status, lamb_statistical_t *stat);
int lamb_write_statistical(lamb_db_t *db, lamb_statistical_t *stat);
int lamb_check_signal(lamb_cache_t *cache, int id);
void lamb_clear_signal(lamb_cache_t *cache, int id);
void lamb_exit_cleanup(void);

#endif
