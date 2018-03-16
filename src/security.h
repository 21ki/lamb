
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_SECURITY_H
#define _LAMB_SECURITY_H

#include <stdbool.h>
#include "cache.h"

#define LAMB_POL_EMPTY 0
#define LAMB_BLACKLIST 1
#define LAMB_WHITELIST 2

bool lamb_security_check(lamb_cache_t *cache, int type, char *phone);

#endif
