
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "channel.h"

int lamb_get_channels(lamb_db_t *db, int gid, lamb_channels_t *channels, int size) {
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    channels->len = 0;
    column = "id, gid, weight";
    sprintf(sql, "SELECT %s FROM channels WHERE gid = %d ORDER BY weight ASC", column, gid);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0, j = 0; (i < rows) && (i < size); i++, j++) {
        lamb_channel_t *c = NULL;
        c = (lamb_channel_t)calloc(1, sizeof(lamb_channel_t));
        if (c != NULL) {
            c->id = atoi(PQgetvalue(res, i, 0));
            c->gid = atoi(PQgetvalue(res, i, 1));
            c->weight = atoi(PQgetvalue(res, i, 2));
            channels->list[j] = c;
            channels->len++;
        } else {
            if (j > 0) {
                j--;
            }
        }
    }

    PQclear(res);

    return count;
}
