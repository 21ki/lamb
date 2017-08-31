
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
#include "list.h"
#include "config.h"
#include "account.h"
#include "company.h"
#include "gateway.h"
#include "template.h"
#include "group.h"


static lamb_cache_t cache;
static lamb_keyword_t keys;
static lamb_config_t config;
static lamb_gateway_t *gateways[LAMB_MAX_GATEWAY];
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
    lamb_db_t db;
    lamb_account_t *accounts[LAMB_MAX_CLIENT];

    /* Postgresql Database  */
    err = lamb_db_init(&db);
    if (err) {
        lamb_errlog(config.logfile, "Can't initialize postgresql database handle", 0);
        return;
    }

    /* fetch all account */
    memset(accounts, 0, sizeof(accounts));
    err = lamb_account_get_all(&db, accounts, LAMB_MAX_CLIENT);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch account information", 0);
        return;
    }

    /* Start work process */
    for (int i = 0; (i < LAMB_MAX_CLIENT) && (accounts[i] != NULL); i++) {
        pid = fork();
        if (pid < 0) {
            lamb_errlog(config.logfile, "Can't fork id %d work child process", accounts[i]->id);
            return;
        } else if (pid == 0) {
            lamb_work_loop(accounts[i]);
            return;
        }
    }

    /* Master process event monitor */
    lamb_set_process("lamb_server");
    
    while (true) {
        sleep(3);
    }

    return;
}

void lamb_work_loop(lamb_account_t *account) {
    int err, ret;
    lamb_list_t *queue;
    lamb_group_t group;
    lamb_queue_t client;
    lamb_company_t company;
    lamb_templates_t template;
    lamb_work_object_t object;
    
    lamb_set_process("lamb-workd");

    /* Redis Initialization */
    err = lamb_cache_connect(&cache, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis server", 0);
        return;
    }

    /* Work Queue Initialization */
    queue = lamb_list_new();
    if (queue == NULL) {
        lamb_errlog(config.logfile, "Lamb work queue initialization failed ", 0);
        return;
    }
    
    /* Open Client Queue */
    char name[128];
    lamb_queue_t queue;
    lamb_queue_opt opt;
    struct mq_attr attr;
    
    opt.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    attr.mq_maxmsg = config.client_queue;
    attr.mq_msgsize = sizeof(lamb_message_t);
    opt.attr = &attr;

    sprintf(name, "/cli.%d.message", account->id);
    err = lamb_queue_open(&client, name, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s client queue", name);
        return;
    }

    /* Fetch company information */
    err = lamb_company_get(&db, account->company, &company);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch id %d company information", account->company);
        return;
    }

    /* Fetch template information */
    err = lamb_template_get_all(&db, account->id, &template, LAMB_MAX_TEMPLATE);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch template information", 0);
        return;
    }

    /* Fetch all gateway */
    memset(gateways, 0, sizeof(gateways));
    err = lamb_gateway_get_all(&db, gateways, LAMB_MAX_GATEWAY);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch gateway information", 0);
        return;
    }

    /* Open all gateway queue */
    lamb_queue_opt gopt;

    gopt.flag = O_WRONLY | O_NONBLOCK;
    gopt.attr = NULL;
    
    memset(gw_queue, 0, sizeof(gw_queue));
    err = lamb_gateway_queue_open(&gw_queue, LAMB_MAX_GATEWAY, gateways, LAMB_MAX_GATEWAY, &gopt, LAMB_QUEUE_SEND);
    if (err) {
        lamb_errlog(config.logfile, "Can't open all gateway queue", 0);
        return;
    }

    err = lamb_group_get(&db, account->route, &group);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch group information", 0);
        return;
    }

    /* Start sender worker threads */
    object.queue = queue;
    object.client = &client;
    object.group = &group;
    object.account = account;
    object.company = &company;
    object.template = &template;
    
    lamb_start_thread(lamb_worker_loop, (void *)&object, config.work_threads);

    /* Read queue message */
    lamb_message_t *message;

    while (true) {
        message = (lamb_message_t *)calloc(1 , sizeof(lamb_message_t));
        if (!message) {
            lamb_errlog(config.logfile, "Lamb message queue failed to allocate memory", 0);
            lamb_sleep(1000);
            continue;
        }

        ret = lamb_queue_receive(&client, (char *)message, sizeof(lamb_message_t), 0);
        if (ret < 1) {
            free(message);
            continue;
        }

        while (true) {
            if (queue->len < config.work_queue) {
                pthread_mutex_lock(&queue->lock);
                node = lamb_list_rpush(queue, lamb_list_node_new(message));
                pthread_mutex_unlock(&queue->lock);
                break;
            }

            lamb_sleep(10);
        }

    }
}

void *lamb_worker_loop(void *data) {
    int err;
    lamb_list_t *queue;
    lamb_group_t *group;
    lamb_queue_t *client;
    lamb_account_t *account;
    lamb_company_t *company;
    lamb_templates_t *template;
    lamb_work_object_t *object;
    lamb_message_t *message;
    lamb_submit_t *submit;

    object = (lamb_work_object_t *)data;
    queue = object->queue;
    client = object->client;
    account = object->account;
    company = object->company;
    template = object->template;
    group = object->group;
    
    while (true) {
        lamb_list_node_t *node;

        pthread_mutex_lock(&queue->lock);
        node = lamb_list_lpop(queue, (char *)&message, sizeof(lamb_message_t), 0);
        pthread_mutex_unlock(&queue->lock);

        if (node != NULL) {
            message = (lamb_message_t *)node->val;
            submit = (lamb_submit_t *)&message->data;

            /* 
               printf("-> id: %llu, ", msg->id);
               printf("phone: %s, ", msg->phone);
               printf("spcode: %s, ", msg->spcode);
               printf("content: %s\n, ", msg->content);
            */

            /* Blacklist and Whitelist */
            if (account->policy != LAMB_POL_EMPTY) {
                if (lamb_security_check(seclist, account->policy, submit->phone)) {
                    continue;
                }
            }

            /* SpCode Processing  */
            if (account->extended) {
                lamb_account_spcode_process(account->spcode, submit->spcode, 20);
            } else {
                strcpy(submit->spcode, account->spcode);
            }

            /* Template Processing */
            if (account->check_template) {
                if (!lamb_template_check(template, submit->content, submit->length)) {
                    continue;
                }
            }

            /* Keywords Filtration */
            if (account->check_keyword) {
                if (lamb_keyword_check(&keys, submit->content)) {
                    continue;
                }
            }

            /* Routing Scheduling */
            gw = lamb_route_schedul(account->route, group);

            err = lamb_queue_send(gw_queue[gw]->send, (char *)&message, sizeof(message), 0);
            if (err) {
                lamb_errlog(config.logfile, "Can't write message id to redis database", 0);
            }

            /* Write Message Id to Cache */
            err = lamb_write_msgid(&cache, msg.id);
            if (err) {
                lamb_errlog(config.logfile, "Can't write message id to redis database", 0);
            }

            /* Billing and Billing Methods */
            if (account->charge_type == LAMB_CHARGE_SUBMIT) {
                lamb_company_billing(company->id, 1);
            }
        }
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

    if (lamb_get_int(&cfg, "Queue", &conf->queue) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Queue' parameter\n");
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


