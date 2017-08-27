
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SERVER_H
#define _LAMB_SERVER_H

#define LAMB_CLIENT       1
#define LAMB_GATEWAY      2

#define LAMB_MAX_CLIENT   1024
#define LAMB_MAX_GROUP    1024
#define LAMB_MAX_GATEWAY  1024
#define LAMB_MAX_COMPANY  1024
#define LAMB_MAX_TEMPLATE 1024
#define LAMB_MAX_ROUTE    1024

#define LAMB_SENDER_QUEUE  "/sender.queue"
#define LAMB_DELIVER_QUEUE "/deliver.queue"

typedef struct {
    int id;
    bool debug;
    bool daemon;
    char logfile[128];
    int sender;
    int deliver;
    int work_threads;
    long long sender_queue;
    long long deliver_queue;
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
    char data[512];
} lamb_message_t;

typedef struct {
    unsigned long long id;
    char phone[24];
    char spcode[24];
    char content[160];
} lamb_submit_t;

void lamb_event_loop(void);
void lamb_sender_loop(void);
void lamb_deliver_loop(void);
void lamb_start_worker(void *(*func)(void *), int count);
void *lamb_sender_worker(void *val);
void *lamb_deliver_worker(void *val);
int lamb_read_config(lamb_config_t *conf, const char *file);
int lamb_check_template(char *pattern, char *message, int len);

#endif
