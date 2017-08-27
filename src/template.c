
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include "template.h"

int lamb_template_get(lamb_db_t *db, int id, lamb_template_t *template) {
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, name, contents, account";
    sprintf(sql, "SELECT %s FROM template WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    if (PQntuples(res) < 1) {
        return 2;
    }

    template->id = atoi(PQgetvalue(res, 0, 0));
    strncpy(template->name, PQgetvalue(res, 0, 1), 63);
    strncpy(template->contents, PQgetvalue(res, 0, 2), 511);
    template->account = atoi(PQgetvalue(res, 0, 3));
    
    PQclear(res);

    return 0;
}

int lamb_template_get_all(lamb_db_t *db, lamb_template_t *templates[], size_t size) {
    int rows = 0;
    char *column;
    char sql[256];
    PGresult *res = NULL;
    
    column = "id, name, contents, account";
    sprintf(sql, "SELECT %s FROM template ORDER BY id", column);
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
        lamb_template_t *t= NULL;
        t = (lamb_template_t *)malloc(sizeof(lamb_template_t));
        if (t != NULL) {
            memset(t, 0, sizeof(lamb_template_t));
            t->id = atoi(PQgetvalue(res, i, 0));
            strncpy(t->name, PQgetvalue(res, i, 1), 63);
            strncpy(t->contents, PQgetvalue(res, i, 2), 511);
            t->account = atoi(PQgetvalue(res, i, 3));
            templates[i] = t;
        }
    }

    PQclear(res);
    return 0;
}

int lamb_check_template(char *pattern, char *message, int len) {
    int rc;
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[510];

    re = pcre_compile(pattern, 0, &error, &erroffset, NULL);
    if (re == NULL) {
        return -1;
    }

    rc = pcre_exec(re, NULL, message, len, 0, 0, ovector, 510);
    if (rc < 0) {
        pcre_free(re);
        return -1;
    }

    pcre_free(re);
    return 0;
}
