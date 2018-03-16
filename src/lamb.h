
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_LAMB_H
#define _LAMB_LAMB_H

#include "linenoise.h"

int lamb_sock_connect(int *sock, char *host, int port);
void completion(const char *buf, linenoiseCompletions *lc);
char *hints(const char *buf, int *color, int *bold);
void lamb_show_client(int sock, int id);
int lamb_signal(int sig, void (*handler)(int));
void lamb_signal_processing(void);

#endif
