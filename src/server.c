
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
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cmpp.h>
#include "server.h"
#include "utils.h"
#include "config.h"
#include "account.h"
#include "company.h"
#include "gateway.h"
#include "template.h"

static lamb_db_t db;
static lamb_list_t list;
static lamb_cache_t cache;
static lamb_config_t config;
static lamb_account_t *accounts[LAMB_MAX_CLIENT];
static lamb_company_t *companys[LAMB_MAX_COMPANY];
static lamb_gateway_t *gateways[LAMB_MAX_GATEWAY];
static lamb_template_t *templates[LAMB_MAX_TEMPLATE];
static lamb_route_t *routes[LAMB_MAX_ROUTE];
static lamb_account_queue_t *cli_queue[LAMB_MAX_CLIENT];
static lamb_gateway_queue_t *gw_queue[LAMB_MAX_GATEWAY];

int main(int argc, char *argv[]) {
    char *file = "server.conf";

    int opt = 0;
    char *optstring = "c:";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'c':
            file = optarg;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    /* Read lamb configuration file */
    if (lamb_read_config(&config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (config.daemon) {
        //lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal();

    /* Start main event thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;
    pid_t pid;

    /* fetch all account */
    memset(accounts, 0, sizeof(accounts));
    err = lamb_account_get_all(&db, accounts, LAMB_MAX_CLIENT);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch account information");
        return;
    }

    /* fetch all company */
    memset(companys, 0, sizeof(companys));
    err = lamb_company_get_all(&db, companys, LAMB_MAX_COMPANY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch company information");
        return;
    }

    /* fetch all group */
    //lamb_group_get_all(&db, groups);

    /* fetch all gateway */
    memset(gateways, 0, sizeof(gateways));
    err = lamb_gateway_get_all(&db, gateways, LAMB_MAX_GATEWAY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch gateway information");
        return;
    }

    /* Open all gateway queue */
    memset(gw_queue, 0, sizeof(gw_queue));    
    err = lamb_gateway_queue_open(gw_queue, LAMB_MAX_GATEWAY, gateways, LAMB_MAX_GATEWAY);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all gateway queue");
        return;
    }

    /* Open all client queue */
    err = lamb_account_queue_open(cli_queue, LAMB_MAX_ACCOUNT, accounts, LAMB_MAX_ACCOUNT);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all client queue failed");
        return;
    }
    
    /* Start sender process */
    for (int i = 0; i < config.sender; i++) {
        pid = fork();
        if (pid < 0) {
            lamb_errlog(config.logfile, "Can't fork sender child process", 0);
            return;
        } else if (pid == 0) {
            lamb_sender_loop();
            return;
        }
    }

    /* Start deliver process */
    for (int i = 0; i < config.deliver; i++) {
        pid = fork();
        if (pid < 0) {
            lamb_errlog(config.logfile, "Can't fork deliver child process", 0);
            return;
        } else if (pid == 0) {
            lamb_deliver_loop();
            return;
        }
    }

    while (true) {
        sleep(3);
    }

    return;
}

void lamb_sender_loop(void) {
    int err;

    /* Redis Initialization */
    err = lamb_cache_connect(&cache, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis server", 0);
        return;
    }

    /* client queue fd */
    int ret;
    int epfd, nfds;
    struct epoll_event ev, events[32];
    lamb_submit_t message;
    lamb_list_t list;

    epfd = epoll_create1(0);
    err = lamb_account_epoll_add(epfd, &ev, cli_queue, LAMB_MAX_ACCOUNT, LAMB_QUEUE_SEND);
    if (err) {
        lamb_errlog(config.logfile, "Lamb epoll add client sender queue failed", 0);
        return;
    }
    
    /* Start worker threads */
    lamb_send_worker(list, config.work_threads);

    /* Read queue message */
    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if (events[i].events & EPOLLIN) {
                    if (list->len < config.queue) {
                        ret = mq_receive(events[i].data.fd, message, sizeof(message), 0);
                        if (ret > 0) {
                            lamb_list_rpush(list, message);
                        }
                    } else {
                        lamb_sleep(500);
                    }
                }
            }
        }
    }
}

void lamb_deliver_loop(void) {
    int err;

    /* Postgresql Database Initialization */
    lamb_db_init(&db);
    err = lamb_db_connect(&db, config.db_host, config.db_port, config.db_user, config.db_password, config.db_name);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to postgresql database", 0);
        return;
    }

    /* client queue fd */
    int ret;
    int epfd, nfds;
    struct epoll_event ev, events[32];
    lamb_submit_t message;
    lamb_list_t list;

    lamb_epoll_add(ev, (void *)gw_queue, LAMB_GATEWAY);
    
    /* Start worker threads */
    lamb_deliver_worker(list, config.work_threads);

    /* Read queue message */
    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if (events[i].events & EPOLLIN) {
                    if (list->len < config.queue) {
                        ret = mq_receive(events[i].data.fd, message, sizeof(message), 0);
                        if (ret > 0) {
                            lamb_list_rpush(list, message);
                        }
                    } else {
                        lamb_sleep(500);
                    }
                }
            }
        }
    }
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

    if (lamb_get_bool(&cfg, "Daemon", &conf->daemon) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Daemon' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Sender", &conf->sender) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Sender' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Deliver", &conf->deliver) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Deliver' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "WorkThreads", &conf->work_threads) != 0) {
        fprintf(stderr, "ERROR: Can't read 'WorkThreads' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "SenderQueue", &conf->sender_queue) != 0) {
        fprintf(stderr, "ERROR: Can't read 'SenderQueue' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "DeliverQueue", &conf->deliver_queue) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DeliverQueue' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'LogFile' parameter\n");
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

    if (lamb_get_string(&cfg, "RedisPassword", conf->redis_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisPassword' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisDb' parameter\n");
        goto error;
    }

    if (conf->redis_db < 0 || conf->redis_db > 65535) {
        fprintf(stderr, "ERROR: Invalid redis database name\n");
        goto error;
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

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}

int lamb_check_template(char *pattern, char *message, int len) {
    int rc;
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[510];

    re = pcre_compile(pattern, 0, &error, &erroffset, NULL);
    if (re == NULL) {
        return -1;
    }

    rc = pcre_exec(re, NULL, message, len, 0, 0, ovector, 510);
    if (rc < 0) {
        pcre_free(re);
        return -1;
    }

    pcre_free(re);
    return 0;
}
