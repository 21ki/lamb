
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

int lamb_account_get(lamb_db_t *db, char *username, lamb_account_t *account) {
    redisReply *reply = NULL;

    redisReply *reply = NULL;
    const char *cmd = "HMGET account.%s id username password spcode company charge_type ip_addr concurrent route extended policy check_template check_keyword";
    reply = redisCommand(cache->handle, cmd, user);

    if (reply == NULL ) {
        return -1;
    }

    if ((reply->type != REDIS_REPLY_ARRAY) || (reply->elements != 13)) {
        goto exit;
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
        account->charge_type = atoi(reply->element[5]->str);
    }

    /* IP Address */
    if (reply->element[6]->len > 0) {
        if (reply->element[6]->len > 15) {
            memcpy(account->ip_addr, reply->element[6]->str, 15);
        } else {
            memcpy(account->ip_addr, reply->element[6]->str, reply->element[6]);
        }
    }

    /* Concurrent */
    if (reply->element[7]->len > 0) {
        account->concurrent = atoi(reply->element[7]->str);
    }

    /* Route */
    if (reply->element[8]->len > 0) {
        account->route = atoi(reply->element[8]->str);    
    }
    
    /* Extended */
    if (reply->element[9]->len > 0) {
        account->extended = (atoi(reply->element[9]->str) == 0) ? false : true;    
    }

    /* Policy */
    if (reply->element[10]->len > 0) {
        account->policy = atoi(reply->element[10]->str);
    }

    /* Template */
    if (reply->element[11]->len > 0) {
        account->check_template = (atoi(reply->element[11]->str) == 0) ? false : true;
    }

    /* Keyword */
    if (reply->element[12]->len > 0) {
        account->check_keyword = (atoi(reply->element[12]->str) == 0) ? false : true;
    }

    freeReplyObject(reply);

    return 0;
}

int lamb_account_fetch(lamb_db_t *db, int id, lamb_account_t *account) {
    char sql[512];
    char *column = NULL;
    PGresult *res = NULL;

    column = "id, username, spcode, company, charge_type, ip_addr, concurrent, route, extended, policy, check_template, check_keyword";
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
    strncpy(account->user, PQgetvalue(res, 0, 1), 7);
    strncpy(account->spcode, PQgetvalue(res, 0, 2), 20);
    account->company = atoi(PQgetvalue(res, 0, 3));
    account->charge_type = atoi(PQgetvalue(res, 0, 4));
    strncpy(account->ip_addr, PQgetvalue(res, 0, 5), 15);
    account->concurrent = atoi(PQgetvalue(res, 0, 6));
    account->route = atoi(PQgetvalue(res, 0, 7));
    account->extended = atoi(PQgetvalue(res, 0, 8)) == 0 ? false : true;
    account->policy = atoi(PQgetvalue(res, 0, 9));
    account->check_template = atoi(PQgetvalue(res, 0, 10)) == 0 ? false : true;
    account->check_keyword = atoi(PQgetvalue(res, 0, 11)) == 0 ? false : true;

    PQclear(res);
    return 0;
}

int lamb_account_get_all(lamb_db_t *db, lamb_accounts_t *accounts, int size) {
    int rows;
    char *column;
    char sql[512];
    PGresult *res = NULL;

    accounts->len = 0;
    column = "id, username, spcode, company, charge_type, ip_addr, concurrent, route, extended, policy, check_template, check_keyword";
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

int lamb_account_queue_open(lamb_account_queues_t *queues, int qlen, lamb_accounts_t *accounts, int alen, lamb_queue_opt *opt, int type) {
    int err;
    char name[128];

    queues->len = 0;
    memset(name, 0, sizeof(name));

    for (int i = 0, j = 0; (i < alen) && (i < qlen) && (accounts->list[i] != NULL); i++, j++) {
        lamb_account_queue_t *q = NULL;
        q = (lamb_account_queue_t *)calloc(1, sizeof(lamb_account_queue_t));
        if (q != NULL) {
            q->id = accounts->list[i]->id;

            /* Open send queue */
            if (type == LAMB_QUEUE_SEND) {
                sprintf(name, "/cli.%d.message", accounts->list[i]->id);
            } else {
                sprintf(name, "/cli.%d.deliver", accounts->list[i]->id);
            }
            
            err = lamb_queue_open(&(q->queue), name, opt);
            if (err) {
                j--;
                free(q);
                continue;
            }

            queues->list[j] = q;
        } else {
            if (j > 0) {
                j--;
            }
        }
    }

    return 0;
}

int lamb_account_epoll_add(int epfd, struct epoll_event *event, lamb_account_queues_t *queues, int len, int type) {
    int err;

    for (int i = 0; (i < queues->len) && (i < len) && (queues->list[i] != NULL); i++) {
        switch (type) {
        case LAMB_QUEUE_SEND:
            event->events = EPOLLIN;
            break;
        case LAMB_QUEUE_RECV:
            event->events = EPOLLOUT;
            break;
        }

        event->data.fd = queues->list[i]->queue.mqd;
        err = epoll_ctl(epfd, EPOLL_CTL_ADD, queues->list[i]->queue.mqd, event);
        if (err == -1) {
            return 1;
        }
    }

    return 0;
}

int lamb_account_spcode_process(char *code, char *spcode, size_t size) {
    size_t len;

    len = strlen(code);
    
    if (len > strlen(spcode)) {
        memcpy(spcode, code, len >= size ? (size - 1) : len);
        return 0;
    }

    for (int i = 0; (i < len) && (i < (size - 1)); i++) {
        spcode[i] = code[i];
    }

    return 0;
}

