
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include "account.h"

int lamb_account_get(lamb_cache_t *cache, char *username, lamb_account_t *account) {
    const char *cmd;
    redisReply *reply = NULL;

    cmd = "HMGET account.%s id username password spcode company charge address concurrent dbase template keyword";
    reply = redisCommand(cache->handle, cmd, username);

    if (reply == NULL ) {
        return -1;
    }

    if ((reply->type != REDIS_REPLY_ARRAY) || (reply->elements != 11)) {
        freeReplyObject(reply);
        return -1;
    }

    /* Id */
    if (reply->element[0]->len > 0) {
        account->id = atoi(reply->element[0]->str);
    }

    /* Username */
    if (reply->element[1]->len > 0) {
        if (reply->element[1]->len > 7) {
            memcpy(account->username, reply->element[1]->str, 7);
        } else {
            memcpy(account->username, reply->element[1]->str, reply->element[1]->len);
        }
    }
    
    /* Password */
    if (reply->element[2]->len > 0) {
        if (reply->element[2]->len > 63) {
            memcpy(account->password, reply->element[2]->str, 63);
        } else {
            memcpy(account->password, reply->element[2]->str, reply->element[2]->len);
        }
    }

    /* SpCode */
    if (reply->element[3]->len > 0) {
        if (reply->element[3]->len > 20) {
            memcpy(account->spcode, reply->element[3]->str, 20);
        } else {
            memcpy(account->spcode, reply->element[3]->str, reply->element[3]->len);
        }
    }

    /* Company */
    if (reply->element[4]->len > 0) {
        account->company = atoi(reply->element[4]->str);
    }

    /* Charge Type */
    if (reply->element[5]->len > 0) {
        account->charge = atoi(reply->element[5]->str);
    }

    /* IP Address */
    if (reply->element[6]->len > 0) {
        if (reply->element[6]->len > 15) {
            memcpy(account->address, reply->element[6]->str, 15);
        } else {
            memcpy(account->address, reply->element[6]->str, reply->element[6]->len);
        }
    }

    /* Concurrent */
    if (reply->element[7]->len > 0) {
        account->concurrent = atoi(reply->element[7]->str);
    }

    /* Database */
    if (reply->element[8]->len > 0) {
        account->dbase = atoi(reply->element[8]->str);
    }

    /* Template */
    if (reply->element[9]->len > 0) {
        account->template = (atoi(reply->element[9]->str) == 0) ? false : true;
    }

    /* Keyword */
    if (reply->element[10]->len > 0) {
        account->keyword = (atoi(reply->element[10]->str) == 0) ? false : true;
    }

    freeReplyObject(reply);

    return 0;
}

int lamb_account_fetch(lamb_db_t *db, int id, lamb_account_t *account) {
    char sql[512];
    char *column = NULL;
    PGresult *res = NULL;

    column = "id, username, spcode, company, charge, address, concurrent, dbase, template, keyword";
    sprintf(sql, "SELECT %s FROM account WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return 1;
    }

    account->id = atoi(PQgetvalue(res, 0, 0));
    strncpy(account->username, PQgetvalue(res, 0, 1), 7);
    strncpy(account->spcode, PQgetvalue(res, 0, 2), 20);
    account->company = atoi(PQgetvalue(res, 0, 3));
    account->charge = atoi(PQgetvalue(res, 0, 4));
    strncpy(account->address, PQgetvalue(res, 0, 5), 15);
    account->concurrent = atoi(PQgetvalue(res, 0, 6));
    account->dbase = atoi(PQgetvalue(res, 0, 7));
    account->template = atoi(PQgetvalue(res, 0, 8)) == 0 ? false : true;
    account->keyword = atoi(PQgetvalue(res, 0, 9)) == 0 ? false : true;

    PQclear(res);
    return 0;
}

int lamb_account_get_all(lamb_db_t *db, lamb_accounts_t *accounts, int size) {
    int rows;
    char *column;
    char sql[512];
    PGresult *res = NULL;

    accounts->len = 0;
    column = "id, username, spcode, company, charge, address, concurrent, dbase, template, keyword";
    sprintf(sql, "SELECT %s FROM account ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0, j = 0; (i < rows) && (i < size); i++, j++) {
        lamb_account_t *a = NULL;
        a = (lamb_account_t *)calloc(1, sizeof(lamb_account_t));
        if (a != NULL) {
            a->id = atoi(PQgetvalue(res, i, 0));
            strncpy(a->username, PQgetvalue(res, i, 1), 7);
            strncpy(a->spcode, PQgetvalue(res, i, 2), 20);
            a->company = atoi(PQgetvalue(res, i, 3));
            a->charge = atoi(PQgetvalue(res, i, 4));
            strncpy(a->address, PQgetvalue(res, i, 5), 15);
            a->concurrent = atoi(PQgetvalue(res, i, 6));
            a->dbase = atoi(PQgetvalue(res, i, 7));
            a->template = atoi(PQgetvalue(res, i, 8)) == 0 ? false : true;
            a->keyword = atoi(PQgetvalue(res, i, 9)) == 0 ? false : true;
            accounts->list[j] = a;
            accounts->len++;
        } else {
            if (j > 0) {
                j--;
            }
        }
    }

    PQclear(res);
    return 0;
}
