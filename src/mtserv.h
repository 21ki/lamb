
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ISMG_H
#define _LAMB_ISMG_H

#include <stdbool.h>

#define LAMB_MAX_WORK_THREAD 1024

typedef struct {
    bool debug;
    char listen[16];
    int port;
    long long timeout;
    int worker_thread;
    char logfile[128];
} lamb_config_t;

void lamb_event_loop(void);
void *lamb_work_loop(void *arg);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
