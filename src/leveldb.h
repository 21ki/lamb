
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-20
 */

#ifndef _LAMB_DB_H
#define _LAMB_DB_H

#include <leveldb/c.h>

typedef struct {
    char *name;
    leveldb_t *handle;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} lamb_db_t;

int lamb_db_init(lamb_db_t *db, const char *name);
int lamb_db_open(lamb_db_t *db, const char *name);
int lamb_db_put(lamb_db_t *db, const char *key, size_t keylen, const char *val, size_t vallen);
char *lamb_db_get(lamb_db_t *db, const char *key, size_t keylen, size_t *len);
int lamb_db_delete(lamb_db_t *db, const char *key, size_t keylen);
int lamb_db_close(lamb_db_t *db);

#endif
