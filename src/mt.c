
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
#include <pthread.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include "utils.h"
#include "config.h"
#include "mt.h"

static lamb_config_t config;

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
    
    err = lamb_server_init(&fd, config.listen, config.port);
    if (err) {
        lamb_errlog(config.logfile, "lamb server initialization failed", 0);
        lamb_debug("lamb server initialization failed");
        return;
    }

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
                    lamb_debug("server accept client connect error");
                    continue;
                }

                /* Start client work thread */
                lamb_start_thread(lamb_work_loop,  (void *)confd, 1);
                lamb_errlog(config.logfile, "New client connection from %s", inet_ntoa(clientaddr.sin_addr));
                lamb_debug("New client connection from %s", inet_ntoa(clientaddr.sin_addr));
            }
        }
    }
    
}

void *lamb_work_loop(void *arg) {
    int err;
    int confd = (int)arg;
    lamb_submit_t *message;
    
    lamb_debug("start a new %lu thread ", pthread_self());

    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = confd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);
        if (nfds > 0) {
            message = (lamb_submit_t *)calloc(1, sizeof(message));
            err = lamb_sock_recv(confd, &message, sizeof(message));
            if (err == -1) {
                break;
            }

            if (message->spid) {
                
            }
        }
    }
    
    pthread_exit(NULL);
}

int lamb_server_init(int *sock, const char *addr, int port) {
    int fd;
    struct sockaddr_in servaddr;
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    if (inet_pton(AF_INET, addr, &servaddr.sin_addr) < 1) {
        return 1;
    }

    servaddr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) == -1) {
        return 2;
    }

    if (listen(sock->fd, backlog) == -1) {
        return 3;
    }

    lamb_sock_nonblock(fd, true);
    lamb_sock_tcpnodelay(fd, true);

    *sock = fd;

    return 0;
}

lamb_list_t *lamb_list_new(void) {
    lamb_list_t *self;

    self = malloc(sizeof(lamb_list_t));
    if (!self) {
        return NULL;
    }

    self->len = 0;
    pthread_mutex_init(&self->lock, NULL);
    self->head = NULL;
    self->tail = NULL;

    return self;
}

lamb_node_t *lamb_list_push(lamb_list_t *self, void *val) {
    if (!val) {
        return NULL;
    }

    lamb_node_t *node;
    node = (lamb_node_t *)malloc(sizeof(lamb_node_t));

    if (node) {
        node->val = val;
        node->next = NULL;
        if (self->len) {
            self->tail->next = node;
            self->tail = node;
        } else {
            self->head = self->tail = node;
        }
        ++self->len;
    }

    return node;
}

lamb_node_t *lamb_list_pop(lamb_list_t *self) {
    if (!self->len) {
        return NULL;
    }

    lamb_node_t *node = self->head;

    if (--self->len) {
        self->head = node->next;
    } else {
        self->head = self->tail = NULL;
    }

    node->next = NULL;

    return node;
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

