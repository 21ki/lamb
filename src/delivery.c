
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "delivery.h"

int lamb_get_delivery(lamb_db_t *db, lamb_queue_t *deliverys) {
    int rows;
    char sql[128];
    PGresult *res = NULL;

    sprintf(sql, "SELECT id, rexp, target FROM delivery");
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return -1;
    }

    rows = PQntuples(res);
    
    for (int i = 0; i < rows; i++) {
        lamb_delivery_t *delivery;
        delivery = (lamb_delivery_t *)calloc(1, sizeof(lamb_delivery_t));
        if (delivery) {
            delivery->id = atoi(PQgetvalue(res, i, 0));
            strncpy(delivery->rexp, PQgetvalue(res, i, 1), 127);
            delivery->target = atoi(PQgetvalue(res, i, 2));
            lamb_queue_push(deliverys, delivery);
        }
    }

    PQclear(res);

    return 0;
}
