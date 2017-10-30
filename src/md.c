
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
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
#include "utils.h"
#include "config.h"
#include "cache.h"
#include "socket.h"
#include "md.h"

static lamb_rep_t resp;
static lamb_cache_t rdb;
static lamb_pool_t *pools;
static lamb_config_t config;
static lamb_queue_t *delivery;
static pthread_cond_t cond;
static pthread_mutex_t mutex;

int main(int argc, char *argv[]) {
    char *file = "md.conf";
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

    /* Signal event processing */
    lamb_signal_processing();

    /* Start Main Event Thread */
    lamb_set_process("lamb-mdd");
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int fd, err;

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    
    /* Client Queue Pools Initialization */
    pools = lamb_pool_new();
    if (!pools) {
        lamb_errlog(config.logfile, "lamb queue pool initialization failed", 0);
        return;
    }

    delivery = lamb_queue_new(0);
    if (!delivery) {
        lamb_errlog(config.logfile, "delivery queue initialization failed", 0);
        return;
    }
    
    /* Redis Cache Initialization */
    err = lamb_cache_connect(&rdb, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to redis %s server", config.redis_host);
        return;
    }

    /* MT Server Initialization */
    err = lamb_server_init(&fd, config.listen, config.port);
    if (err) {
        return;
    }

    /* Start Data Acquisition Thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);

    int rc, len;
    lamb_req_t *req;

    len = sizeof(lamb_req_t);
    
    while (true) {
        char *buf = NULL;
        rc = nn_recv(fd, &buf, NN_MSG, 0);

        if (rc < 0) {
            continue;
        }
        
        if (rc != len) {
            nn_freemsg(buf);
            lamb_errlog(config.logfile, "Invalid request from client", 0);
            continue;
        }

        req = (lamb_req_t *)buf;

        if (req->id < 1) {
            nn_freemsg(buf);
            lamb_errlog(config.logfile, "Requests from unidentified clients", 0);
            continue;
        }

        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + 3;

        pthread_mutex_lock(&mutex);
        memset(&resp, 0, sizeof(resp));

        if (req->type == LAMB_NN_PULL) {
            lamb_start_thread(lamb_pull_loop,  (void *)buf, 1);
        } else if (req->type == LAMB_NN_PUSH) {
            lamb_start_thread(lamb_push_loop,  (void *)buf, 1);
        } else {
            nn_freemsg(buf);
            pthread_mutex_unlock(&mutex);
            continue;
        }

        pthread_cond_timedwait(&cond, &mutex, &timeout);
        nn_send(fd, (char *)&resp, sizeof(resp), 0);
        pthread_mutex_unlock(&mutex);
    }
}

void *lamb_push_loop(void *arg) {
    int err;
    int fd, rc;
    lamb_req_t *client;
    lamb_queue_t *queue;
    
    client = (lamb_req_t *)arg;

    printf("-> new client from %s connectd\n", client->addr);

    /* Client channel initialization */
    unsigned short port = 30001;
    err = lamb_child_server(&fd, config.listen, &port, NN_PAIR);
    if (err) {
        pthread_cond_signal(&cond);
        lamb_errlog(config.logfile, "lamb can't find available port", 0);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex);

    resp.id = client->id;
    memcpy(resp.addr, config.listen, 16);
    resp.port = port;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);

    /* Start event processing */
    int len, rlen, dlen;
    lamb_report_t *report;
    lamb_message_t *message;
    
    len = sizeof(lamb_message_t);
    rlen = sizeof(lamb_report_t);
    dlen = sizeof(lamb_deliver_t);
    
    while (true) {
        char *buf = NULL;
        rc = nn_recv(fd, &buf, NN_MSG, 0);
        
        if (rc < 0) {
            continue;
        }

        if (rc != len && rc != rlen && rc != dlen) {
            nn_freemsg(buf);
            continue;
        }

        message = (lamb_message_t *)buf;

        /* Report */
        if (message->type == LAMB_REPORT) {
            report = (lamb_report_t *)message;
            queue = lamb_pool_find(pools, report->account);
            if (!queue) {
                queue = lamb_queue_new(report->account);
                if (queue) {
                    lamb_pool_add(pools, queue);
                }
            }

            if (queue) {
                lamb_queue_push(queue, message);
            } else {
                nn_freemsg(buf);
            }

            continue;
        }

        /* Delivery */
        if (message->type == LAMB_DELIVER) {
            lamb_queue_push(delivery, message);
            continue;
        }

        /* Close */
        if (message->type == LAMB_BYE) {
            nn_freemsg(buf);
            break;
        }

        nn_freemsg(buf);
    }

    nn_close(fd);
    nn_freemsg((char *)client);
    printf("-> connection closed from %s\n", client->addr);
    lamb_errlog(config.logfile, "connection closed from %s", client->addr);

    pthread_exit(NULL);
}

void *lamb_pull_loop(void *arg) {
    int err;
    int fd, rc;
    long long timeout;
    lamb_node_t *node;
    lamb_req_t *client;
    lamb_queue_t *queue;
    
    client = (lamb_req_t *)arg;

    printf("-> new client from %s connectd\n", client->addr);

    /* client queue initialization */
    queue = lamb_pool_find(pools, 0);
    if (!queue) {
        queue = lamb_queue_new(0);
        if (queue) {
            lamb_pool_add(pools, queue);
        }
    }

    if (!queue) {
        nn_freemsg((char *)client);
        lamb_errlog(config.logfile, "can't create queue for client %s", client->addr);
        pthread_exit(NULL);
    }

    /* Client channel initialization */
    unsigned short port = 30001;
    err = lamb_child_server(&fd, config.listen, &port, NN_REP);
    if (err) {
        pthread_cond_signal(&cond);
        lamb_errlog(config.logfile, "lamb can't find available port", 0);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex);

    resp.id = client->id;
    memcpy(resp.addr, config.listen, 16);
    resp.port = port;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);

    timeout = config.timeout;
    nn_setsockopt(fd, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(timeout));
    
    /* Start event processing */
    int len, rlen, dlen;
    lamb_message_t *message;

    len = sizeof(lamb_message_t);
    rlen = sizeof(lamb_report_t);
    dlen = sizeof(lamb_deliver_t);
    
    while (true) {
        char *buf = NULL;
        rc = nn_recv(fd, &buf, NN_MSG, 0);

        if (rc < 0) {
            if (nn_errno() == ETIMEDOUT) {
                break;
            }
            continue;
        }

        if (rc < len) {
            nn_freemsg(buf);
            continue;
        }

        message = (lamb_message_t *)buf;

        if (message->type == LAMB_REQ) {
            node = lamb_queue_pop(queue);
            if (node) {
                message = (lamb_message_t *)node->val;
                if (message->type == LAMB_REPORT) {
                    nn_send(fd, message, rlen, 0);
                } else if (message->type == LAMB_DELIVER) {
                    nn_send(fd, message, dlen, 0);
                }
                nn_freemsg(message);
                free(node);
            } else {
                nn_send(fd, "0", 1, 0);
            }

            nn_freemsg(buf);
            continue;
        }

        if (message->type == LAMB_BYE) {
            nn_freemsg(buf);
            break;
        }

        nn_freemsg(buf);
    }

    nn_close(fd);
    nn_freemsg((char *)client);
    printf("-> connection closed from %s\n", client->addr);
    lamb_errlog(config.logfile, "connection closed from %s", client->addr);

    pthread_exit(NULL);
}

int lamb_server_init(int *sock, const char *listen, int port) {
    int fd;
    char addr[128];

    sprintf(addr, "tcp://%s:%d", listen, port);
    
    fd = nn_socket(AF_SP, NN_REP);
    if (fd < 0) {
        lamb_errlog(config.logfile, "socket %s", nn_strerror(nn_errno()));
        return -1;
    }

    if (nn_bind(fd, addr) < 0) {
        nn_close(fd);
        lamb_errlog(config.logfile, "bind %s", nn_strerror(nn_errno()));
        return -1;
    }

    *sock = fd;

    return 0;
}

int lamb_child_server(int *sock, const char *listen, unsigned short *port, int protocol) {
    int fd;
    char addr[128];
    
    fd = nn_socket(AF_SP, protocol);
    if (fd < 0) {
        lamb_errlog(config.logfile, "socket %s", nn_strerror(nn_errno()));
        return -1;
    }

    while (true) {
        sprintf(addr, "tcp://%s:%u", listen, *port);

        if (nn_bind(fd, addr) > 0) {
            break;
        }

        (*port)++;
    }

    *sock = fd;
    
    return 0;
}

void *lamb_stat_loop(void *arg) {
    redisReply *reply;
    lamb_queue_t *queue;
    long long length = 0;    

    while (true) {
        length = 0;
        queue = pools->head;        

        while (queue != NULL) {
            length += queue->len;
            printf("-> queue: %d, len: %lld\n", queue->id, queue->len);
            queue = queue->next;
        }

        reply = redisCommand(rdb.handle, "HMSET md.%d queue %lld", config.id, length);
        if (reply != NULL) {
            freeReplyObject(reply);
        }

        lamb_sleep(3000);
    }

    pthread_exit(NULL);
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

    if (lamb_get_string(&cfg, "RedisHost", conf->redis_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisHost' parameter\n");
    }

    if (lamb_get_int(&cfg, "RedisPort", &conf->redis_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisPort' parameter\n");
    }

    if (conf->redis_port < 1 || conf->redis_port > 65535) {
        fprintf(stderr, "ERROR: Invalid redis port number\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisDb' parameter\n");
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

