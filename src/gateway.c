
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdlib.h>
#include <string.h>
#include "gateway.h"

int lamb_get_gateway(lamb_db_t *db, int id, lamb_gateway_t *gateway) {
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id,type,host,port,username,password,spid,spcode,encoding,extended,concurrent";
    snprintf(sql, sizeof(sql), "SELECT %s FROM gateway WHERE id = %d", column, id);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        PQclear(res);
        return -1;
    }

    gateway->id = atoi(PQgetvalue(res, 0, 0));
    gateway->type = atoi(PQgetvalue(res, 0, 1));
    strncpy(gateway->host, PQgetvalue(res, 0, 2), 31);
    gateway->port = atoi(PQgetvalue(res, 0, 3));
    strncpy(gateway->username, PQgetvalue(res, 0, 4), 31);
    strncpy(gateway->password, PQgetvalue(res, 0, 5), 63);
    strncpy(gateway->spid, PQgetvalue(res, 0, 6), 7);
    strncpy(gateway->spcode, PQgetvalue(res, 0, 7), 20);
    gateway->encoding = atoi(PQgetvalue(res, 0, 8));
    gateway->extended = atoi(PQgetvalue(res, 0, 9)) ? true : false;
    gateway->concurrent = atoi(PQgetvalue(res, 0, 10));

    PQclear(res);

    return 0;
}
