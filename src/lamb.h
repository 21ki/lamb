
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_LAMB_H
#define _LAMB_LAMB_H

#include "db.h"
#include "cache.h"
#include "list.h"
#include "linenoise.h"

typedef struct {
    int len;
    char *val[16];
} lamb_opt_t;

typedef struct {
    int id;
} lamb_config_t;

typedef struct {
    int id;
    char rexp[128];
    int target;
} lamb_delivery_t;

typedef struct {
    int id;
    char *acc;
    long total;
    char *desc;
} lamb_kv_t;

void lamb_help(const char *line);
void lamb_show_mt(const char *line);
void lamb_show_mo(const char *line);
void lamb_show_core(const char *line);
void lamb_show_client(const char *line);
void lamb_show_server(const char *line);
void lamb_show_account(const char *line);
void lamb_show_gateway(const char *line);
void lamb_show_channel(const char *line);
void lamb_show_log(const char *line);
void lamb_show_routing(const char *line);
void lamb_show_delivery(const char *line);
void lamb_start_server(const char *line);
void lamb_start_gateway(const char *line);
void lamb_kill_client(const char *line);
void lamb_kill_server(const char *line);
void lamb_kill_gateway(const char *line);
void lamb_change_password(const char *line);
void lamb_show_version(const char *line);
int lamb_opt_parsing(const char *cmd, const char *prefix, lamb_opt_t *opt);
void lamb_opt_free(lamb_opt_t *opt);
int lamb_get_delivery(lamb_db_t *db, lamb_list_t *deliverys);
void completion(const char *buf, linenoiseCompletions *lc);
void lamb_component_initialization(lamb_config_t *cfg);
int lamb_add_taskqueue(lamb_db_t *db, int eid, char *mod, char *config, char *argv);
int lamb_get_queue(lamb_cache_t *cache, const char *type, lamb_list_t *queues);
void lamb_md5(const void *data, unsigned long len, char *string);
void lamb_sha1(const void *data, size_t len, char *string);
void lamb_hex_string(unsigned char* digest, size_t len, char* string);
int lamb_set_password(lamb_cache_t *cache, const char *password);
void lamb_check_status(const char *lock, int *pid, int *status);
void lamb_check_channel(lamb_cache_t *cache, int id, int *status, int *speed, int *error);
void lamb_check_client(lamb_cache_t *cache, int id, char *host, int *status, int *speed, int *error);

#endif
