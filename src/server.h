
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SERVER_H
#define _LAMB_SERVER_H

typedef struct {
    int id;
    bool debug;
    bool daemon;
    char logfile[128];
    int sender;
    int deliver;
    int work_threads;
    long long queue;
    char redis_host[16];
    int redis_port;
    char redis_password[64];
    int redis_db;
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[64];
    char db_name[64];
} lamb_config_t;

typedef struct {
    int type;
    unsigned long long id;
    char phone[24];
    char spcode[24];
    char content[160];
} lamb_submit_t;

int lamb_read_config(lamb_config_t *conf, const char *file);
int lamb_check_template(char *pattern, char *message, int len);

#endif
