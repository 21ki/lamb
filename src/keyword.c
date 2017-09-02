
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "keyword.h"

int lamb_keyword_get_all(lamb_db_t *db, lamb_keywords_t *keys, int size) {
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;
    
    keys->len = 0;
    column = "val";
    sprintf(sql, "SELECT %s FROM keyword ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0; (i < rows) && (i < size); i++) {
        keys->list[i] = lamb_strdup(PQgetvalue(res, i, 1));
        keys->len++;
    }

    PQclear(res);
    return 0;
}

bool lamb_keyword_check(lamb_keywords_t *keys, char *content) {
    for (int i = 0; (i < keys->len) && (keys->list[i] != NULL); i++) {
        if (strstr(content, keys->list[i])) {
            return true;
        }
    }

    return false;
}
