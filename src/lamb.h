
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_LAMB_H
#define _LAMB_LAMB_H

#include "linenoise.h"

typedef struct {
    int len;
    char *val[16];
} lamb_opt_t;

typedef struct {
    int id;
} lamb_config_t;

void completion(const char *buf, linenoiseCompletions *lc);
void lamb_component_initialization(lamb_config_t *cfg);
void lamb_show_version(void);
void lamb_show_core(void);
void lamb_show_client(void);
void lamb_show_server(void);
void lamb_show_account(void);
void lamb_show_gateway(void);
void lamb_show_log(const char *line);
void lamb_show_routing(const char *line);
void lamb_start_gateway(const char *line);
void lamb_change_password(const char *line);
int lamb_opt_parsing(const char *cmd, const char *prefix, lamb_opt_t *opt);
void lamb_opt_free(lamb_opt_t *opt);

#endif
