
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include "message.h"

int lamb_write_message(lamb_db_t *db, int account, int company, lamb_submit_t *message) {
    char *column;
    char sql[256];
    PGresult *res = NULL;

    if (message != NULL) {
        column = "id, msgid, spid, spcode, phone, content, status, account, company";
        sprintf(sql, "INSERT INTO message(%s) VALUES(%lld, %u, '%s', '%s', '%s', '%s', %d, %d, %d)", column,
            (long long int)message->id, 0, message->spid, message->spcode, message->phone, message->content, 0, account, company);
        res = PQexec(db->conn, sql);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            return -1;
        }

        PQclear(res);
    }

    return 0;
}

int lamb_update_message(lamb_db_t *db, lamb_update_t *message) {
    char sql[512];
    PGresult *res = NULL;

    if (message != NULL) {
        sprintf(sql, "UPDATE message SET msgid = %lld WHERE id = %lld", message->msgId, message->id);
        res = PQexec(db->conn, sql);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            return -1;
        }

        PQclear(res);
    }

    return 0;
}