
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SECURITY_H
#define _LAMB_SECURITY_H

#include <stdbool.h>

#define _GNU_SOURCE
#include <search.h>

#define LAMB_POL_EMPTY 0
#define LAMB_BLACKLIST 1
#define LAMB_WHITELIST 2

bool lamb_security_check(struct hsearch_data *htab, int type, char *phone);

#endif
