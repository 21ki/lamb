
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "keyword.h"

int lamb_keyword_get_all(lamb_db_t *db, lamb_list_t *keys) {
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, val";
    snprintf(sql, sizeof(sql), "SELECT %s FROM keyword ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0; i < rows; i++) {
        lamb_keyword_t *key = (lamb_keyword_t *)malloc(sizeof(lamb_keyword_t));
        if (key) {
            key->id = atoi(PQgetvalue(res, i, 0));
            key->val = lamb_strdup(PQgetvalue(res, i, 1));
            lamb_list_rpush(keys, lamb_node_new(key));
        }
    }

    PQclear(res);
    return 0;
}

