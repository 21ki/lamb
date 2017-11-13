
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "routing.h"
#include "channel.h"

int lamb_get_routing(lamb_db_t *db, int id, lamb_queue_t *routing) {
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, rid, weight";
    sprintf(sql, "SELECT %s FROM channels WHERE rid = %d ORDER BY weight ASC", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0; i < rows; i++) {
        lamb_channel_t *channel;
        channel = (lamb_channel_t *)calloc(1, sizeof(lamb_channel_t));
        if (channel != NULL) {
            channel->id = atoi(PQgetvalue(res, i, 0));
            channel->rid = atoi(PQgetvalue(res, i, 1));
            channel->weight = atoi(PQgetvalue(res, i, 2));
            lamb_queue_push(routing, channel);
        }
    }

    PQclear(res);
    return 0;
}

