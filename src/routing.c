
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdlib.h>
#include <string.h>
#include "routing.h"
#include "channel.h"

int lamb_get_routing(lamb_db_t *db, lamb_list_t *routings) {
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, rexp, target";
    snprintf(sql, sizeof(sql), "SELECT %s FROM routing ORDER BY id ASC", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0; i < rows; i++) {
        lamb_routing_t *routing;
        routing = (lamb_routing_t *)calloc(1, sizeof(lamb_routing_t));
        if (routing != NULL) {
            routing->id = atoi(PQgetvalue(res, i, 0));
            strncpy(routing->rexp, PQgetvalue(res, i, 1), 127);
            routing->target = atoi(PQgetvalue(res, i, 2));
            lamb_list_rpush(routings, lamb_node_new(routing));
        }
    }

    PQclear(res);
    return 0;
}

int lamb_rexp_routing(lamb_db_t *db, const char *rexp) {
    int target;
    char sql[256];
    PGresult *res = NULL;

    target = 0;
    snprintf(sql, sizeof(sql), "SELECT target FROM routing WHERE rexp = '%s' LIMIT 1", rexp);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) > 0) {
        target = atoi(PQgetvalue(res, 0, 0));
    }

    PQclear(res);
    return target;
}
