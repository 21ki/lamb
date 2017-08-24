
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-20
 */

#ifndef _LAMB_DB_H
#define _LAMB_DB_H

#include <pthread.h>
#include <libpq-fe.h>

typedef struct {
    leveldb_t *conn;
    pthread_mutex_t lock;
} lamb_db_t;

int lamb_db_init(lamb_db_t *db);
int lamb_db_connect(lamb_db_t *db, char *host, int port, char *user, char *password, char *dbname);
bool lamb_db_check_status(lamb_db_t *db);
int lamb_db_close(lamb_db_t *db);


#endif
