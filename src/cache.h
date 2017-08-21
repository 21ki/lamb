
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-20
 */

#ifndef _LAMB_CACHE_H
#define _LAMB_CACHE_H

#include <stdbool.h>
#include <hiredis/hiredis.h>

typedef struct {
    redisContext *handle;
} lamb_cache_t;

int lamb_cache_connect(lamb_cache_t *cache, char *host, int port, char *password, int db);
bool lamb_cache_check_connect(lamb_cache_t *cache);
int lamb_cache_close(lamb_cache_t *cache);
bool lamb_cache_has(lamb_cache_t *cache, char *key);
int lamb_cache_get(lamb_cache_t *cache, char *key, char *buff, size_t len);

#endif
