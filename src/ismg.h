
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_ISMG_H
#define _LAMB_ISMG_H

#include <stdbool.h>

typedef struct {
    int id;
    char listen[16];
    int port;
    int connections;
    char logfile[128];
    bool debug;
    bool daemon;
} lamb_config_t;

void lamb_event_loop(void);
void lamb_accept_loop(cmpp_ismg_t *cmpp);
int lamb_read_config(lamb_config_t *conf, const char *file);

#endif
