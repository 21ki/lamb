
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
    int err;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    group->id = id;
    group->len = 0;
    column = "id";
    sprintf(sql, "SELECT %s FROM groups WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return 1;
    }

    PQclear(res);

    group->channels = (lamb_channels_t *)calloc(1, sizeof(lamb_channels_t));
    err = lamb_get_channels(db, id, group->channels, LAMB_MAX_CHANNEL);
    if (err) {
        free(group->channels);
        return 2;
    }

    group->len = group->channels->len;    
    return 0;
}

int lamb_group_get_all(lamb_db_t *db, lamb_groups_t *groups, int size) {
    int err;
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    groups->len = 0;
    column = "id";
    sprintf(sql, "SELECT %s FROM groups ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0, j = 0; (i < rows) && (i < size); i++, j++) {
        lamb_group_t *g = NULL;
        g = (lamb_group_t *)calloc(1, sizeof(lamb_group_t));
        if (g != NULL) {
            int gid = atoi(PQgetvalue(res, i, 0));
            err = lamb_group_get(db, gid, g);
            if (!err) {
                groups->list[j] = g;
                groups->len++;
            }
        } else {
            if (j > 0) {
                j--;
            }
        }
    }

    PQclear(res);
    return 0;
}
