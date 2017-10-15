
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
#include "utils.h"
#include "config.h"
#include "socket.h"
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
    int i, err;
    int fd, confd;
    socklen_t clilen;
    struct sockaddr_in clientaddr;

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
        lamb_errlog(config.logfile, "lamb server initialization failed", 0);
        return;
    }

    /* Start Data Acquisition Thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);
    
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = fd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);
        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == fd) {
                /* Accept new client connection  */
                confd = accept(fd, (struct sockaddr *)&clientaddr, &clilen);
                if (confd < 0) {
                    lamb_errlog(config.logfile, "server accept client connect error", 0);
                    continue;
                }

                /* Start client work thread */
                lamb_start_thread(lamb_work_loop,  (void *)(intptr_t)confd, 1);
                lamb_errlog(config.logfile, "New client connection from %s", inet_ntoa(clientaddr.sin_addr));
            }
        }
    }
    
}

void *lamb_work_loop(void *arg) {
    int err;
    lamb_node_t *node;
    int confd = (intptr_t)arg;
    lamb_submit_t *message;
    
    printf("start a new %lu thread ", pthread_self());

    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = confd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);
        if (nfds > 0) {
            message = (lamb_submit_t *)calloc(1, sizeof(lamb_submit_t));
            if (!message) {
                continue;
            }

            err = lamb_sock_recv(confd, (char *)message, sizeof(lamb_submit_t), 1000);
            if (err == -1) {
                free(message);
                break;
            }

            node = lamb_find_node(list_pools, message->account);
            if (node) {
                lamb_msg_push(node, (void *)message);
            } else {
                node = lamb_node_new(message->account);
                if (node) {
                    lamb_node_add(list_pools, node);
                    lamb_msg_push(node, (void *)message);
                }
            }
        }
    }

    printf("close %lu thread ", pthread_self());
    
    pthread_exit(NULL);
}

int lamb_server_init(int *sock, const char *addr, int port) {
    int fd, err;

    fd = lamb_sock_create();
    if (fd < 0) {
        lamb_errlog(config.logfile, "lamb create socket failed", 0);
        return -1;
    }

    err = lamb_sock_bind(fd, addr, port, 1024);
    if (err) {
        lamb_errlog(config.logfile, "lamb bind socket failed", 0);
        return -1;
    }

    lamb_sock_nonblock(fd, true);
    lamb_sock_tcpnodelay(fd, true);

    *sock = fd;

    return 0;
}

lamb_node_t *lamb_node_new(int id) {
    lamb_node_t *self;

    self = malloc(sizeof(lamb_node_t));
    if (!self) {
        return NULL;
    }

    self->id = id;
    self->len = 0;
    pthread_mutex_init(&self->lock, NULL);
    self->head = NULL;
    self->tail = NULL;
    self->next = NULL;

    return self;
}

lamb_msg_t *lamb_msg_push(lamb_node_t *self, void *val) {
    if (!val) {
        return NULL;
    }

    lamb_msg_t *msg;
    msg = (lamb_msg_t *)malloc(sizeof(lamb_msg_t));

    if (msg) {
        msg->val = val;
        msg->next = NULL;
        if (self->len) {
            self->tail->next = msg;
            self->tail = msg;
        } else {
            self->head = self->tail = msg;
        }
        ++self->len;
    }

    return msg;
}

lamb_msg_t *lamb_msg_pop(lamb_node_t *self) {
    if (!self->len) {
        return NULL;
    }

    lamb_msg_t *msg = self->head;

    if (--self->len) {
        self->head = msg->next;
    } else {
        self->head = self->tail = NULL;
    }

    msg->next = NULL;

    return msg;
}

lamb_node_t *lamb_node_add(lamb_pool_t *self, lamb_node_t *node) {
    if (!node) {
        return NULL;
    }

    if (self->len) {
        self->tail->next = node;
        self->tail = node;
    } else {
        self->head = self->tail = node;
    }
    ++self->len;

    return node;
}

void lamb_node_del(lamb_pool_t *self, int id) {
    lamb_node_t *curr;
    lamb_node_t *node;

    node = NULL;
    curr = self->head;

    while (curr != NULL) {
        if (curr->id == id) {
            if (node) {
                node->next = curr->next;
            } else {
                self->head = curr->next;
            }

            --self->len;
            free(curr);
            break;
        }

        node = curr;
        curr = curr->next;
    }

    return;
}

lamb_node_t *lamb_find_node(lamb_pool_t *self, int id) {
    lamb_node_t *node;

    node = self->head;
    
    while (node != NULL) {
        if (node->id == id) {
            break;
        }
        node = node->next;
    }

    return node;
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
    lamb_node_t *node;
    redisReply *reply;
    
    while (true) {
        node = list_pools->head;
        while (node != NULL) {
            reply = redisCommand(rdb.handle, "HMSET client.%d queue %lld", node->id, node->len);
            if (reply != NULL) {
                freeReplyObject(reply);
            }
            printf("-> node: %d, len: %lld\n", node->id, node->len);
            node = node->next;
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

