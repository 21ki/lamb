
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
#include "list.h"
#include "config.h"
#include "account.h"
#include "company.h"
#include "gateway.h"
#include "template.h"
#include "group.h"
#include "message.h"

static lamb_db_t db;
static lamb_db_t mdb;
static lamb_cache_t rdb;
static lamb_caches_t cache;
static lamb_list_t *queue;
static lamb_list_t *storage;
static lamb_list_t *delivery;
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
    lamb_signal_processing();
    
    /* Start main event thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;
    lamb_queue_opt opt;
    struct mq_attr attr;
    
    lamb_set_process("lamb-deliverd");

    /* Work Queue Initialization */
    queue = lamb_list_new();
    if (!queue) {
        lamb_errlog(config.logfile, "Lmab work queue initialization failed", 0);
        return;
    }

    /* Storage Queue Initialization */
    storage = lamb_list_new();
    if (!queue) {
        lamb_errlog(config.logfile, "Lmab storage queue initialization failed", 0);
        return;
    }

    /* Redis Database Initialization */
    err = lamb_cache_connect(&rdb, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis database", 0);
        return;
    }

    /* Cache Node Initialization */
    memset(&cache, 0, sizeof(cache));
    lamb_nodes_connect(&cache, LAMB_MAX_CACHE, config.nodes, 7);
    if (cache.len < 1) {
        lamb_errlog(config.logfile, "Can't connect to cache node server", 0);
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

    /* Postgresql Database Initialization */
    err = lamb_db_init(&mdb);
    if (err) {
        lamb_errlog(config.logfile, "Can't initialize message database handle", 0);
        return;
    }

    err = lamb_db_connect(&mdb, config.msg_host, config.msg_port, config.msg_user, config.msg_password, config.msg_name);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to message database", 0);
        return;
    }

    /* Fetch All Account */
    memset(&accounts, 0, sizeof(accounts));
    err = lamb_account_get_all(&db, &accounts, LAMB_MAX_CLIENT);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch account information", 0);
        return;
    }

    /* Fetch All Company */
    memset(&companys, 0, sizeof(companys));
    err = lamb_company_get_all(&db, &companys, LAMB_MAX_COMPANY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch company information", 0);
        return;
    }

    /* Fetch All Gateway */
    memset(&gateways, 0, sizeof(gateways));
    err = lamb_gateway_get_all(&db, &gateways, LAMB_MAX_GATEWAY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch gateway information", 0);
        return;
    }

    /* Open All Client Queue */
    opt.flag = O_WRONLY | O_NONBLOCK;
    attr.mq_maxmsg = 5;
    attr.mq_msgsize = sizeof(lamb_message_t);
    opt.attr = &attr;

    memset(&cli_queue, 0, sizeof(cli_queue));
    err = lamb_account_queue_open(&cli_queue, LAMB_MAX_CLIENT, &accounts, LAMB_MAX_CLIENT, &opt, LAMB_QUEUE_RECV);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all client queue failed", 0);
        return;
    }

    /* Open All Gateway Queue */
    opt.flag = O_RDWR | O_NONBLOCK;
    opt.attr = NULL;
    
    memset(&gw_queue, 0, sizeof(gw_queue));
    err = lamb_gateway_queue_open(&gw_queue, LAMB_MAX_GATEWAY, &gateways, LAMB_MAX_GATEWAY, &opt, LAMB_QUEUE_RECV);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all gateway queue", 0);
        return;
    }

    ssize_t ret;
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    err = lamb_gateway_epoll_add(epfd, &ev, &gw_queue, LAMB_MAX_GATEWAY, LAMB_QUEUE_RECV);
    if (err) {
        lamb_errlog(config.logfile, "Lamb epoll add delvier queue failed", 0);
        return;
    }
    
    /* Start sender worker threads */
    lamb_start_thread(lamb_deliver_worker, NULL, config.work_threads);

    /* Start sender worker threads */
    lamb_start_thread(lamb_report_message, NULL, config.work_threads);

    /* Read queue message */
    lamb_list_node_t *node;
    lamb_message_t *message;

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if ((events[i].events & EPOLLIN) && (queue->len < config.work_queue)) {
                    message = (lamb_message_t *)calloc(1, sizeof(lamb_message_t));
                    if (!message) {
                        lamb_errlog(config.logfile, "Lamb can't allocate memory for message", 0);
                        lamb_sleep(3000);
                        continue;
                    }

                    ret = mq_receive(events[i].data.fd, (char *)message, sizeof(lamb_message_t), 0);
                    if (ret > 0) {
                        pthread_mutex_lock(&queue->lock);
                        node = lamb_list_rpush(queue, lamb_list_node_new(message));
                        pthread_mutex_unlock(&queue->lock);
                        if (node == NULL) {
                            lamb_errlog(config.logfile, "Lamb can't allocate memory for queue", 0);
                            lamb_sleep(3000);
                        }
                    }
                }
            }
        }
    }

    return;
}

void *lamb_deliver_worker(void *data) {
    int i, err;
    int account;
    int company;
    int charge;
    char spcode[24];
    long long money;
    char key[64];
    lamb_report_t *report;
    lamb_list_node_t *node;
    lamb_message_t *message;
    lamb_deliver_t *deliver;

    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }
    
    while (true) {
        pthread_mutex_lock(&queue->lock);
        node = lamb_list_lpop(queue);
        pthread_mutex_unlock(&queue->lock);
        
        if (!node) {
            lamb_sleep(10);
            continue;
        }

        message = (lamb_message_t *)node->val;

        switch (message->type) {
        case LAMB_REPORT:
            report = (lamb_report_t *)&message->data;
            printf("-> [report] msgId: %llu, phone: %s, status: %s, submitTime: %s, doneTime: %s\n",
                   report->id, report->phone, report->status, report->submitTime, report->doneTime);

            pthread_mutex_lock(&storage->lock);
            node = lamb_list_rpush(storage, node);
            pthread_mutex_unlock(&storage->lock);

            if (node == NULL) {
                lamb_errlog(config.logfile, "Memory cannot be allocated for the storage queue", 0);
                lamb_sleep(3000);
            }

            /* Fetch Cache Message Information */
            memset(key, 0, sizeof(key));
            sprintf(key, "%llu", report->id);
            i = (submit->id % cache.len);
            err = lamb_get_cache_message(cache.nodes[i], key, &account, &company, &charge, report->spcode, 24);
            if (err) {
                break;
            }

            /* Deduction For Account */
            if ((charge == LAMB_CHARGE_SUCCESS) && (report->status == 1)) {
                pthread_mutex_lock(&(rdb.lock));
                lamb_company_billing(&rdb, company, -1, &money);
                pthread_mutex_unlock(&(rdb.lock));
                if (err) {
                    lamb_errlog(config.logfile, "Lamb account %d for company %d chargeback failure", account, company);
                    lamb_sleep(3000);
                }
            }

            /* Status Report Delivery to Client*/
            for (int j = 0; j < cli_queue.len; j++) {
                if (cli_queue.list[j]->id == account) {
                    err = lamb_queue_send(&(cli_queue.list[j]->queue), (const char *)&message, sizeof(message), 0);
                    if (err) {
                        
                    }
                    break;
                }
            }

            break;
        case LAMB_DELIVER:
            deliver = (lamb_deliver_t *)&message->data;
            printf("-> [deliver] msgId: %llu, phone: %s, spcode: %s, serviceId: %s, msgFmt: %d, length: %d\n",
                   deliver->id, deliver->phone, deliver->spcode, deliver->serviceId, deliver->msgFmt, deliver->length);

            /*
              int id;
              id = lamb_route_query(&routes, deliver->spcode);
              if (lamb_is_online(&cache, id)) {
              lamb_queue_send(&(cli_queue.list[id]->queue), (char *)message, sizeof(lamb_message_t), 0);
              } else {
              lamb_save_deliver(&db, deliver);
              }
            */
            break;
        }

        free(node);
        free(message);
    }

    pthread_exit(NULL);
}

bool lamb_is_online(lamb_cache_t *cache, int id) {
    printf("-> lamb_is_online()\n");
    return true;
}

void *lamb_report_loop(void *data) {
    int err;
    lamb_report_t *report;
    lamb_message_t *message;

    while (true) {
        pthread_mutex_lock(&storage->lock);
        node = lamb_list_lpop(storage);
        pthread_mutex_unlock(&storage->lock);

        if (node == NULL) {
            lamb_sleep(10);
            continue;
        }

        /* Write Report to Database */
        message = (lamb_message_t *)node->val;
        report = (lamb_report_t *)&(message->data);

        /* Write Message to Database */
        err = lamb_update_message(&mdb, report);
        if (err) {
            lamb_errlog(config.logfile, "Write report to message database failure", 0);
            lamb_sleep(1000);
        }

        free(node);
        free(message);
    }
    
    pthread_exit(NULL);
}

int lamb_save_deliver(lamb_db_t *db, lamb_deliver_t *deliver) {
    printf("-> lamb_save_deliver()\n");
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

    if (lamb_get_bool(&cfg, "Daemon", &conf->daemon) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Daemon' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "WorkQueue", &conf->work_queue) != 0) {
        fprintf(stderr, "ERROR: Can't read 'WorkQueue' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "WorkThreads", &conf->work_threads) != 0) {
        fprintf(stderr, "ERROR: Can't read 'WorkThreads' parameter\n");
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

    if (lamb_get_string(&cfg, "CachePassword", conf->cache_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'CachePassword' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "CacheDb", &conf->cache_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'CacheDb' parameter\n");
        goto error;
    }

    if (conf->cache_db < 0 || conf->cache_db > 65535) {
        fprintf(stderr, "ERROR: Invalid cache database name\n");
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

    char node[32];
    memset(node, 0, sizeof(node));

    if (lamb_get_string(&cfg, "node1", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node1' parameter\n");
        goto error;
    }
    conf->nodes[0] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node2", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node2' parameter\n");
        goto error;
    }
    conf->nodes[1] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node3", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node3' parameter\n");
        goto error;
    }
    conf->nodes[2] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node4", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node4' parameter\n");
        goto error;
    }
    conf->nodes[3] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node5", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node5' parameter\n");
        goto error;
    }
    conf->nodes[4] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node6", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node1' parameter\n");
        goto error;
    }
    conf->nodes[5] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node7", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node7' parameter\n");
        goto error;
    }
    conf->nodes[6] = lamb_strdup(node);

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}


