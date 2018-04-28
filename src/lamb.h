
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_LAMB_H
#define _LAMB_LAMB_H

#include "linenoise.h"

void completion(const char *buf, linenoiseCompletions *lc);
void lamb_show_version(void);
void lamb_show_core(void);
void lamb_show_client(void);
void lamb_show_server(void);
void lamb_show_gateway(void);
void lamb_change_password(const char *line);

#endif
