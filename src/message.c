
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include "message.h"

int lamb_cache_message(lamb_cache_t *cache, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HMSET %llu spid %s spcode %s phone %s account %d company %d charge %d",
                         message->id, message->spid, message->spcode, message->phone, account->id, company->id, account->charge_type);
    if (reply == NULL) {
        return -1;
    }

    freeReplyObject(reply);

    return 0;
}

int lamb_write_message(lamb_db_t *db, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message) {
    char *column;
    char sql[512];
    PGresult *res = NULL;

    if (message != NULL) {
        column = "id, msgid, spid, spcode, phone, content, status, account, company";
        sprintf(sql, "INSERT INTO message(%s) VALUES(%lld, %u, '%s', '%s', '%s', '%s', %d, %d, %d)", column,
                (long long int)message->id, 0, message->spid, message->spcode, message->phone, message->content, 6, account->id, company->id);
        res = PQexec(db->conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            return -1;
        }

        PQclear(res);
    }

    return 0;
}

