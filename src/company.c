
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

    column = "id, paytype";
    sprintf(sql, "SELECT %s FROM groups WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    if (PQntuples(res) < 1) {
        return 2;
    }

    company->id = atoi(PQgetvalue(res, 0, 0));
    company->paytype = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);

    return 0;
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

