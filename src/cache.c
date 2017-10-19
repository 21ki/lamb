
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-20
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "cache.h"
#include "utils.h"

int lamb_cache_connect(lamb_cache_t *cache, char *host, int port, char *password, int db) {
    redisReply *reply = NULL;
    struct timeval timeout = {3, 500000};
    
    cache->handle = redisConnectWithTimeout(host, port, timeout);
    if (cache->handle == NULL|| cache->handle->err) {
        if (cache->handle) {
            redisFree(cache->handle);
        }

        return 1;
    }

    if (password) {
        reply = redisCommand(cache->handle, "AUTH %s", password);
        if (reply != NULL) {
            freeReplyObject(reply);
        }
    }

    reply = redisCommand(cache->handle, "SELECT %d", db);
    if (reply != NULL) {
        if ((reply->type == REDIS_REPLY_STATUS) && (strcmp(reply->str, "OK") == 0)) {
            freeReplyObject(reply);
            pthread_mutex_init(&cache->lock, NULL);
            return 0;
        }
        freeReplyObject(reply);
    }

    redisFree(cache->handle);
    return 2;
}

bool lamb_cache_check_connect(lamb_cache_t *cache) {
    if (!cache || cache->handle) {
        return false;
    }
    
    bool r = false;
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "PING");
    if (reply != NULL) {
        if (strcmp(reply->str, "PONG") == 0) {
            r = true;
        }
        freeReplyObject(reply);
    }

    return r;
}

int lamb_cache_close(lamb_cache_t *cache) {
    if (cache->handle) {
        redisFree(cache->handle);
        pthread_mutex_destroy(&cache->lock);
    }

    return 0;
}

bool lamb_cache_has(lamb_cache_t *cache, char *key) {
    if (!cache || !cache->handle) {
        return false;
    }

    bool r = false;
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "EXISTS %s", key);
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_INTEGER) {
            r = (reply->integer == 1) ? true : false;
        }
        freeReplyObject(reply);
    }

    return r;
}

int lamb_cache_get(lamb_cache_t *cache, char *key, char *buff, size_t len) {
    if (!cache || !cache->handle || !key) {
        return -1;
    }

    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "GET %s", key);
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING) {
            if (reply->len > len) {
                memcpy(buff, reply->str, len - 1);
            } else {
                memcpy(buff, reply->str, reply->len);
            }
        }

        freeReplyObject(reply);
    }

    return 0;
}

int lamb_cache_hget(lamb_cache_t *cache, char *key, char *field, char *buff, size_t len) {
    if (!cache || !cache->handle || !key) {
        return -1;
    }

    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HGET %s %s", key, field);
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING) {
            if (reply->len > len) {
                memcpy(buff, reply->str, len - 1);
            } else {
                memcpy(buff, reply->str, reply->len);
            }
        }

        freeReplyObject(reply);
    }

    return 0;
}

int lamb_nodes_connect(lamb_caches_t *cache, int len, char *nodes[], int size, int db) {
    int err;
    char host[16];
    int port = 0;
    lamb_cache_t *node;

    cache->len = 0;

    for (int i = 0, j = 0; (j < len) && (i < size) && (nodes[i] != NULL) && (*nodes[i] != '\0'); i++, j++) {
        memset(host, 0, sizeof(host));
        lamb_hp_parse(nodes[i], host, &port);
        node = (lamb_cache_t *)calloc(1, sizeof(lamb_cache_t));
        if (node != NULL) {
            err = lamb_cache_connect(node, host, port, NULL, db);
            if (!err) {
                cache->nodes[j] = node;
                cache->len++;
            } else {
                free(node);
                if (j > 0) {
                    j--;
                }
            }
        } else {
            if (j > 0) {
                j--;
            }
        }
    }

    return 0;
}
