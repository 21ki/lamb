
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
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <errno.h>
#include <cmpp.h>
#include "common.h"
#include "ac.h"
#include "db.h"
#include "cache.h"
#include "list.h"
#include "queue.h"
#include "socket.h"
#include "config.h"

static lamb_db_t db;
static lamb_cache_t rdb;
static lamb_config_t config;
static lamb_list_t *mt;
static lamb_list_t *mo;
static lamb_list_t *ismg;
static lamb_list_t *server;
static lamb_list_t *scheduler;
static lamb_list_t *delivery;
static lamb_list_t *gateway;

int main(int argc, char *argv[]) {
    char *file = "ac.conf";
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

    /* log file setting */
    if (setenv("logfile", config.logfile, 1) == -1) {
        lamb_vlog(LOG_ERR, "setenv error: %s", strerror(errno));
        return -1;
    }
    
    /* Signal event processing */
    lamb_signal_processing();

    /* Start Main Event Thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int fd;
    int err;
    void *pk;
    int rc, len;
    int timeout;
    Request *req;
    char *buf = NULL;
    char host[64] = {0};
    unsigned short port;
    lamb_client_t *client;
    Response resp = LAMB_RESPONSE_INIT;

    lamb_set_process("lamb-acd");

    err = lamb_component_initialization(&config);

    if (err) {
        return;
    }

    /* start listen daemon thread */
    lamb_start_thread(lamb_listen_loop, NULL, 1);

    /* start heartbeat detection thread */
    lamb_start_thread(lamb_check_keepalive, NULL, 1);
    
    /* Start main loop thread */
    err = lamb_nn_server(&fd, config.listen, config.port, NN_REP);
    if (err) {
        lamb_log(LOG_ERR, "ac server initialization failed");
        return;
    }

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

        req = lamb_request_unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
        nn_freemsg(buf);

        if (!req) {
            lamb_log(LOG_ERR, "can't unpack request protocol packets");
            continue;
        }

        if (req->id < 1) {
            lamb_request_free_unpacked(req, NULL);
            lamb_log(LOG_WARNING, "can't recognition client identity");
            continue;
        }

        int cli;
        port = config.port + 1;
        err = lamb_child_server(&cli, config.listen, &port, NN_PAIR);

        if (err) {
            lamb_request_free_unpacked(req, NULL);
            lamb_log(LOG_ERR, "lamb can't find available port");
            continue;
        }

        resp.id = req->id;
        snprintf(host, sizeof(host), "tcp://%s:%d", config.listen, port);
        resp.host = host;

        timeout = config.timeout;
        nn_setsockopt(fd, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(timeout));

        len = lamb_response_get_packed_size(&resp);
        pk = malloc(len);

        if (!pk) {
            lamb_request_free_unpacked(req, NULL);
            continue;
        }

        lamb_response_pack(&resp, pk);
        len = lamb_pack_assembly(&buf, LAMB_RESPONSE, pk, len);
        if (len > 0) {
            nn_send(fd, buf, len, 0);
            free(buf);
        }

        free(pk);
        client = (lamb_client_t *)calloc(1, sizeof(lamb_client_t));

        if (!client) {
            nn_close(cli);
            lamb_request_free_unpacked(req, NULL);
            continue;
        }

        client->id = req->id;
        client->sock = cli;
        strncpy(client->addr, req->addr, 15);
        pthread_mutex_init(&client->lock, NULL);
        nn_setsockopt(cli, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(timeout));

        if (req->type == LAMB_MT) {
            /* Start mt processing thread */
            lamb_start_thread(lamb_mt_loop, (void *)client, 1);
        } else if (req->type == LAMB_MO) {
            lamb_list_rpush(mo, lamb_node_new(client));
        } else if (req->type == LAMB_ISMG) {
            lamb_list_rpush(ismg, lamb_node_new(client));
        } else if (req->type == LAMB_SERVER) {
            lamb_list_rpush(server, lamb_node_new(client));
        } else if (req->type == LAMB_SCHEDULER) {
            lamb_list_rpush(scheduler, lamb_node_new(client));
        } else if (req->type == LAMB_DELIVERY) {
            lamb_list_rpush(delivery, lamb_node_new(client));
        } else if (req->type == LAMB_GATEWAY) {
            lamb_list_rpush(gateway, lamb_node_new(client));
        } else {
            nn_close(cli);
            lamb_log(LOG_ERR, "invalid type of client");
        }

        lamb_request_free_unpacked(req, NULL);
    }

    return;
}

void *lamb_listen_loop(void *arg) {
    int rc;
    int err;
    int sock;
    char *buf;

    err = lamb_cli_daemon(&scok, "127.0.0.1", 1024);

    if (err) {
        lamb_log(LOG_ERR, "can't start the cli daemon");
        pthread_exit(NULL);
    }

    lamb_debug("-> start cli listen successfull!\n");
    
    while (true) {
        rc = nn_recv(sock, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }
            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_DELIVERY_RELOAD) {
            
        }

        lamb_debug("recvive a client command\n");

        nn_freemsg(buf);
    }

    pthread_exit(NULL);
}

void *lamb_check_keepalive(void *arg) {
    char *buf;
    char *ping;
    int rc, len, err;
    lamb_node_t *node;
    lamb_list_t *list;
    lamb_list_iterator_t *it;
    lamb_client_t *client;

    list = (lamb_list_t *)arg;
    len = lamb_pack_assembly(&ping, LAMB_PING, NULL, 0);

    while (true) {
        it = lamb_list_iterator_new(list, LIST_HEAD);

        while ((node = lamb_list_iterator_next(it))) {
            client = (lamb_client_t *)node->val;
            pthread_mutex_lock(&client->lock);
            nn_send(client->sock, ping, len, NN_DONTWAIT);

            rc = nn_recv(client->sock, buf, NN_MSG, 0);

            if (rc < HEAD) {
                if (rc > 0) {
                    nn_freemsg(buf);
                }
                pthread_mutex_lock(&client->lock);
                continue;
            }

            if (CHECK_COMMAND(buf) == LAMB_OK) {
                pthread_mutex_lock(&client->lock);
                continue;
            }

            lamb_list_remove(list, node);
            nn_freemsg(buf);
        }

        lamb_list_iterator_destroy(it);
        lamb_sleep(5000);
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

int lamb_cli_daemon(int *sock, const char *host, unsigned short port) {
    return lamb_nn_server(sock, host, port, NN_PAIR);
}

void lamb_free_client(void *val) {
    lamb_client_t *cli;

    cli = (lamb_client_t *)val;

    
}

int lamb_component_initialization(lamb_config_t *cfg) {
    int err;

    if (!cfg) {
        return -1;
    }

    /* mt object queue initialization */
    mt = lamb_list_new();
    if (!mt) {
        lamb_log(LOG_ERR, "mt object queue initialization failed");
        return -1;
    }
    mt->free = lamb_free_client;

    /* mo object queue initialization */
    mo = lamb_list_new();
    if (!mo) {
        lamb_log(LOG_ERR, "mo object queue initialization failed");
        return -1;
    }
    mo->free = lamb_free_client;

    /* ismg object queue initialization */
    ismg = lamb_list_new();
    if (!ismg) {
        lamb_log(LOG_ERR, "ismg object queue initialization failed");
        return -1;
    }
    ismg->free = lamb_free_client;
    
    /* server object queue initialization */
    server = lamb_list_new();
    if (!server) {
        lamb_log(LOG_ERR, "server object queue initialization failed");
        return -1;
    }
    server->free = lamb_free_client;
    
    /* scheduler object queue initialization */
    scheduler = lamb_list_new();
    if (!scheduler) {
        lamb_log(LOG_ERR, "scheduler object queue initialization failed");
        return -1;
    }
    scheduler->free = lamb_free_client;

    /* delivery object queue initialization */
    delivery = lamb_list_new();
    if (!delivery) {
        lamb_log(LOG_ERR, "delivery object queue initialization failed");
        return -1;
    }
    delivery->free = lamb_free_client;

    /* gateway object queue initialization */
    gateway = lamb_list_new();
    if (!gateway) {
        lamb_log(LOG_ERR, "gateway object queue initialization failed");
        return -1;
    }
    gateway->free = lamb_free_client;

    /* redis initialization */
    err = lamb_cache_connect(&rdb, cfg->redis_host, cfg->redis_port, NULL,
                             cfg->redis_db);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to redis server %s", cfg->redis_host);
        return -1;
    }

    /* database initialization */
    err = lamb_db_init(&db);
    if (err) {
        lamb_log(LOG_ERR, "database initialization failed");
        return -1;
    }

    err = lamb_db_connect(&db, cfg->db_host, cfg->db_port, cfg->db_user,
                          cfg->db_password, cfg->db_name);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to database %s", cfg->db_host);
        return -1;
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
    
    if (lamb_get_string(&cfg, "Listen", conf->listen, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid Listen IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Port", &conf->port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Port' parameter\n");
        goto error;
    }

    if (conf->port < 1 || conf->port > 65535) {
        fprintf(stderr, "ERROR: Invalid listen port number\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Connections", &conf->connections) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Connections' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "RedisHost", conf->redis_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisPort", &conf->redis_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisPort' parameter\n");
        goto error;
    }

    if (conf->redis_port < 1 || conf->redis_port > 65535) {
        fprintf(stderr, "ERROR: Invalid redis port number\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisDb' parameter\n");
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

