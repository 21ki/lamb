
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "gateway.h"

int lamb_gateway_get(lamb_db_t *db, int id, lamb_gateway_t *gateway) {
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, type, host, port, username, password, spid, spcode, encoded, concurrent";
    sprintf(sql, "SELECT %s FROM gateway WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return 1;
    }

    gateway->id = atoi(PQgetvalue(res, 0, 0));
    gateway->type = atoi(PQgetvalue(res, 0, 1));
    strncpy(gateway->host, PQgetvalue(res, 0, 2), 15);
    gateway->port = atoi(PQgetvalue(res, 0, 3));
    strncpy(gateway->username, PQgetvalue(res, 0, 4), 31);
    strncpy(gateway->password, PQgetvalue(res, 0, 5), 63);
    strncpy(gateway->spid, PQgetvalue(res, 0, 6), 31);
    strncpy(gateway->spcode, PQgetvalue(res, 0, 7), 20);
    gateway->encoded = atoi(PQgetvalue(res, 0, 8));
    gateway->concurrent = atoi(PQgetvalue(res, 0, 9));
    
    PQclear(res);

    return 0;
}

int lamb_gateway_get_all(lamb_db_t *db, lamb_gateways_t *gateways, int size) {
    int rows = 0;
    char *column;
    char sql[256];
    PGresult *res = NULL;

    gateways->len = 0;
    column = "id, type, host, port, username, password, spid, spcode, encoded, concurrent";
    sprintf(sql, "SELECT %s FROM gateway ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0, j = 0; (i < rows) && (i < size); i++, j++) {
        lamb_gateway_t *g = NULL;
        g = (lamb_gateway_t *)calloc(1, sizeof(lamb_gateway_t));
        if (g != NULL) {
            g->id = atoi(PQgetvalue(res, i, 0));
            g->type = atoi(PQgetvalue(res, i, 1));
            strncpy(g->host, PQgetvalue(res, i, 2), 15);
            g->port = atoi(PQgetvalue(res, i, 3));
            strncpy(g->username, PQgetvalue(res, i, 4), 31);
            strncpy(g->password, PQgetvalue(res, i, 5), 63);
            strncpy(g->spid, PQgetvalue(res, i, 6), 31);
            strncpy(g->spcode, PQgetvalue(res, i, 7), 20);
            g->encoded = atoi(PQgetvalue(res, i, 8));
            g->concurrent = atoi(PQgetvalue(res, i, 9));
            gateways->list[j] = g;
            gateways->len++;
        } else {
            if (j > 0) {
                j--;
            }
        }
    }

    PQclear(res);
    return 0;
}

int lamb_gateway_queue_open(lamb_gateway_queues_t *queues, int qlen, lamb_gateways_t *gateways, int glen, lamb_mq_opt *opt, int type) {
    int err;
    char name[128];

    queues->len = 0;
    memset(name, 0, sizeof(name));

    for (int i = 0, j = 0; (i < gateways->len) && (i < glen) && (i < qlen) && (gateways->list[i] != NULL); i++, j++) {
        lamb_gateway_queue_t *q = NULL;
        q = (lamb_gateway_queue_t *)calloc(1, sizeof(lamb_gateway_queue_t));
        if (q != NULL) {
            /* Open send queue */
            if (type == LAMB_QUEUE_SEND) {
                sprintf(name, "/gw.%d.message", gateways->list[i]->id);
            } else {
                sprintf(name, "/gw.%d.deliver", gateways->list[i]->id);
            }

            err = lamb_mq_open(&(q->queue), name, opt);
            if (err) {
                j--;
                free(q);
                continue;
            }

            q->id = gateways->list[i]->id;
            queues->list[j] = q;
            queues->len++;
        } else {
            j--;
        }
    }

    return 0;
}

int lamb_gateway_epoll_add(int epfd, struct epoll_event *event, lamb_gateway_queues_t *queues, int len, int type) {
    int err;

    for (int i = 0; i < len && (queues->list[i] != NULL); i++) {
        switch (type) {
        case LAMB_QUEUE_SEND:
            event->events = EPOLLOUT;
            break;
        case LAMB_QUEUE_RECV:
            event->events = EPOLLIN;
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
