
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "route.h"

int lamb_route_get(lamb_db_t *db, int id, lamb_route_t *route) {
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, spcode, account";
    sprintf(sql, "SELECT %s FROM routes WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    if (PQntuples(res) < 1) {
        return 2;
    }

    route->id = atoi(PQgetvalue(res, 0, 0));
    strncpy(route->spcode, PQgetvalue(res, 0, 1), 31);
    strncpy(route->account, PQgetvalue(res, 0, 2), 31);
    
    PQclear(res);

    return 0;
}

int lamb_route_get_all(lamb_db_t *db, lamb_route_t *routes[], size_t size) {
    int rows = 0;
    char *column;
    char sql[256];
    PGresult *res = NULL;
    
    column = "id, spcode, account";
    sprintf(sql, "SELECT %s FROM routes ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    rows = PQntuples(res);
    if (rows < 1) {
        return 2;
    }

    for (int i = 0; (i < rows) && (i < size); i++) {
        lamb_route_t *r= NULL;
        r = (lamb_route_t *)malloc(sizeof(lamb_route_t));
        if (r != NULL) {
            memset(r, 0, sizeof(lamb_route_t));
            r->id = atoi(PQgetvalue(res, i, 0));
            strncpy(r->spcode, PQgetvalue(res, i, 1), 31);
            strncpy(r->account, PQgetvalue(res, i, 2), 31);
            routes[i] = r;
        }
    }

    PQclear(res);
    return 0;
}

int lamb_route_query(lamb_db_t *db, char *spcode) {
    return 0;
}
