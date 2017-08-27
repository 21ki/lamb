
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

int lamb_gateway_get_all(lamb_db_t *db, lamb_gateway_t *gateways[], size_t size) {
    int rows = 0;
    char *column;
    char sql[256];
    PGresult *res = NULL;
    
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

    for (int i = 0; (i < rows) && (i < size); i++) {
        lamb_gateway_t *g= NULL;
        g = (lamb_gateway_t *)malloc(sizeof(lamb_gateway_t));
        if (g != NULL) {
            memset(g, 0, sizeof(lamb_gateway_t));
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
            gateways[i] = g;
        }
    }

    PQclear(res);
    return 0;
}

int lamb_gateway_queue_open(lamb_gateway_queue_t *queues[], size_t qlen, lamb_gateway_t *gateways[], size_t glen) {
    int err;
    char name[128];
    lamb_queue_opt opt;

    memset(name, 0, sizeof(name));

    for (int i = 0, j = 0; (i < glen) && (i < qlen) && (gateways[i] != NULL); i++, j++) {
        lamb_gateway_queue_t *q = NULL;
        q = (lamb_gateway_queue_t *)malloc(sizeof(lamb_gateway_queue_t));
        if (q != NULL) {
            memset(q, 0, sizeof(lamb_gateway_queue_t));
            q->id = gateways[i]->id;

            /* Open send queue */
            opt.flag = O_WRONLY | O_NONBLOCK;
            opt.attr = NULL;
            sprintf(name, "/gw.%d.message", gateways[i]->id);
            err = lamb_queue_open(&(q->send), name, &opt);
            if (err) {
                j--;
                free(q);
                continue;
            }

            /* Open recv queue */
            opt.flag = O_RDWR | O_NONBLOCK;
            opt.attr = NULL;
            sprintf(name, "/gw.%d.deliver", gateways[i]->id);
            err = lamb_queue_open(&(q->recv), name, &opt);
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
