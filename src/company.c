
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdlib.h>
#include <string.h>
#include "company.h"

int lamb_company_get(lamb_cache_t *cache, int id, lamb_company_t *company) {
    if (!cache || !cache->handle || !company) {
        return -1;
    }

    int r = 0;
    redisReply *reply = NULL;
    const char *cmd = "HMGET company.%d paytype";
    reply = redisCommand(cache->handle, cmd, id);

    company->id = id;

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            for (int i = 0; i < reply->elements; i++) {
                if (reply->element[i]->len == 0) {
                    r = 1;
                    break;
                }

                switch (i) {
                case 0:
                    account->company = atoi(reply->element[0]->str);
                    break;
            }
        }

        freeReplyObject(reply);
    }

    return r;
}

int lamb_company_get_all(lamb_db_t *db, lamb_company_t *companys[], size_t size) {
    int rows = 0;
    char *sql = NULL;
    PGresult *res = NULL;
    
    sql = "SELECT id, paytype FROM company ORDER BY id";
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
        lamb_company_t *c= NULL;
        c = (lamb_company_t *)malloc(sizeof(lamb_company_t));
        if (c != NULL) {
            memset(c, 0, sizeof(lamb_company_t));
            c->id = atoi(PQgetvalue(res, i, 0));
            c->paytype = atoi(PQgetvalue(res, i, 1));
            companys[i] = c;
        }
    }

    PQclear(res);
    return 0;
}

