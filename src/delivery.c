
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <syslog.h>
#include "common.h"
#include "config.h"
#include "cache.h"
#include "socket.h"
#include "message.h"
#include "delivery.h"
#include "log.h"

//static int ac;
static lamb_db_t db;
static lamb_db_t mdb;
static lamb_list_t *pool;
static lamb_cache_t *rdb;
static lamb_config_t config;
static lamb_list_t *storage;
static lamb_list_t *delivery;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static Response resp = RESPONSE__INIT;
static volatile bool sleeping = false;

int main(int argc, char *argv[]) {
    char *file = "deliver.conf";
    bool background = false;

    int opt = 0;
    char *optstring = "c:d";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'c':
            file = optarg;
            break;
        case 'd':
            background = true;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    /* Read lamb configuration file */
    if (lamb_read_config(&config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (background) {
        lamb_daemon();
    }

    /* Logger initialization*/
    lamb_log_init("lamb-delivery");

    /* Check lock protection */
    lamb_lock_t lock;

    if (lamb_lock_protection(&lock, "/tmp/delivery.lock")) {
        syslog(LOG_ERR, "Already started, please do not repeat the start!\n");
        return -1;
    }

    /* Save pid to file */
    lamb_pid_file(&lock, getpid());

    /* Signal event processing */
    lamb_signal_processing();

    /* Setting process information */
    lamb_set_process("lamb-delivery");

    /* Start Main Event Thread */
    lamb_event_loop();

    /* Release lock protection */
    lamb_lock_release(&lock);

    return 0;
}

void lamb_event_loop(void) {
    int fd;
    int err;
    char addr[128];

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);

    err = lamb_signal(SIGHUP, lamb_reload);
    lamb_debug("lamb signal initialization %s\n", err ? "failed" : "successfull");

    /* Client Queue Pools Initialization */
    pool = lamb_list_new();
    if (!pool) {
        syslog(LOG_ERR, "queue pool initialization failed");
        return;
    }

    pool->match = lamb_queue_compare;

    /* Storage queue initialization */
    storage = lamb_list_new();
    if (!storage) {
        syslog(LOG_ERR, "storage queue initialization failed");
        return;
    }

    /* delivery queue initialization */
    delivery = lamb_list_new();
    if (!delivery) {
        syslog(LOG_ERR, "delivery routing table initialization failed");
        return;
    }

    /* redis cache initialization */
    rdb = (lamb_cache_t *)malloc(sizeof(lamb_cache_t));

    if (rdb) {
        err = lamb_cache_connect(rdb, config.redis_host, config.redis_port, NULL,
                                 config.redis_db);
        if (err) {
            syslog(LOG_ERR, "can't connect to redis %s", config.redis_host);
            return;
        }
    }

    /* Core database initialization */
    err = lamb_db_init(&db);
    if (err) {
        syslog(LOG_ERR, "postgresql database initialization failed");
        return;
    }

    err = lamb_db_connect(&db, config.db_host, config.db_port, config.db_user,
                          config.db_password, config.db_name);
    if (err) {
        syslog(LOG_ERR, "can't connect to postgresql database");
        return;
    }

    /* Message database initialization */
    err = lamb_db_init(&mdb);
    if (err) {
        syslog(LOG_ERR, "postgresql database initialization failed");
        return;
    }

    err = lamb_db_connect(&mdb, config.msg_host, config.msg_port, config.msg_user,
                          config.msg_password, config.msg_name);
    if (err) {
        syslog(LOG_ERR, "can't connect to postgresql database %s", config.db_host);
        return;
    }

    /* fetch delivery routing */
    err = lamb_get_delivery(&db, delivery);
    if (err) {
        syslog(LOG_ERR, "fetch delivery routing failed");
        return;
    }

#ifdef _DEBUG

    lamb_node_t *node;
    lamb_delivery_t *dd;
    lamb_list_iterator_t *it = lamb_list_iterator_new(delivery, LIST_HEAD);

    while ((node = lamb_list_iterator_next(it))) {
        dd = (lamb_delivery_t *)node->val;
        printf("-> id: %d, rexp: %s, target: %d\n", dd->id, dd->rexp, dd->target);
    }

    lamb_list_iterator_destroy(it);

#endif

    /* delivery server Initialization */
    fd = nn_socket(AF_SP, NN_REP);
    if (fd < 0) {
        syslog(LOG_ERR, "socket %s", nn_strerror(nn_errno()));
        return;
    }

    snprintf(addr, sizeof(addr), "tcp://%s:%d", config.listen, config.port);
        
    if (nn_bind(fd, addr) < 0) {
        nn_close(fd);
        syslog(LOG_ERR, "bind %s", nn_strerror(nn_errno()));
        return;
    }

    /* Start storage processing thread */
    lamb_start_thread(lamb_store_loop, NULL, 1);

    /* Start Data Acquisition Thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);

    int rc, len;
    Request *req;
    char *buf = NULL;

    while (true) {
        rc = nn_recv(fd, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }
            continue;
        }

        if (CHECK_COMMAND(buf) != LAMB_REQUEST) {
            nn_freemsg(buf);
            syslog(LOG_WARNING, "invalid command %#x request from client", CHECK_COMMAND(buf));
            continue;
        }

        req = request__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
        nn_freemsg(buf);

        if (!req) {
            syslog(LOG_ERR, "can't parse protobuff protocol packets");
            continue;
        }

        if (req->id < 1) {
            syslog(LOG_WARNING, "Incorrect client identity id number");
            continue;
        }

        struct timeval now;
        struct timespec timeout;
        
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = now.tv_usec * 1000;
        timeout.tv_sec += 3;

        pthread_mutex_lock(&mutex);

        if (req->type == LAMB_PULL) {
            lamb_start_thread(lamb_pull_loop,  (void *)req, 1);
        } else if (req->type == LAMB_PUSH) {
            lamb_start_thread(lamb_push_loop,  (void *)req, 1);
        } else {
            request__free_unpacked(req, NULL);
            pthread_mutex_unlock(&mutex);
            continue;
        }

        err = pthread_cond_timedwait(&cond, &mutex, &timeout);

        if (err != ETIMEDOUT) {
            void *pk;
            len = response__get_packed_size(&resp);
            pk = malloc(len);

            if (pk) {
                response__pack(&resp, pk);
                len = lamb_pack_assembly(&buf, LAMB_RESPONSE, pk, len);
                if (len > 0) {
                    nn_send(fd, buf, len, 0);
                    free(buf);
                }
                free(pk);
            }
        }

        pthread_mutex_unlock(&mutex);
    }
}

void lamb_reload(int signum) {
    int err;
    lamb_node_t *node;
    lamb_list_iterator_t *it;

    if (signal(SIGHUP, lamb_reload) == SIG_ERR) {
        syslog(LOG_WARNING, "signal setting process failed");
    }

    sleeping = true;
    lamb_sleep(3000);

    syslog(LOG_INFO, "Start heavy load configuration ...");

    delivery->free = free;
    it = lamb_list_iterator_new(delivery, LIST_TAIL);

    /* clean delivery information */
    while ((node = lamb_list_iterator_next(it))) {
        lamb_list_remove(delivery, node);
    }

    lamb_list_iterator_destroy(it);

    /* fetch delivery routing */
    err = lamb_get_delivery(&db, delivery);

    if (err) {
        syslog(LOG_ERR, "fetch delivery information failed");
    } else {
        syslog(LOG_ERR, "fetch delivery information successfull");
    }

    sleeping = false;
    syslog(LOG_INFO, "The reload configuration completion");
    lamb_debug("The reload configuration completion");

    return;
}

void *lamb_push_loop(void *arg) {
    int err;
    int fd, rc;
    int timeout;
    char host[128];
    Request *client;
    
    client = (Request *)arg;

    syslog(LOG_INFO, "new client from %s connectd\n", client->addr);

    /* Client channel initialization */
    unsigned short port = config.port + 1;
    err = lamb_child_server(&fd, config.listen, &port, NN_PAIR);
    if (err) {
        pthread_cond_signal(&cond);
        request__free_unpacked(client, NULL);
        syslog(LOG_ERR, "There are no ports available for the operating system");
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex);

    memset(host, 0, sizeof(host));
    resp.id = client->id;
    snprintf(host, sizeof(host), "tcp://%s:%d", config.listen, port);
    resp.host = host;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);

    timeout = config.timeout;
    nn_setsockopt(fd, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(timeout));

    /* Start event processing */
    int account;
    char *buf = NULL;
    lamb_node_t *node;
    lamb_queue_t *queue;
    lamb_report_t *report;
    lamb_deliver_t *deliver;

    while (true) {
        if (sleeping) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(fd, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }

            if (nn_errno() == ETIMEDOUT) {
                if (!nn_get_statistic(fd, NN_STAT_CURRENT_CONNECTIONS)) {
                    break;
                }
            }

            continue;
        }

        /* Report */
        if (CHECK_COMMAND(buf) == LAMB_REPORT) {
            Report *r = report__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!r) {
                lamb_debug("can't unpack report message packet\n");
                continue;
            }

            node = lamb_list_find(pool, (void *)(intptr_t)r->account);

            if (node) {
                queue = (lamb_queue_t *)node->val;
            } else {
                queue = lamb_queue_new(r->account);
                if (queue) {
                    lamb_list_rpush(pool, lamb_node_new(queue));
                }
            }

            if (queue) {
                report = (lamb_report_t *)calloc(1, sizeof(lamb_report_t));
                if (report) {
                    report->type = LAMB_REPORT;
                    report->id = r->id;
                    report->account = r->account;
                    report->company = r->company;
                    strncpy(report->spcode, r->spcode, 20);
                    strncpy(report->phone, r->phone, 11);
                    report->status = r->status;
                    strncpy(report->submittime, r->submittime, 10);
                    strncpy(report->donetime, r->donetime, 10);
                    lamb_queue_push(queue, report);
                }

            }

            report__free_unpacked(r, NULL);
            continue;
        }

        /* Delivery */
        if (CHECK_COMMAND(buf) == LAMB_DELIVER) {
            Deliver *d = deliver__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!d) {
                continue;
            }

            account = 0;

            deliver = (lamb_deliver_t *)calloc(1, sizeof(lamb_deliver_t));

            if (!deliver) {
                continue;
            }

            deliver->type = LAMB_DELIVER;
            deliver->id = d->id;
            deliver->account = account;
            strncpy(deliver->phone, d->phone, 11);
            strncpy(deliver->spcode, d->spcode, 20);
            deliver->msgfmt = d->msgfmt;
            deliver->length = d->length;
            memcpy(deliver->content, d->content.data, d->content.len);
            
            lamb_delivery_t *dt;
            lamb_list_iterator_t *it = lamb_list_iterator_new(delivery, LIST_HEAD);

            while ((node = lamb_list_iterator_next(it))) {
                dt = (lamb_delivery_t *)node->val;
                if (lamb_check_delivery(dt, deliver->spcode, strlen(d->spcode))) {
                    account = dt->target;
                    break;
                }
            }

            lamb_list_iterator_destroy(it);

            if (account > 0) {
                node = lamb_list_find(pool, (void *)(intptr_t)account);
                if (node) {
                    queue = (lamb_queue_t *)node->val;
                } else {
                    queue = lamb_queue_new(account);
                    if (queue) {
                        lamb_list_rpush(pool, lamb_node_new(queue));
                    }
                }
                
                if (queue) {
                    lamb_queue_push(queue, deliver);
                }
            } else {
                deliver->account = 0;
                deliver->company = 0;
                lamb_list_rpush(storage, lamb_node_new(deliver));
            }

            deliver__free_unpacked(d, NULL);
            continue;
        }

        /* Close */
        if (CHECK_COMMAND(buf) == LAMB_BYE) {
            nn_freemsg(buf);
            break;
        }

        lamb_debug("receive a invalid command\n");
        nn_freemsg(buf);
    }

    nn_close(fd);
    lamb_debug("connection closed from %s\n", client->addr);
    syslog(LOG_INFO, "connection closed from %s", client->addr);
    request__free_unpacked(client, NULL);

    pthread_exit(NULL);
}

void *lamb_pull_loop(void *arg) {
    int fd;
    int err;
    int timeout;
    char host[128];
    Request *client;
    lamb_node_t *node;
    lamb_queue_t *queue;
    
    client = (Request *)arg;

    syslog(LOG_INFO, "new client from %s connectd\n", client->addr);

    /* client queue initialization */
    node = lamb_list_find(pool, (void *)(intptr_t)client->id);

    if (node) {
        queue = (lamb_queue_t *)node->val;
    } else {
        queue = lamb_queue_new(client->id);
        if (queue) {
            lamb_list_rpush(pool, lamb_node_new(queue));
        }
    }

    node = NULL;

    if (!queue) {
        syslog(LOG_ERR, "can't create queue for client %s", client->addr);
        request__free_unpacked(client, NULL);
        pthread_exit(NULL);
    }

    /* Client channel initialization */
    unsigned short port = config.port + 1;
    err = lamb_child_server(&fd, config.listen, &port, NN_REP);
    if (err) {
        pthread_cond_signal(&cond);
        request__free_unpacked(client, NULL);
        syslog(LOG_ERR, "There are no ports available for the operating system");
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex);

    memset(host, 0, sizeof(host));
    resp.id = client->id;
    snprintf(host, sizeof(host), "tcp://%s:%d", config.listen, port);
    resp.host = host;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);

    timeout = config.timeout;
    nn_setsockopt(fd, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(timeout));
    
    /* Start event processing */
    int rc, len;
    void *pk;
    void *message;
    char *buf = NULL;
    Report report = REPORT__INIT;
    Deliver deliver = DELIVER__INIT;

    while (true) {
        rc = nn_recv(fd, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }

            if (nn_errno() == ETIMEDOUT) {
                if (!nn_get_statistic(fd, NN_STAT_CURRENT_CONNECTIONS)) {
                    break;
                }
            }

            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_REQ) {
            nn_freemsg(buf);
            node = lamb_queue_pop(queue);

            if (!node) {
                len = lamb_pack_assembly(&buf, LAMB_EMPTY, NULL, 0);
                if (len) {
                    nn_send(fd, buf, len, NN_DONTWAIT);
                    free(buf);
                }
                continue;
            }

            message = node->val;

            if (CHECK_TYPE(message) == LAMB_REPORT) {
                lamb_report_t *r = (lamb_report_t *)message;
                report.id = r->id;
                report.account = r->account;
                report.company = r->company;
                report.spcode = r->spcode;
                report.phone = r->phone;
                report.status = r->status;
                report.submittime = r->submittime;
                report.donetime = r->donetime;

                len = report__get_packed_size(&report);
                pk = malloc(len);

                if (pk) {
                    report__pack(&report, pk);
                    len = lamb_pack_assembly(&buf, LAMB_REPORT, pk, len);
                    if (len > 0) {
                        nn_send(fd, buf, len, NN_DONTWAIT);
                        free(buf);
                    }
                    free(pk);
                }

            } else if (CHECK_TYPE(message) == LAMB_DELIVER) {
                lamb_deliver_t *d = (lamb_deliver_t *)message;
                deliver.id = d->id;
                deliver.account = d->account;
                deliver.company = d->company;
                deliver.phone = d->phone;
                deliver.spcode = d->spcode;
                deliver.serviceid = d->serviceid;
                deliver.msgfmt = d->msgfmt;
                deliver.length = d->length;
                deliver.content.len = d->length;
                deliver.content.data = (uint8_t *)d->content;

                len = deliver__get_packed_size(&deliver);
                pk = malloc(len);

                if (pk) {
                    deliver__pack(&deliver, pk);
                    len = lamb_pack_assembly(&buf, LAMB_DELIVER, pk, len);
                    if (len > 0) {
                        nn_send(fd, buf, len, NN_DONTWAIT);
                        free(buf);
                    }
                    free(pk);
                }
            }

            free(node);
            free(message);
            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_BYE) {
            nn_freemsg(buf);
            break;
        }

        nn_freemsg(buf);
    }

    nn_close(fd);
    lamb_debug("connection closed from %s\n", client->addr);
    syslog(LOG_ERR, "connection closed from %s", client->addr);
    request__free_unpacked(client, NULL);

    pthread_exit(NULL);
}

int lamb_child_server(int *sock, const char *listen, unsigned short *port, int protocol) {
    int fd;
    char addr[128];
    
    fd = nn_socket(AF_SP, protocol);
    if (fd < 0) {
        syslog(LOG_ERR, "socket %s", nn_strerror(nn_errno()));
        return -1;
    }

    while (true) {
        snprintf(addr, sizeof(addr), "tcp://%s:%u", listen, *port);

        if (nn_bind(fd, addr) > 0) {
            break;
        }

        (*port)++;
    }

    *sock = fd;
    
    return 0;
}

void *lamb_store_loop(void *data) {
    void *message;
    lamb_node_t *node;

    while (true) {
        node = lamb_list_lpop(storage);

        if (!node) {
            lamb_sleep(10);
            continue;
        }

        message = node->val;

        if (CHECK_TYPE(message) == LAMB_DELIVER) {
            
            lamb_write_deliver(&mdb, (lamb_deliver_t *)message);
        }

        free(node);
        free(message);
    }

    pthread_exit(NULL);
}

void *lamb_stat_loop(void *arg) {
    int signal;

    while (true) {
#ifdef _DEBUG
        lamb_node_t *node;
        lamb_queue_t *queue;
        long long length = 0;
        lamb_list_iterator_t *it;

        it = lamb_list_iterator_new(pool, LIST_HEAD);

        while ((node = lamb_list_iterator_next(it))) {
            queue = (lamb_queue_t *)node->val;
            length += queue->list->len;
            lamb_debug("queue: %d, len: %u\n", queue->id, queue->list->len);
        }

        lamb_list_iterator_destroy(it);
#endif

        signal = lamb_check_signal(rdb, config.id);
        if (!sleeping && signal == 1) {
            lamb_reload(SIGHUP);
        }

        lamb_sleep(3000);
    }

    pthread_exit(NULL);
}

bool lamb_check_delivery(lamb_delivery_t *d, char *spcode, size_t len) {
    if (lamb_pcre_regular(d->rexp, spcode, len)) {
        return true;
    }

    return false;
}

int lamb_get_delivery(lamb_db_t *db, lamb_list_t *deliverys) {
    int rows;
    char sql[128];
    PGresult *res = NULL;

    snprintf(sql, sizeof(sql), "SELECT id, rexp, target FROM delivery");
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return 0;
    }

    rows = PQntuples(res);
    
    for (int i = 0; i < rows; i++) {
        lamb_delivery_t *delivery;
        delivery = (lamb_delivery_t *)calloc(1, sizeof(lamb_delivery_t));
        if (delivery) {
            delivery->id = atoi(PQgetvalue(res, i, 0));
            strncpy(delivery->rexp, PQgetvalue(res, i, 1), 127);
            delivery->target = atoi(PQgetvalue(res, i, 2));
            lamb_list_rpush(deliverys, lamb_node_new(delivery));
        }
    }

    PQclear(res);

    return 0;
}

int lamb_write_deliver(lamb_db_t *db, lamb_deliver_t *message) {
    int err;
    char *column;
    char sql[512];
    char *fromcode;
    char content[512];
    PGresult *res = NULL;

    if (!message) {
        return -1;
    }

    switch (message->msgfmt) {
    case 0:
        fromcode = "ASCII";
        break;
    case 8:
        fromcode = "UCS-2BE";
        break;
    case 11:
        fromcode = NULL;
        break;
    case 15:
        fromcode = "GBK";
        break;
    default:
        return -1;
    }

    if (fromcode != NULL) {
        memset(content, 0, sizeof(content));
        err = lamb_encoded_convert(message->content, message->length, content, sizeof(content),
                                   fromcode, "UTF-8", &message->length);
        if (err || (message->length < 1)) {
            return -1;
        }
    }

    column = "id, spcode, phone, content, account, company";
    snprintf(sql, sizeof(sql), "INSERT INTO delivery(%s) VALUES(%lld, '%s', '%s', '%s', %d, %d)",
             column, (long long int)message->id, message->spcode,
             message->phone, fromcode ? content : message->content,
             message->account, message->company);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);

    return 0;
}

int lamb_check_signal(lamb_cache_t *rdb, int id) {
    int signal = 0;
    redisReply *reply = NULL;

    reply = redisCommand(rdb->handle, "HGET delivery.%d signal", id);

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING && reply->len > 0) {
            signal = atoi(reply->str);
        }
        freeReplyObject(reply);
    }

    /* Reset signal state */
    lamb_clear_signal(rdb, id);

    return signal;
}

void lamb_clear_signal(lamb_cache_t *rdb, int id) {
    redisReply *reply = NULL;

    reply = redisCommand(rdb->handle, "HSET delivery.%d signal 0", id);

    if (reply != NULL) {
        freeReplyObject(reply);
    }

    return;
}

int lamb_read_config(lamb_config_t *conf, const char *file) {
    if (!conf) {
        return -1;
    }

    config_t cfg;
    if (lamb_read_file(&cfg, file) != 0) {
        fprintf(stderr, "Can't open the %s configuration file\n", file);
        goto error;
    }

    if (lamb_get_int(&cfg, "Id", &conf->id) != 0) {
        fprintf(stderr, "Can't read config 'Id' parameter\n");
        goto error;
    }

    if (lamb_get_bool(&cfg, "Debug", &conf->debug) != 0) {
        fprintf(stderr, "Can't read config 'Debug' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Listen", conf->listen, 16) != 0) {
        fprintf(stderr, "Invalid Listen IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Port", &conf->port) != 0) {
        fprintf(stderr, "Can't read config 'Port' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "Can't read config 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Ac", conf->ac, 128) != 0) {
        fprintf(stderr, "Can't read config 'Ac' parameter\n");
    }

    if (lamb_get_string(&cfg, "RedisHost", conf->redis_host, 16) != 0) {
        fprintf(stderr, "Can't read config 'RedisHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisPort", &conf->redis_port) != 0) {
        fprintf(stderr, "Can't read config 'RedisPort' parameter\n");
        goto error;
    }

    if (conf->redis_port < 1 || conf->redis_port > 65535) {
        fprintf(stderr, "Invalid redis port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "RedisPassword", conf->redis_password, 64) != 0) {
        fprintf(stderr, "Can't read config 'RedisPassword' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "Can't read config 'RedisDb' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "Can't read config 'DbHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DbPort", &conf->db_port) != 0) {
        fprintf(stderr, "Can't read config 'DbPort' parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "Invalid database port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbPassword", conf->db_password, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "DbName", conf->db_name, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbName' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgHost", conf->msg_host, 16) != 0) {
        fprintf(stderr, "Can't read config 'MsgHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MsgPort", &conf->msg_port) != 0) {
        fprintf(stderr, "Can't read config 'MsgPort' parameter\n");
        goto error;
    }

    if (conf->msg_port < 1 || conf->msg_port > 65535) {
        fprintf(stderr, "Invalid 'MsgPort' port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgUser", conf->msg_user, 64) != 0) {
        fprintf(stderr, "Can't read config 'MsgUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgPassword", conf->msg_password, 64) != 0) {
        fprintf(stderr, "Can't read config 'MsgPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "MsgName", conf->msg_name, 64) != 0) {
        fprintf(stderr, "Can't read config 'MsgName' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "Can't read config 'LogFile' parameter\n");
        goto error;
    }

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}

