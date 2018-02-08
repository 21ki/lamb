
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdlib.h>
#include <string.h>
#include "company.h"

int lamb_company_get(lamb_db_t *db, int id, lamb_company_t *company) {
    char sql[256];
    char *column;
    PGresult *res = NULL;

    column = "id, money";
    snprintf(sql, sizeof(sql), "SELECT %s FROM company WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return 1;
    }

    company->id = atoi(PQgetvalue(res, 0, 0));
    company->money = atoi(PQgetvalue(res, 0, 1));
    company->status = 1;
    PQclear(res);

    return 0;
}

int lamb_company_get_all(lamb_db_t *db, lamb_companys_t *companys, int size) {
    int rows = 0;
    char *sql = NULL;
    PGresult *res = NULL;

    companys->len = 0;
    sql = "SELECT id FROM company ORDER BY id";
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0, j = 0; (i < rows) && (i < size); i++, j++) {
        lamb_company_t *c = NULL;
        c = (lamb_company_t *)calloc(1, sizeof(lamb_company_t));
        if (c != NULL) {
            c->id = atoi(PQgetvalue(res, i, 0));
            companys->list[j] = c;
            companys->len++;
        } else {
            if (j > 0) {
                j--;
            }
        }
    }

    PQclear(res);
    return 0;
}

int lamb_company_billing(lamb_cache_t *cache, int company, long long money) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HINCRBY company.%d money %d", company, money);
    if (reply != NULL) {
        freeReplyObject(reply);
        return 0;
    }
    
    return -1;
}

