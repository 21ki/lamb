
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include <stdlib.h>
#include <string.h>
#include "gateway.h"
#include "queue.h"

int lamb_gateway_get(lamb_db_t *db, int id, lamb_gateway_t *gateway) {
    char *column;
    char sql[256];
    PGresult *res = NULL;

    column = "id, type, host, port, username, password, spid, spcode, encoded, concurrent";
    sprintf(sql, "SELECT %s FROM channel WHERE id = %d", column, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    if (PQntuples(res) < 1) {
        return 2;
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
    sprintf(sql, "SELECT %s FROM channel ORDER BY id", column);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 1;
    }

    rows = PQntuples(res);
    if (rows < 1) {
        return 2;
    }

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

int lamb_gateway_queue_open(lamb_gateway_queues_t *queue, int qlen, lamb_gateway_t *gateways[], int glen, lamb_queue_opt *opt, int type) {
    int err;
    char name[128];

    memset(name, 0, sizeof(name));

    for (int i = 0, j = 0; (i < glen) && (i < qlen) && (gateways[i] != NULL); i++, j++) {
        lamb_gateway_queue_t *q = NULL;
        q = malloc(sizeof(lamb_gateway_queue_t));
        if (q != NULL) {
            memset(q, 0, sizeof(lamb_gateway_queue_t));
            q->id = gateways[i]->id;

            /* Open send queue */
            if (type == LAMB_QUEUE_SEND) {
                sprintf(name, "/gw.%d.message", gateways[i]->id);
            } else {
                sprintf(name, "/gw.%d.deliver", gateways[i]->id);
            }

            err = lamb_queue_open(&(q->send), name, opt);
            if (err) {
                j--;
                free(q);
                continue;
            }
            queue->list[j] = q;
            queue->len++;
        } else {
            j--;
        }
    }

    return 0;
}

int lamb_gateway_epoll_add(int epfd, struct epoll_event *event, lamb_gateway_queue_t *queues[], size_t len, int type) {
    int err;

    for (int i = 0; i < len && (queues[i] != NULL); i++) {
        switch (type) {
        case LAMB_QUEUE_SEND:
            event->data.fd = queues[i]->send.mqd;
            event->events = EPOLLOUT;
            err = epoll_ctl(epfd, EPOLL_CTL_ADD, queues[i]->send.mqd, event);
            if (err == -1) {
                return 1;
            }
            break;
        case LAMB_QUEUE_RECV:
            event->data.fd = queues[i]->recv.mqd;
            event->events = EPOLLIN;
            err = epoll_ctl(epfd, EPOLL_CTL_ADD, queues[i]->recv.mqd, event);
            if (err == -1) {
                return 1;
            }
            break;
        }
    }

    return 0;
}
