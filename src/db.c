
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-20
 */

#include "db.h"

int lamb_db_init(lamb_db_t *db) {
    db->conn = NULL;
    pthread_mutex_init(&db->lock, NULL);

    return 0;
}

int lamb_db_connect(lamb_db_t *db, char *host, int port, char *user, char *password, char *dbname) {
    char info[512];
    char *string = "host=%s port=%d user=%s password=%s dbname=%s connect_timeout=3";
    sprintf(info, string, host, port, user, password, dbname);

    db->conn = PQconnectdb(info);
    if (PQstatus(db->conn) != CONNECTION_OK) {
        return -1;
    }

    return 0;
}

bool lamb_db_check_status(lamb_db_t *db) {
    if (PQstatus(db->conn) == CONNECTION_OK) {
        return true;
    }

    return false;
}

int lamb_db_close(lamb_db_t *db) {
    PQfinish(db->conn);
    pthread_mutex_destroy(&db->lock);
    return 0;
}
