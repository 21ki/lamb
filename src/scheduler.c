
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
#include "common.h"
#include "config.h"
#include "cache.h"
#include "socket.h"
#include "queue.h"
#include "message.h"
#include "account.h"
#include "routing.h"
#include "scheduler.h"

//static int ac;
static lamb_db_t db;
static lamb_config_t config;
static lamb_list_t *gateway;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static Response resp = RESPONSE__INIT;

static char *cmcc[] = {"134", "135", "136", "137", "138", "139", "147", "150",
                       "151", "152", "157", "158", "159", "178", "182", "183",
                       "184", "187", "188", "198"};
static char *ctcc[] = {"133", "149", "153", "173", "177", "180", "181", "189", "199"};
static char *cucc[] = {"130", "131", "132", "155", "156", "145", "175", "176", "185",
                       "186", "166"};

int main(int argc, char *argv[]) {
    char *file = "scheduler.conf";
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

    if (setenv("logfile", config.logfile, 1) == -1) {
        fprintf(stderr, "setenv error: %s", strerror(errno));
        return -1;
    }

    /* Signal event processing */
    lamb_signal_processing();

    /* Start Main Event Thread */
    lamb_set_process("lamb-scheduler");
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int fd, err;

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    
    /* Client Queue Pools Initialization */
    gateway = lamb_list_new();
    if (!gateway) {
        lamb_log(LOG_ERR, "gateway pool initialization failed");
        return;
    }

    gateway->match = lamb_queue_compare;

    /* Database Initialization */
    err = lamb_db_init(&db);
    if (err) {
        lamb_log(LOG_ERR, "database initialization failed");
        return;
    }

    err = lamb_db_connect(&db, config.db_host, config.db_port,
                          config.db_user, config.db_password, config.db_name);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to database %s", config.db_host);
        return;
    }

    /* MT Server Initialization */
    err = lamb_nn_server(&fd, config.listen, config.port, NN_REP);
    if (err) {
        lamb_log(LOG_ERR, "scheduler initialization failed");
        return;
    }
    
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
            lamb_log(LOG_WARNING, "invalid request from client");
            continue;
        }

        req = request__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
        nn_freemsg(buf);

        if (!req) {
            lamb_log(LOG_ERR, "can't parse protocol packets");
            continue;
        }

        if (req->id < 1) {
            lamb_log(LOG_WARNING, "can't recognition client identity");
            continue;
        }

        struct timeval now;
        struct timespec timeout;
        
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = now.tv_usec * 1000;
        timeout.tv_sec += 3;

        pthread_mutex_lock(&mutex);

        if (req->type == LAMB_TEST) {
            lamb_start_thread(lamb_test_loop,  (void *)req, 1);
        } else if (req->type == LAMB_PULL) {
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

void *lamb_test_loop(void *arg) {
    int fd;
    int err;
    int timeout;
    char host[128];
    Request *client;

    client = (Request *)arg;

    lamb_debug("new test client from %s connectd\n", client->addr);

    unsigned short port = config.port + 1;
    err = lamb_child_server(&fd, config.listen, &port, NN_PAIR);
    if (err) {
        pthread_cond_signal(&cond);
        request__free_unpacked(client, NULL);
        lamb_log(LOG_ERR, "lamb can't find available port");
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
    int rc;
    int len;
    char *buf;
    int channel;
    Message *msg;
    lamb_node_t *node;
    lamb_queue_t *queue;
    lamb_submit_t *message;
    bool completed = false;

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

        /* Submit message */
        if (CHECK_COMMAND(buf) == LAMB_MESSAGE) {
            msg = message__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!msg) {
                continue;
            }

            message = (lamb_submit_t *)calloc(1, sizeof(lamb_submit_t));

            if (!message) {
                message__free_unpacked(msg, NULL);
                continue;
            }

            message->id = msg->id;
            channel = msg->channel;
            strncpy(message->spid, msg->spid, 6);
            strncpy(message->spcode, msg->spcode, 20);
            strncpy(message->phone, msg->phone, 11);
            message->msgfmt = msg->msgfmt;
            message->length = msg->length;
            memcpy(message->content, msg->content.data, msg->content.len);

            message__free_unpacked(msg, NULL);

            /* Search for gateway channels */
            node = lamb_list_find(gateway, (void *)(intptr_t)channel);
            if (node) {
                queue = (lamb_queue_t *)node->val;
                lamb_queue_push(queue, message);
                completed = true;
            }

            len = lamb_pack_assembly(&buf, completed ? LAMB_OK : LAMB_NOROUTE, NULL, 0);
            nn_send(fd, buf, len, NN_DONTWAIT);
            free(buf);
            continue;
        }

        /* Close */
        if (CHECK_COMMAND(buf) == LAMB_BYE) {
            nn_freemsg(buf);
            break;
        }

        lamb_debug("invalid request data packet\n");
        nn_freemsg(buf);
    }

    nn_close(fd);
    lamb_debug("connection closed from %s\n", client->addr);
    lamb_log(LOG_INFO, "connection closed from %s", client->addr);
    request__free_unpacked(client, NULL);

    pthread_exit(NULL);
}

void *lamb_push_loop(void *arg) {
    int fd;
    int err;
    int timeout;
    char host[128];
    Request *client;
    lamb_list_t *channels;
    
    client = (Request *)arg;

    lamb_debug("new client from %s connectd\n", client->addr);

    channels = lamb_list_new();

    if (channels) {
        lamb_route_channel(&db, client->id, channels);
    } else {
        lamb_log(LOG_ERR, "create %d routing object failed", client->id);
        request__free_unpacked(client, NULL);
        pthread_exit(NULL);
    }

#ifdef _DEBUG
    if (channels->len > 0) {
        lamb_debug("fetch routing channels successfull\n");
    } else {
        lamb_debug("no routing channel is available\n");
    }

    lamb_node_t *nd;
    lamb_channel_t *chan;
    lamb_list_iterator_t *it = lamb_list_iterator_new(channels, LIST_HEAD);

    while ((nd = lamb_list_iterator_next(it))) {
        chan = (lamb_channel_t *)nd->val;
        lamb_debug("-> id: %d, gid: %d, weight: %d\n", chan->id, chan->gid, chan->weight);
    }

    lamb_list_iterator_destroy(it);
#endif

    /* Client channel initialization */
    unsigned short port = config.port + 1;
    err = lamb_child_server(&fd, config.listen, &port, NN_PAIR);
    if (err) {
        pthread_cond_signal(&cond);
        request__free_unpacked(client, NULL);
        lamb_log(LOG_ERR, "lamb can't find available port");
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
    int rc;
    int len;
    char *buf = NULL;
    bool available;
    bool operator;
    bool province;
    bool completed;
    Submit *submit;
    lamb_node_t *node;
    lamb_queue_t *queue;
    lamb_channel_t *channel;
    lamb_submit_t *message;

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

        /* Submit */
        if (CHECK_COMMAND(buf) == LAMB_SUBMIT) {
            submit = submit__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!submit) {
                continue;
            }

            message = (lamb_submit_t *)calloc(1, sizeof(lamb_submit_t));

            if (!message) {
                submit__free_unpacked(submit, NULL);
                continue;
            }

            message->id = submit->id;
            message->account = submit->account;
            message->company = submit->company;
            strncpy(message->spid, submit->spid, 6);
            strncpy(message->spcode, submit->spcode, 20);
            strncpy(message->phone, submit->phone, 11);
            message->msgfmt = submit->msgfmt;
            message->length = submit->length;
            memcpy(message->content, submit->content.data, submit->content.len);

            submit__free_unpacked(submit, NULL);

            available = false;
            operator = false;
            province = false;
            completed = false;

            lamb_list_iterator_t *it;
            it = lamb_list_iterator_new(channels, LIST_HEAD);

            while ((node = lamb_list_iterator_next(it))) {
                available = true;
                channel = (lamb_channel_t *)node->val;

                /* Check operators */
                if (lamb_check_operator(channel, message->phone)) {
                    operator = true;
                    /* Check province */
                    if (lamb_check_province(channel, message->phone)) {
                        province = true;
                        node = lamb_list_find(gateway, (void *)(intptr_t)channel->id);

                        if (node) {
                            queue = (lamb_queue_t *)node->val;
                            if (queue->list->len < 128) {
                                completed = true;
                                lamb_queue_push(queue, message);
                                break;
                            }
                        }
                    }
                }
            }

            lamb_list_iterator_destroy(it);

            if (completed) {
                len = lamb_pack_assembly(&buf, LAMB_OK, NULL, 0);
            } else {
                if (!available) {
                    len = lamb_pack_assembly(&buf, LAMB_NOROUTE, NULL, 0);
                } else if (!operator || !province) {
                    len = lamb_pack_assembly(&buf, LAMB_REJECT, NULL, 0);
                } else {
                    len = lamb_pack_assembly(&buf, LAMB_BUSY, NULL, 0);
                }
            }

            nn_send(fd, buf, len, NN_DONTWAIT);
            free(buf);
            continue;
        }

        /* Close */
        if (CHECK_COMMAND(buf) == LAMB_BYE) {
            nn_freemsg(buf);
            break;
        }

        lamb_debug("invalid request data packet\n");
        nn_freemsg(buf);
    }

    nn_close(fd);
    lamb_debug("connection closed from %s\n", client->addr);
    lamb_log(LOG_INFO, "connection closed from %s", client->addr);
    request__free_unpacked(client, NULL);

    pthread_exit(NULL);
}

void *lamb_pull_loop(void *arg) {
    int err;
    int fd, rc;
    int timeout;
    char host[128];
    Request *client;
    lamb_node_t *node;
    lamb_queue_t *queue;
    
    client = (Request *)arg;

    lamb_debug("new client from %s connectd\n", client->addr);

    /* client queue initialization */
    node = lamb_list_find(gateway, (void *)(intptr_t)client->id);

    if (node) {
        queue = (lamb_queue_t *)node->val;
    } else {
        queue = lamb_queue_new(client->id);
        if (queue) {
            lamb_list_rpush(gateway, lamb_node_new(queue));
        }
    }

    if (!queue) {
        lamb_log(LOG_ERR, "can't create %d queue from %s", client->id, client->addr);
        request__free_unpacked(client, NULL);
        pthread_exit(NULL);
    }

    /* Client channel initialization */
    unsigned short port = config.port + 1;
    err = lamb_child_server(&fd, config.listen, &port, NN_REP);
    if (err) {
        pthread_cond_signal(&cond);
        request__free_unpacked(client, NULL);
        lamb_log(LOG_ERR, "lamb can't find available port");
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
    int len;
    char *pk;
    char *buf = NULL;
    lamb_submit_t *message;
    Submit submit = SUBMIT__INIT;
    
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
                nn_send(fd, buf, len, NN_DONTWAIT);
                continue;
            }

            message = (lamb_submit_t *)node->val;
            submit.id = message->id;
            submit.account = message->account;
            submit.company = message->company;
            submit.spid = message->spid;
            submit.spcode = message->spcode;
            submit.phone = message->phone;
            submit.msgfmt = message->msgfmt;
            submit.length = message->length;
            submit.content.len = message->length;
            submit.content.data = (uint8_t *)message->content;

            len = submit__get_packed_size(&submit);
            pk = malloc(len);

            if (pk) {
                submit__pack(&submit, (uint8_t *)pk);
                len = lamb_pack_assembly(&buf, LAMB_SUBMIT, pk, len);
                if (len > 0) {
                    nn_send(fd, buf, len, 0);
                    free(buf);
                }
                free(pk);
            }

            free(message);
            free(node);
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
    lamb_log(LOG_INFO, "connection closed from %s", client->addr);
    request__free_unpacked(client, NULL);

    pthread_exit(NULL);
}

void *lamb_stat_loop(void *arg) {
    lamb_node_t *node;
    lamb_queue_t *queue;
    long long length = 0;    

    while (true) {
        length = 0;
        lamb_list_iterator_t *it;
        it = lamb_list_iterator_new(gateway, LIST_HEAD);

        while ((node = lamb_list_iterator_next(it))) {
            queue = (lamb_queue_t *)node->val;
            length += queue->list->len;
            lamb_debug("queue: %d, len: %u\n", queue->id, queue->list->len);
        }

        lamb_list_iterator_destroy(it);
        lamb_sleep(3000);
    }

    pthread_exit(NULL);
}

int lamb_child_server(int *sock, const char *host, unsigned short *port, int protocol) {
    while (true) {
        if (!lamb_nn_server(sock, host, *port, protocol)) {
            break;
        }
        (*port)++;
    }

    return 0;
}

void lamb_route_channel(lamb_db_t *db, int id, lamb_list_t *channels) {
    int err, gid;
    lamb_account_t account;

    memset(&account, 0, sizeof(account));
    err = lamb_account_fetch(db, id, &account);

    if (!err) {
        gid = lamb_rexp_routing(db, account.username);
        if (gid > 0) {
            lamb_get_channels(db, gid, channels);
        }
    }

    return;
}

bool lamb_check_operator(lamb_channel_t *channel, char *phone) {
    int i;
    int len;

    len = sizeof(cmcc) / sizeof(cmcc[0]);

    for (i = 0; i < len; i++) {
        if (memcmp(phone, cmcc[i], 3) == 0) {
            if (channel->operator & LAMB_CMCC) {
                return true;
            }
        }
    }

    len = sizeof(ctcc) / sizeof(ctcc[0]);

    for (i = 0; i < len; i++) {
        if (memcmp(phone, ctcc[i], 3) == 0) {
            if (channel->operator & LAMB_CTCC) {
                return true;
            }
        }
    }

    len = sizeof(cucc) / sizeof(cucc[0]);

    for (i = 0; i < len; i++) {
        if (memcmp(phone, cucc[i], 3) == 0) {
            if (channel->operator & LAMB_CUCC) {
                return true;
            }
        }
    }

    if (channel->operator & LAMB_MVNO) {
        return true;
    }

    return false;
}

bool lamb_check_province(lamb_channel_t *channel, char *phone) {
    return true;
}

int lamb_read_config(lamb_config_t *conf, const char *file) {
    if (!conf) {
        return -1;
    }

    config_t cfg;
    if (lamb_read_file(&cfg, file) != 0) {
        fprintf(stderr, "ERROR: Can't open the %s configuration file\n", file);
        goto error;
    }

    if (lamb_get_int(&cfg, "Id", &conf->id) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Id' parameter\n");
        goto error;
    }

    if (lamb_get_bool(&cfg, "Debug", &conf->debug) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Debug' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Listen", conf->listen, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid Listen IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Port", &conf->port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Port' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Ac", conf->ac, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Ac' parameter\n");
    }

    if (lamb_get_string(&cfg, "DbHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DbPort", &conf->db_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPort' parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid DB port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbPassword", conf->db_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "DbName", conf->db_name, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbName' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'LogFile' parameter\n");
        goto error;
    }

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}

