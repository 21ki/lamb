
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "channel.h"

int lamb_channel_get(lamb_db_t *db, int gid, lamb_channel_t *channels[], size_t size) {
    int rows;
    int count;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    count = 0;
    column = "id, gid, weight";
    sprintf(sql, "SELECT %s FROM channels WHERE gid = %d", column, gid);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 0;
    }

    rows = PQntuples(res);
    if (rows < 1) {
        return 0;
    }

    for (int i = 0, j = 0; (i < rows) && (i < size); i++, j++) {
        lamb_channel_t *c = NULL;
        c = malloc(sizeof(lamb_channel_t));
        if (c != NULL) {
            memset(c, 0, sizeof(lamb_channel_t));
            c->id = atoi(PQgetvalue(res, i, 0));
            c->gid = atoi(PQgetvalue(res, i, 1));
            c->weight = atoi(PQgetvalue(res, i, 2));
            channels[j] = c;
            count++;
        } else {
            j--;
        }
    }

    PQclear(res);

    return count;
}
