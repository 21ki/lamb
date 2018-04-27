
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_MT_H
#define _LAMB_MT_H

#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <stdbool.h>
#include <pthread.h>
#include <syslog.h>

typedef struct {
    int id;
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    char ac[128];
    char redis_host[16];
    int redis_port;
    char redis_password[64];
    int redis_db;
    char logfile[128];
} lamb_config_t;

void lamb_event_loop(void);
void *lamb_push_loop(void *arg);
void *lamb_pull_loop(void *arg);
int lamb_child_server(int *sock, const char *listen, unsigned short *port, int protocol);
void *lamb_stat_loop(void *arg);
int lamb_sync_update(lamb_cache_t *cache, int id, unsigned int num);
void lamb_reset_queues(lamb_cache_t *cache);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
