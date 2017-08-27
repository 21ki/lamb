
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "group.h"
#include "channel.h"

int lamb_group_get(lamb_db_t *db, int id, lamb_group_t *group) {
    int count;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    count = 0;
    column = "id";
    sprintf(sql, "SELECT %s FROM groups WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    if (PQntuples(res) < 1) {
        return 2;
    }

    group->id = atoi(PQgetvalue(res, 0, 0));

    PQclear(res);

    count = lamb_channel_get(db, id, group->channels, 1024);
    group->len = count;
    
    return 0;
}

int lamb_group_get_all(lamb_db_t *db, lamb_group_t *groups[], size_t size) {
    int err;
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;
    
    column = "id";
    sprintf(sql, "SELECT %s FROM groups ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    rows = PQntuples(res);
    if (rows < 1) {
        return 2;
    }

    for (int i = 0, j = 0; (i < rows) && (i < size); i++, j++) {
        lamb_group_t *g = NULL;
        g = (lamb_group_t *)malloc(sizeof(lamb_group_t));
        if (g != NULL) {
            int gid = atoi(PQgetvalue(res, i, 0));
            err = lamb_group_get(db, gid, g);
            if (err) {
                g->id = gid;
                g->len = 0;
            }
            groups[j] = g;
        } else {
            j--;
        }
    }

    PQclear(res);
    return 0;
}
