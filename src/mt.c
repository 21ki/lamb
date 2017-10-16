
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

    clilen = sizeof(struct sockaddr);

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
                lamb_amqp_t *client = (lamb_amqp_t *)calloc(1, sizeof(lamb_amqp_t));
                if (!client) {
                    lamb_errlog(config.logfile, "the system cannot allocate memory", 0);
                }

                client->fd = confd;
                lamb_sock_nonblock(client->fd, true);
                lamb_sock_tcpnodelay(client->fd, true);
                pthread_mutex_init(&client->lock, NULL);
                client->ip = lamb_strdup(inet_ntoa(clientaddr.sin_addr));

                lamb_start_thread(lamb_work_loop,  (void *)client, 1);

                lamb_errlog(config.logfile, "New client connection from %s", client->ip);
            }
        }
    }
    
}

void *lamb_work_loop(void *arg) {
    int ret, event;
    lamb_node_t *node;
    lamb_queue_t *queue;
    lamb_amqp_t *client;
    lamb_submit_t *submit;
    lamb_amqp_message message;
    
    printf("-> Start a new %lu thread ", pthread_self());

    client = (lamb_amqp_t *)arg;

    int epfd, event;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = client->fd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, client->fd, &ev);

    while (true) {
        event = epoll_wait(epfd, events, 32, -1);
        if (event > 0) {
            memset(&message, 0, sizeof(message));

            ret = lamb_sock_recv(client->fd, (char *)&message, 4, config.timeout);

            if (ret == -1) {
                break;
            }

            queue = lamb_find_queue(list_pools, message->id);

            if (!queue) {
                queue = lamb_queue_new(message->account);
                if (queue) {
                    lamb_queue_add(list_pools, queue);
                }
            }

            if (queue) {
                if (message.type == LAMB_AMQP_PUSH) {
                    
                }

                ret = lamb_queue_push(queue, (void *)message);
                if (ret) {
                    lamb_sock_ack(confd);
                    continue;
                }
            }

            lamb_sock_reject(confd);
        }
    }

    printf("-> The client %s disconnect\n", client->ip);
    
    close(epfd);
    free(client->ip);
    lamb_amqp_destroy(client);
    
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

lamb_node_t *lamb_queue_add(lamb_pool_t *self, lamb_queue_t *queue) {
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

lamb_node_t *lamb_find_queue(lamb_pool_t *self, int id) {
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

