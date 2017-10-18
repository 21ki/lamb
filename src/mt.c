
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
#include <sys/epoll.h>
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
#include "mt.h"

static lamb_cache_t rdb;
static lamb_config_t config;
static lamb_pool_t *list_pools;

int main(int argc, char *argv[]) {
    char *file = "mt.conf";
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
    lamb_set_process("lamb-mtserv");
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int fd, err;

    /* Client Queue Pools Initialization */
    list_pools = lamb_pool_new();
    if (!list_pools) {
        lamb_errlog(config.logfile, "lamb node pool initialization failed", 0);
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

    int rc;
    
    while (true) {
        char *buf = NULL;
        rc = nn_recv(fd, &buf, NN_MSG, 0);

        if (rc != sizeof(lamb_client_t)) {
            nn_freemsg(buf);
            continue;
        }

        lamb_start_thread(lamb_work_loop,  (void *)buf, 1);
    }
}

void *lamb_work_loop(void *arg) {
    int fd, rc;
    char url[128];
    lamb_node_t *node;
    lamb_queue_t *queue;
    lamb_client_t *client;
    
    client = (lamb_client_t *)arg;

    printf("-> New client from %s connectd\n", client->addr);

    /* Client queue initialization */
    queue = lamb_find_queue(list_pools, client->id);
    if (!queue) {
        queue = lamb_queue_new(client->id);
        if (queue) {
            lamb_queue_add(list_pools, queue);
        }
    }

    if (!queue) {
        nn_freemsg((char *)client);
        lamb_errlog(config.logfile, "can't create queue for client %s", client->addr);
        pthread_exit(NULL);
    }
    
    /* Client channel initialization */
    fd = nn_socket(AF_SP, NN_PAIR);
    if (fd < 0) {
        nn_freemsg((char *)client);
        lamb_errlog(config.logfile, "socket %s", nn_strerror(nn_errno()));
        pthread_exit(NULL);
    }

    nn_setsockopt(fd, NN_SOL_SOCKET, NN_SNDTIMEO, &config.timeout, sizeof(config.timeout));

    sprintf(url, "tcp://%s:%d", client->addr, client->port);
    if (nn_connect(fd, url) < 0) {
        nn_close(fd);
        nn_freemsg((char *)client);
        lamb_errlog(config.logfile, "connect %s", nn_strerror(nn_errno()));
        pthread_exit(NULL);
    }

    /* Start event processing */
    int len;

    len = sizeof(lamb_submit_t);
    
    while (true) {
        char *buf = NULL;
        rc = nn_recv(fd, &buf, NN_MSG, 0);
        
        if (rc < 0) {
            break;
        }

        if (rc == len) {
            node = lamb_queue_push(queue, buf);
            nn_send(fd, node ? "ack" : "err", 4, 0);
            continue;
        }

        if (rc >= 3 && (strncasecmp(buf, "req", 3) == 0)) {
            node = lamb_queue_pop(queue);
            if (node) {
                if (nn_send(fd, node->val, len, 0) < 0) {
                    free(node->val);
                    free(node);
                    nn_freemsg(buf);
                    lamb_errlog(config.logfile, "socket %s from %s", nn_strerror(nn_errno()), client->addr);
                    break;
                }
                free(node->val);
                free(node);
            }
            nn_freemsg(buf);
        }
    }

    nn_close(fd);
    nn_freemsg((char *)client);
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

lamb_queue_t *lamb_queue_add(lamb_pool_t *self, lamb_queue_t *queue) {
    if (!queue) {
        return NULL;
    }

    if (self->len) {
        self->tail->next = queue;
        self->tail = queue;
    } else {
        self->head = self->tail = queue;
    }
    ++self->len;

    return queue;
}

void lamb_queue_del(lamb_pool_t *self, int id) {
    lamb_queue_t *prev;
    lamb_queue_t *curr;

    prev = NULL;
    curr = self->head;

    while (curr != NULL) {
        if (curr->id == id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                self->head = curr->next;
            }

            --self->len;
            free(curr);
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    return;
}

lamb_queue_t *lamb_find_queue(lamb_pool_t *self, int id) {
    lamb_queue_t *queue;

    queue = self->head;
    
    while (queue != NULL) {
        if (queue->id == id) {
            break;
        }
        queue = queue->next;
    }

    return queue;
}

lamb_pool_t *lamb_pool_new(void) {
    lamb_pool_t *self;
    
    self = (lamb_pool_t *)malloc(sizeof(lamb_pool_t));
    if (!self) {
        return NULL;
    }

    self->len = 0;
    self->head = NULL;
    self->tail = NULL;

    return self;
}

void *lamb_stat_loop(void *arg) {
    lamb_queue_t *queue;
    redisReply *reply;
    
    while (true) {
        queue = list_pools->head;
        while (queue != NULL) {
            reply = redisCommand(rdb.handle, "HMSET client.%d queue %lld", queue->id, queue->len);
            if (reply != NULL) {
                freeReplyObject(reply);
            }
            printf("-> node: %d, len: %lld\n", queue->id, queue->len);
            queue = queue->next;
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

    if (lamb_get_int(&cfg, "WorkerThread", &conf->worker_thread) != 0) {
        fprintf(stderr, "ERROR: Can't read 'WorkerThread' parameter\n");
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

