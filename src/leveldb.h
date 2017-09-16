
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-20
 */

#ifndef _LAMB_LEVELDB_H
#define _LAMB_LEVELDB_H

#include <leveldb/c.h>

typedef struct {
    char *name;
    leveldb_t *handle;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} lamb_leveldb_t;

int lamb_level_init(lamb_leveldb_t *db, const char *name);
int lamb_level_open(lamb_leveldb_t *db, const char *name);
int lamb_level_put(lamb_leveldb_t *db, const char *key, size_t keylen, const char *val, size_t vallen);
char *lamb_level_get(lamb_leveldb_t *db, const char *key, size_t keylen, size_t *len);
int lamb_level_delete(lamb_leveldb_t *db, const char *key, size_t keylen);
int lamb_level_close(lamb_leveldb_t *db);

#endif
