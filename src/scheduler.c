
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
#include "pool.h"
#include "scheduler.h"

//static int ac;
static lamb_rep_t resp;
static lamb_pool_t *pools;
static lamb_config_t config;
static pthread_cond_t cond;
static pthread_mutex_t mutex;

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
    pools = lamb_pool_new();
    if (!pools) {
        lamb_log(LOG_ERR, "queue pool initialization failed");
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
            lamb_log(LOG_WARNING, "Invalid request from client");
            continue;
        }

        req = (lamb_req_t *)buf;

        if (req->id < 1) {
            nn_freemsg(buf);
            lamb_log(LOG_WARNING, "Requests from unidentified clients");
            continue;
        }

        struct timeval now;
        struct timespec timeout;
        
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = now.tv_usec * 1000;
        timeout.tv_sec += 3;

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
    int timeout;
    lamb_req_t *client;
    lamb_queue_t *queue;

    client = (lamb_req_t *)arg;

    lamb_debug("new client from %s connectd\n", client->addr);

    /* Client channel initialization */
    unsigned short port = config.port + 1;
    err = lamb_child_server(&fd, config.listen, &port, NN_REP);
    if (err) {
        pthread_cond_signal(&cond);
        lamb_log(LOG_ERR, "lamb can't find available port");
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
    int len, slen;
    lamb_submit_t *submit;
    lamb_message_t *message;
    
    queue = NULL;
    len = sizeof(lamb_message_t);
    slen = sizeof(lamb_submit_t);
    
    while (true) {
        char *buf = NULL;
        rc = nn_recv(fd, &buf, NN_MSG, 0);
        
        if (rc < 0) {
            if (nn_errno() == ETIMEDOUT) {
                if (!nn_get_statistic(fd, NN_STAT_CURRENT_CONNECTIONS)) {
                    break;
                }
            }
            continue;
        }

        if (rc != len && rc != slen) {
            nn_send(fd, "0", 1, NN_DONTWAIT);
            nn_freemsg(buf);
            continue;
        }

        message = (lamb_message_t *)buf;

        /* Submit */
        if (message->type == LAMB_SUBMIT) {
            submit = (lamb_submit_t *)message;
            queue = lamb_pool_find(pools, submit->channel);
            if (!queue) {
                queue = lamb_queue_new(submit->channel);
                if (queue) {
                    lamb_pool_add(pools, queue);
                }
            }

            if (queue && queue->len < 16) {
                lamb_queue_push(queue, submit);
                nn_send(fd, "ok", 2, NN_DONTWAIT);
            } else {
                nn_send(fd, "0", 1, NN_DONTWAIT);
                nn_freemsg(buf);
            }

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
    lamb_debug("connection closed from %s\n", client->addr);
    lamb_log(LOG_INFO, "connection closed from %s", client->addr);

    pthread_exit(NULL);
}

void *lamb_pull_loop(void *arg) {
    int err;
    int fd, rc;
    int timeout;
    lamb_node_t *node;
    lamb_req_t *client;
    lamb_queue_t *queue;
    
    client = (lamb_req_t *)arg;

    lamb_debug("new client from %s connectd\n", client->addr);

    /* client queue initialization */
    queue = lamb_pool_find(pools, client->id);
    if (!queue) {
        queue = lamb_queue_new(client->id);
        if (queue) {
            lamb_pool_add(pools, queue);
        }
    }

    if (!queue) {
        nn_freemsg((char *)client);
        lamb_log(LOG_ERR, "can't create queue for client %s", client->addr);
        pthread_exit(NULL);
    }

    /* Client channel initialization */
    unsigned short port = config.port + 1;
    err = lamb_child_server(&fd, config.listen, &port, NN_REP);
    if (err) {
        pthread_cond_signal(&cond);
        lamb_log(LOG_ERR, "lamb can't find available port");
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
    int len;
    lamb_message_t *message;

    len = sizeof(lamb_message_t);
    
    while (true) {
        char *buf = NULL;
        rc = nn_recv(fd, &buf, NN_MSG, 0);

        if (rc < 0) {
            if (nn_errno() == ETIMEDOUT) {
                if (!nn_get_statistic(fd, NN_STAT_CURRENT_CONNECTIONS)) {
                    break;
                }
            }
            continue;
        }

        if (rc < len) {
            nn_send(fd, "0", 1, NN_DONTWAIT);
            nn_freemsg(buf);
            continue;
        }

        message = (lamb_message_t *)buf;

        if (message->type == LAMB_REQ) {
            node = lamb_queue_pop(queue);
            if (node) {
                nn_send(fd, node->val, sizeof(lamb_submit_t), NN_DONTWAIT);
                nn_freemsg(node->val);
                free(node);
            } else {
                nn_send(fd, "0", 1, NN_DONTWAIT);
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
    lamb_debug("connection closed from %s\n", client->addr);
    lamb_log(LOG_INFO, "connection closed from %s", client->addr);

    pthread_exit(NULL);
}

void *lamb_stat_loop(void *arg) {
    lamb_queue_t *queue;
    long long length = 0;    

    while (true) {
        length = 0;
        queue = pools->head;        

        while (queue != NULL) {
            length += queue->len;
            lamb_debug("queue: %d, len: %lld\n", queue->id, queue->len);
            queue = queue->next;
        }

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

    if (lamb_get_string(&cfg, "AcHost", conf->ac_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'redisHost' parameter\n");
    }

    if (lamb_get_int(&cfg, "AcPort", &conf->ac_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'redisPort' parameter\n");
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

