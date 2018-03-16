
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_MO_H
#define _LAMB_MO_H

#include <stdbool.h>
#include <pthread.h>
#include "queue.h"

typedef struct {
    int id;
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    char ac[128];
    char logfile[128];
} lamb_config_t;

typedef struct {
    int type;
    void *data;
} lamb_element;

void lamb_event_loop(void);
void *lamb_push_loop(void *arg);
void *lamb_pull_loop(void *arg);
int lamb_server_init(int *sock, const char *listen, int port);
int lamb_child_server(int *sock, const char *host, unsigned short *port, int protocol);
void *lamb_stat_loop(void *arg);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
