
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include "account.h"
#include "db.h"

int lamb_account_get(lamb_cache_t *cache, char *user, lamb_account_t *account) {
    if (!cache || !cache->handle || !account) {
        return -1;
    }

    int r = 0;
    redisReply *reply = NULL;
    const char *cmd = "HMGET account.%s company charge_type ip_addr route spcode concurrent extended policy check_template check_keyword";
    reply = redisCommand(cache->handle, cmd, user);

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
                case 1:
                    account->charge_type = atoi(reply->element[1]->str);
                    break;
                case 2:
                    if (reply->element[2]->len > 15) {
                        memcpy(account->ip_addr, reply->element[2]->str, 15);
                    } else {
                        memcpy(account->ip_addr, reply->element[2]->str, reply->element[2]->len);
                    }
                    break;
                case 3:
                    account->route = atoi(reply->element[3]->str);
                    break;
                case 4:
                    if (reply->element[4]->len > 23) {
                        memcpy(account->spcode, reply->element[4]->str, 23);
                    } else {
                        memcpy(account->spcode, reply->element[4]->str, reply->element[4]->len);
                    }
                    break;
                case 5:
                    account->concurrent = atoi(reply->element[5]->str);
                    break;
                case 6:
                    account->extended = (atoi(reply->element[6]->str) == 0) ? false : true;
                    break;
                case 7:
                    account->policy = atoi(reply->element[7]->str);
                    break;
                case 8:
                    account->check_template = (atoi(reply->element[8]->str) == 0) ? false : true;
                    break;
                case 9:
                    account->check_keyword = (atoi(reply->element[9]->str) == 0) ? false : true;
                    break;
                }
            }
        }

        freeReplyObject(reply);
    }

    return r;
}

int lamb_account_get_all(lamb_db_t *db, lamb_account_t *accounts[], size_t size) {
    int rows = 0;
    char *sql = NULL;
    PGresult *res = NULL;
    
    sql = "SELECT id, username, spcode, company, charge_type, ip_addr, concurrent, route, extended, policy, check_template, check_keyword FROM account ORDER BY id";
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    rows = PQntuples(res);
    if (rows < 1) {
        return 2;
    }

    for (int i = 0; (i < rows) && (i < size); i++) {
        lamb_account_t *a= NULL;
        a = (lamb_account_t *)malloc(sizeof(lamb_account_t));
        if (a != NULL) {
            memset(a, 0, sizeof(lamb_account_t));
            a->id = atoi(PQgetvalue(res, i, 0));
            strncpy(a->user, PQgetvalue(res, i, 1), 7);
            strncpy(a->spcode, PQgetvalue(res, i, 2), 20);
            a->company = atoi(PQgetvalue(res, i, 3));
            a->charge_type = atoi(PQgetvalue(res, i, 4));
            strncpy(a->ip_addr, PQgetvalue(res, i, 5), 15);
            a->concurrent = atoi(PQgetvalue(res, i, 6));
            a->route = atoi(PQgetvalue(res, i, 7));
            a->extended = atoi(PQgetvalue(res, i, 8)) == 0 ? false : true;
            a->policy = atoi(PQgetvalue(res, i, 9));
            a->check_template = atoi(PQgetvalue(res, i, 10)) == 0 ? false : true;
            a->check_keyword = atoi(PQgetvalue(res, i, 11)) == 0 ? false : true;
            accounts[i] = a;
        }
    }

    PQclear(res);
    return 0;
}
