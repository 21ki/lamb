
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "template.h"

int lamb_get_templates(lamb_db_t *db, lamb_list_t *templates) {
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, rexp, name, contents";
    snprintf(sql, sizeof(sql), "SELECT %s FROM template ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0; i < rows; i++) {
        lamb_template_t *t = NULL;
        t = (lamb_template_t *)calloc(1, sizeof(lamb_template_t));
        if (t != NULL) {
            t->id = atoi(PQgetvalue(res, i, 0));
            strncpy(t->rexp, PQgetvalue(res, i, 1), 127);
            strncpy(t->name, PQgetvalue(res, i, 2), 63);
            strncpy(t->contents, PQgetvalue(res, i, 3), 511);
            lamb_list_rpush(templates, lamb_node_new(t));
        }
    }

    PQclear(res);
    return 0;
}

int lamb_get_template(lamb_db_t *db, const char *rexp, lamb_list_t *templates) {
    int rows;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, rexp, name, content";
    snprintf(sql, sizeof(sql), "SELECT %s FROM template WHERE rexp = '%s' ORDER BY id",
             column, rexp);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0; i < rows; i++) {
        lamb_template_t *t = NULL;
        t = (lamb_template_t *)calloc(1, sizeof(lamb_template_t));
        if (t != NULL) {
            t->id = atoi(PQgetvalue(res, i, 0));
            strncpy(t->rexp, PQgetvalue(res, i, 1), 127);
            strncpy(t->name, PQgetvalue(res, i, 2), 63);
            strncpy(t->contents, PQgetvalue(res, i, 3), 511);
            lamb_list_rpush(templates, lamb_node_new(t));
        }
    }

    PQclear(res);
    return 0;
}
