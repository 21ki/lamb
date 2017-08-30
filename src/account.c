
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
#include "db.h"
#include "queue.h"

int lamb_account_get(lamb_db_t *db, char *username, lamb_account_t *account) {
    char sql[512];
    char *column = NULL;
    PGresult *res = NULL;

    column = "id, username, spcode, company, charge_type, ip_addr, concurrent, route, extended, policy, check_template, check_keyword";
    sprintf(sql, "SELECT %s FROM account WHERE username = '%s'", column, username);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    if (PQntuples(res) < 1) {
        return 2;
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

int lamb_account_get_all(lamb_db_t *db, lamb_account_t *accounts[], size_t size) {
    int rows = 0;
    char *sql = NULL;
    PGresult *res = NULL;
    
    sql = "SELECT id, username, spcode, company, charge_type, ip_addr, concurrent, route, extended, policy, check_template, check_keyword FROM account ORDER BY id";
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

int lamb_account_queue_open(lamb_account_queue_t *queues[], size_t qlen, lamb_account_t *accounts[], size_t alen, lamb_queue_opt *ropt, lamb_queue_opt *sopt) {
    int err;
    char name[128];

    memset(name, 0, sizeof(name));

    for (int i = 0, j = 0; (i < alen) && (i < qlen) && (accounts[i] != NULL); i++, j++) {
        lamb_account_queue_t *q = NULL;
        q = malloc(sizeof(lamb_account_queue_t));
        if (q != NULL) {
            memset(q, 0, sizeof(lamb_account_queue_t));
            q->id = accounts[i]->id;

            /* Open send queue */
            sprintf(name, "/cli.%d.message", accounts[i]->id);
            err = lamb_queue_open(&(q->send), name, ropt);
            if (err) {
                j--;
                free(q);
                continue;
            }

            /* Open recv queue */
            sprintf(name, "/cli.%d.deliver", accounts[i]->id);
            err = lamb_queue_open(&(q->recv), name, sopt);

            if (err) {
                j--;
                free(q);
                continue;
            }

            queues[j] = q;
        } else {
            j--;
        }
    }

    return 0;
}

int lamb_account_epoll_add(int epfd, struct epoll_event *event, lamb_account_queue_t *queues[], size_t len, int type) {
    int err;

    for (int i = 0; i < len && (queues[i] != NULL); i++) {
        switch (type) {
        case LAMB_QUEUE_SEND:
            event->data.fd = queues[i]->send.mqd;
            event->events = EPOLLIN;
            err = epoll_ctl(epfd, EPOLL_CTL_ADD, queues[i]->send.mqd, event);
            if (err == -1) {
                return 1;
            }
            break;
        case LAMB_QUEUE_RECV:
            event->data.fd = queues[i]->recv.mqd;
            event->events = EPOLLOUT;
            err = epoll_ctl(epfd, EPOLL_CTL_ADD, queues[i]->recv.mqd, event);
            if (err == -1) {
                return 1;
            }
            break;
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

