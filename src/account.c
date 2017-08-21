
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <hiredis/hiredis.h>
#include "account.h"

int lamb_account_get(lamb_cache_t *cache, char *user, lamb_account_t *account) {
    if (!cache || !cache->handle || !account) {
        return -1;
    }

    redisReply *reply = NULL;
    const char *cmd = "HMGET account.%s company charge_type ip_addr route spcode concurrent extended policy check_template";
    reply = redisCommand(cache->handle, cmd);

    if (reply != NULL) {

    }
}
