
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
#include <sys/prctl.h>
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
#include "group.h"

static lamb_db_t db;
static lamb_cache_t cache;
static lamb_config_t config;
static lamb_account_t *accounts[LAMB_MAX_CLIENT];
static lamb_company_t *companys[LAMB_MAX_COMPANY];
static lamb_group_t *groups[LAMB_MAX_GROUP];
static lamb_gateway_t *gateways[LAMB_MAX_GATEWAY];
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

    /* Postgresql Database  */
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
    
    /* fetch all account */
    memset(accounts, 0, sizeof(accounts));
    err = lamb_account_get_all(&db, accounts, LAMB_MAX_CLIENT);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch account information", 0);
        return;
    }

    /* fetch all company */
    memset(companys, 0, sizeof(companys));
    err = lamb_company_get_all(&db, companys, LAMB_MAX_COMPANY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch company information", 0);
        return;
    }

    /* fetch all gateway */
    memset(gateways, 0, sizeof(gateways));
    err = lamb_gateway_get_all(&db, gateways, LAMB_MAX_GATEWAY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch gateway information", 0);
        return;
    }

    /* fetch all group and channels */
    memset(groups, 0, sizeof(groups));
    err = lamb_group_get_all(&db, groups, LAMB_MAX_GROUP);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch group information", 0);
        return;
    }
    
    /* Open all gateway queue */
    /* 
       memset(gw_queue, 0, sizeof(gw_queue));    
       err = lamb_gateway_queue_open(gw_queue, LAMB_MAX_GATEWAY, gateways, LAMB_MAX_GATEWAY);
       if (err) {
       lamb_errlog(config.logfile, "Can't open all gateway queue", 0);
       return;
       }
    */
    
    /* Open all client queue */
    struct mq_attr sattr, rattr;
    lamb_queue_opt sopt, ropt;
    
    sopt.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    sattr.mq_maxmsg = config.queue;
    sattr.mq_msgsize = sizeof(lamb_message_t);
    sopt.attr = &sattr;

    ropt.flag = O_CREAT | O_WRONLY | O_NONBLOCK;
    rattr.mq_maxmsg = 3;
    rattr.mq_msgsize = sizeof(lamb_message_t);
    ropt.attr = &rattr;

    err = lamb_account_queue_open(cli_queue, LAMB_MAX_CLIENT, accounts, LAMB_MAX_CLIENT, &sopt, &ropt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all client queue failed", 0);
        return;
    }
    
    /* Start sender process */
    for (int i = 0; i < config.sender; i++) {
        pid = fork();
        if (pid < 0) {
            lamb_errlog(config.logfile, "Can't fork sender child process", 0);
            return;
        } else if (pid == 0) {
            prctl(PR_SET_NAME, "lamb-sender", 0, 0, 0);
            lamb_sender_loop();
            return;
        }
    }

    /* Start deliver process */
    /* 
       for (int i = 0; i < config.deliver; i++) {
       pid = fork();
       if (pid < 0) {
       lamb_errlog(config.logfile, "Can't fork deliver child process", 0);
       return;
       } else if (pid == 0) {
       prctl(PR_SET_NAME, "lamb-deliver", 0, 0, 0);
       lamb_deliver_loop();
       return;
       }
       }
    */

    prctl(PR_SET_NAME, "lamb-server", 0, 0, 0);
    
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
    ssize_t ret;
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    err = lamb_account_epoll_add(epfd, &ev, cli_queue, LAMB_MAX_CLIENT, LAMB_QUEUE_SEND);
    if (err) {
        lamb_errlog(config.logfile, "Lamb epoll add client sender queue failed", 0);
        return;
    }

    /* Create Sender Wrok Queue */
    lamb_queue_t queue;
    lamb_queue_opt opt;
    struct mq_attr attr;
    
    opt.flag = O_CREAT | O_WRONLY;
    attr.mq_maxmsg = config.sender_queue;
    attr.mq_msgsize = 10;
    opt.attr = &attr;

    err = lamb_queue_open(&queue, LAMB_SENDER_QUEUE, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open sender work queue", 0);
        return;
    }
    
    /* Start sender worker threads */
    lamb_start_worker(lamb_sender_worker, config.work_threads);

    lamb_message_t *message;
    message = malloc(sizeof(lamb_message_t));

    /* Read queue message */
    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if (events[i].events & EPOLLIN) {
                    memset(message, 0, sizeof(lamb_message_t));
                    ret = mq_receive(events[i].data.fd, (char *)message, sizeof(lamb_message_t), 0);
                    if (ret > 0) {
                        lamb_queue_send(&queue, (char *)message, sizeof(lamb_message_t), 0);
                    }
                }
            }
        }
    }
}

void lamb_deliver_loop(void) {
    int err;

    /* Redis Initialization */
    err = lamb_cache_connect(&cache, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis server", 0);
        return;
    }

    /* client queue fd */
    ssize_t ret;
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    err = lamb_gateway_epoll_add(epfd, &ev, gw_queue, LAMB_MAX_GATEWAY, LAMB_QUEUE_RECV);
    if (err) {
        lamb_errlog(config.logfile, "Lamb epoll add delvier queue failed", 0);
        return;
    }

    /* Create Sender Wrok Queue */
    lamb_queue_t queue;
    lamb_queue_opt opt;
    struct mq_attr attr;
    
    opt.flag = O_CREAT | O_RDWR;
    attr.mq_maxmsg = config.deliver_queue;
    attr.mq_msgsize = 512;
    opt.attr = &attr;

    err = lamb_queue_open(&queue, LAMB_DELIVER_QUEUE, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open deliver work queue", 0);
        return;
    }

    /* Start sender worker threads */
    lamb_start_worker(lamb_deliver_worker, config.work_threads);

    lamb_message_t *message;
    message = malloc(sizeof(lamb_message_t));

    /* Read queue message */
    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if (events[i].events & EPOLLIN) {
                    memset(message, 0, sizeof(lamb_message_t));
                    ret = mq_receive(events[i].data.fd, (char *)message, sizeof(lamb_message_t), 0);
                    if (ret > 0) {
                        lamb_queue_send(&queue, (char *)message, sizeof(lamb_message_t), 0);
                    }
                }
            }
        }
    }
}

void lamb_start_worker(void *(*func)(void *), int count) {
    int err;
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (int i = 0; i < count; i++) {
        err = pthread_create(&tid, &attr, func, NULL);
        if (err) {
            lamb_errlog(config.logfile, "Start work thread failed", 0);
            continue;
        }
    }

    pthread_attr_destroy(&attr);
    return;
}

void *lamb_sender_worker(void *val) {
    int err;
    ssize_t ret;
    lamb_queue_t queue;
    lamb_queue_opt opt;
    lamb_message_t *message;
    lamb_submit_t *msg;
    
    opt.flag = O_RDWR;
    opt.attr = NULL;
    err = lamb_queue_open(&queue, LAMB_SENDER_QUEUE, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open sender work queue", 0);
        goto exit;
    }

    message = (lamb_message_t *)malloc(sizeof(lamb_message_t));

    while (true) {
        memset(message, 0, sizeof(lamb_message_t));
        ret = lamb_queue_receive(&queue, (char *)message, sizeof(lamb_message_t), 0);
        if (ret > 0) {
            msg = (lamb_submit_t *)message->data;
            printf("id: %llu, ", msg->id);
            printf("phone: %s, ", msg->phone);
            printf("spcode: %s, ", msg->spcode);
            printf("content: %s\n, ", msg->content);
            printf("-----------------------------------------------------------------\n");
        }
    }
    

exit:
    pthread_exit(NULL);
    return NULL;
}

void *lamb_deliver_worker(void *val) {
    int err;
    ssize_t ret;
    lamb_queue_t queue;
    lamb_queue_opt opt;
    lamb_message_t *message;
    
    opt.flag = O_RDWR;
    opt.attr = NULL;
    err = lamb_queue_open(&queue, LAMB_DELIVER_QUEUE, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open deliver work queue", 0);
        goto exit;
    }

    message = (lamb_message_t *)malloc(sizeof(lamb_message_t));

    while (true) {
        memset(message, 0, sizeof(lamb_message_t));
        ret = lamb_queue_receive(&queue, (char *)message, sizeof(lamb_message_t), 0);
        if (ret > 0) {
            printf("-----------------------------------------------------------------\n");
        }
    }
    

exit:
    pthread_exit(NULL);
    return NULL;
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


