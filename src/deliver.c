
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
#include "deliver.h"
#include "utils.h"
#include "list.h"
#include "config.h"
#include "account.h"
#include "company.h"
#include "gateway.h"
#include "template.h"
#include "group.h"

static lamb_db_t db;
static lamb_list_t *queue;
static lamb_cache_t cache;
static lamb_config_t config;
static lamb_routes_t routes;
static lamb_accounts_t accounts;
static lamb_companys_t companys;
static lamb_gateways_t gateways;
static lamb_account_queues_t cli_queue;
static lamb_gateway_queues_t gw_queue;

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
    lamb_queue_opt opt;
    struct mq_attr attr;
    
    lamb_set_process("lamb_deliverd");

    /* Work Queue Initialization */
    queue = lamb_list_new();
    if (!queue) {
        lamb_errlog(config.logfile, "Lmab work queue initialization failed", 0);
        return;
    }
    
    /* Postgresql Database Initialization */
    err = lamb_db_init(&db);
    if (err) {
        lamb_errlog(config.logfile, "Can't initialize postgresql database handle", 0);
        return;
    }

    err = lamb_db_connect(&db, config.db_host, config.db_port, config.db_user, config.db_password, config.db_name);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to postgresql database", 0);
        return;
    }
    
    /* Fetch All Account */
    memset(accounts, 0, sizeof(accounts));
    err = lamb_account_get_all(&db, accounts, LAMB_MAX_CLIENT);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch account information", 0);
        return;
    }

    /* Fetch All Company */
    memset(companys, 0, sizeof(companys));
    err = lamb_company_get_all(&db, companys, LAMB_MAX_COMPANY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch company information", 0);
        return;
    }

    /* Fetch All Gateway */
    memset(gateways, 0, sizeof(gateways));
    err = lamb_gateway_get_all(&db, gateways, LAMB_MAX_GATEWAY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch gateway information", 0);
        return;
    }

    /* Open All Client Queue */
    opt.flag = O_CREAT | O_WRONLY | O_NONBLOCK;
    attr.mq_maxmsg = 5;
    attr.mq_msgsize = sizeof(lamb_message_t);
    opt.attr = &attr;

    memset(cli_queue, 0, sizeof(cli_queue));
    err = lamb_account_queue_open(cli_queue, LAMB_MAX_CLIENT, accounts, LAMB_MAX_CLIENT, &sopt, &ropt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all client queue failed", 0);
        return;
    }

    /* Open All Gateway Queue */
    opt.flag = O_RDWR | O_NONBLOCK;
    opt.attr = NULL;
    
    memset(gw_queue, 0, sizeof(gw_queue));
    err = lamb_gateway_queue_open(gw_queue, LAMB_MAX_GATEWAY, gateways, LAMB_MAX_GATEWAY, &sopt, &ropt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all gateway queue", 0);
        return;
    }

    ssize_t ret;
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    err = lamb_gateway_epoll_add(epfd, &ev, gw_queue, LAMB_MAX_GATEWAY, LAMB_QUEUE_RECV);
    if (err) {
        lamb_errlog(config.logfile, "Lamb epoll add delvier queue failed", 0);
        return;
    }
    
    /* Start sender worker threads */
    lamb_start_thread(lamb_deliver_worker, NULL, config.work_threads);

    /* Read queue message */
    lamb_message_t *message;

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if ((events[i].events & EPOLLIN) && (queue->len < config.work_queue)) {
                    message = (lamb_message_t *)calloc(1, sizeof(lamb_message_t));
                    if (!message) {
                        lamb_errlog(config.logfile, "Lamb message queue failed to allocate memory", 0);
                        continue;
                    }

                    ret = mq_receive(events[i].data.fd, (char *)message, sizeof(lamb_message_t), 0);
                    if (ret > 0) {
                        lamb_queue_send(&queue, (char *)message, sizeof(lamb_message_t), 0);
                    }
                }
            }
        }
    }

    return;
}

void *lamb_deliver_worker(void *data) {
    int err;
    lamb_list_node_t *node;
    lamb_message_t *message;
    lamb_update_t *update;
    lamb_report_t *report;
    lamb_deliver_t *deliver;

    while (true) {
        pthread_mutex_lock(&queue->lock);
        node = lamb_list_lpop(queue);
        pthread_mutex_unlock(&queue->lock);
        
        if (!node) {
            lamb_sleep(10);
            continue;
        }

        message = (lamb_message_t *)node->val;
        switch (message.type) {
        case LAMB_UPDATE:
            update = (lamb_update_t *)&(message->data);
            err = lamb_update_msgid(&cache, update->id, update->msgId);
            if (err) {
                lamb_errlog(config.logfile, "Can't write update messsage id", 0);
            }
            break;
        case LAMB_REPORT:
            report = (lamb_report_t *)&(message->data);
            err = lamb_report_update(&db, report);
            if (err) {
                lamb_errlog(config.logfile, "Can't update message status report", 0);
            }
            break;
        case LAMB_DELIVER:;
            deliver = (lamb_deliver_t *)&(message->data);
            id = lamb_route_query(&cache, deliver->spcode);
            if (lamb_is_online(&cache, id)) {
                lamb_queue_send(cli_queue[id], (char *)message, sizeof(lamb_message_t));
            } else {
                lamb_save_deliver(&db, deliver);
            }
            break;
        }

        free(node);
        free(message);
    }
    
    
    pthread_exit(NULL);
}

int lamb_update_msgid(lamb_cache_t *cache, unsigned long long id, unsigned long long msgId) {
    redisReply *reply;

    reply = redisCommand(cache->handle, "SET %llu %llu", id , msgId);
    if (reply == NULL) {
        return -1;
    }

    freeReplyObject(reply);
    return 0;
}

int lamb_report_update(lamb_db_t *db, lamb_report_t *report) {
    return 0;
}

int lamb_save_deliver(lamb_db_t *db, lamb_deliver_t *deliver) {
    return 0;
}

bool lamb_is_online(lamb_cache_t *cache, int id) {
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


