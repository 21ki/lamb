
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
#include <signal.h>
#include <errno.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <cmpp.h>
#include "server.h"
#include "utils.h"
#include "config.h"
#include "keyword.h"
#include "security.h"
#include "message.h"

static int aid = 0;
static lamb_keywords_t keys;
static lamb_config_t config;

int main(int argc, char *argv[]) {
    bool daemon = false;
    char *file = "server.conf";

    int opt = 0;
    char *optstring = "a:c:d";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'a':
            aid = atoi(optarg);
        case 'c':
            file = optarg;
            break;
        case 'd':
            daemon = true;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    /* Read lamb configuration file */
    if (lamb_read_config(&config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (daemon) {
        lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal_processing();
    
    /* Start main event thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err, ret;
    lamb_cache_t rdb;
    lamb_cache_t cache;
    lamb_list_t *queue;
    lamb_list_t *storage;
    lamb_queues_t channel;
    lamb_group_t group;
    lamb_account_t account;
    lamb_company_t company;
    lamb_templates_t template;

    lamb_set_process("lamb-server");

    err = lamb_signal(SIGHUP, lamb_reload);
    if (err) {
        printf("-> [signal] Can't setting SIGHUP signal to lamb_reload()\n");
    }

    /* Work Queue Initialization */
    queue = lamb_list_new();
    if (queue == NULL) {
        lamb_errlog(config.logfile, "Lamb work queue initialization failed ", 0);
        return;
    }

    /* Storage Queue Initialization */
    storage = lamb_list_new();
    if (storage == NULL) {
        lamb_errlog(config.logfile, "Lamb storage queue initialization failed ", 0);
        return;
    }

    /* Redis Initialization */
    err = lamb_cache_connect(&rdb, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis server", 0);
        return;
    }

    /* Cache Initialization */
    err = lamb_cache_connect(&cache, config.cache_host, config.cache_port, NULL, config.cache_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to cache server", 0);
        return;
    }

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
    
    /* fetch  account */
    memset(&account, 0, sizeof(account));
    err = lamb_account_fetch(&db, aid, &account);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch account information", 0);
        return;
    }

    /* Open Client Queue */
    char name[128];
    lamb_queue_t client;
    lamb_queue_opt opt;
    struct mq_attr attr;
    
    opt.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    attr.mq_maxmsg = config.queue;
    attr.mq_msgsize = sizeof(lamb_message_t);
    opt.attr = &attr;

    sprintf(name, "/cli.%d.message", account->id);
    err = lamb_queue_open(&client, name, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s client queue", name);
        return;
    }

    /* Fetch company information */
    memset(&company, 0, sizeof(company));
    err = lamb_company_get(&db, account->company, &company);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch id %d company information", account->company);
        return;
    }

    /* Fetch template information */
    memset(&template, 0, sizeof(template));
    if () {

    }
    err = lamb_template_get_all(&db, account->id, &template, LAMB_MAX_TEMPLATE);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch template information", 0);
        return;
    }

    /* Fetch group information */
    memset(&group, 0, sizeof(group));
    err = lamb_group_get(&db, account->route, &group, LAMB_MAX_CHANNEL);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch group information", 0);
        return;
    }

    /* Fetch keyword information */
    memset(&keys, 0, sizeof(keys));
    if (account.check_keyword) {
        err = lamb_keyword_get_all(&db, &keys, LAMB_MAX_KEYWORD);
        if (err) {
            lamb_errlog(config.logfile, "Can't fetch keyword information", 0);
        }
    }
        
    /* Fetch Channel Queue*/
    lamb_queue_opt gopt;
    gopt.flag = O_WRONLY | O_NONBLOCK;
    gopt.attr = NULL;

    memset(&channel, 0, sizeof(channel));
    err = lamb_each_queue(&group, &gopt, &channel, LAMB_MAX_CHANNEL);
    if (err) {
        if (channel.len < 1) {
            return;
        }
    }

    /* Start sender worker threads */
    lamb_work_object_t object;

    object.db = &db;
    object.rdb = &rdb;
    object.cache = &cache;
    object.queue = queue;
    object.channel = &channel;
    object.storage = storage;
    object.account = account;
    object.company = &company;
    object.template = &template;
    
    lamb_start_thread(lamb_worker_loop, (void *)&object, config.work_threads);

    /* Start Billing and Update thread */
    lamb_start_thread(lamb_storage_billing, (void *)&object, 1);

    /* Read queue message */
    lamb_list_node_t *node;
    lamb_message_t *message;

    while (true) {
        message = (lamb_message_t *)calloc(1 , sizeof(lamb_message_t));
        if (!message) {
            lamb_errlog(config.logfile, "Lamb message queue allocate memory failed", 0);
            lamb_sleep(1000);
            continue;
        }

        ret = lamb_queue_receive(&client, (char *)message, sizeof(lamb_message_t), 0);
        if (ret < 1) {
            free(message);
            lamb_sleep(10);
            continue;
        }

        while (true) {
            if (queue->len < config.work_queue) {
                pthread_mutex_lock(&queue->lock);
                node = lamb_list_rpush(queue, lamb_list_node_new(message));
                pthread_mutex_unlock(&queue->lock);
                if (node == NULL) {
                    lamb_errlog(config.logfile, "Memory cannot be allocated for the workd queue", 0);
                    lamb_sleep(3000);
                }
                break;
            }

            lamb_sleep(10);
        }

    }

    return;
}

void lamb_reload(int signum) {
    if (signal(SIGHUP, lamb_reload) == SIG_ERR) {
        printf("-> [signal] Can't setting SIGHUP signal to lamb_reload()\n");
    }
    printf("lamb reload configuration file success!\n");
    return;
}

void lamb_work_loop(lamb_account_t *account) {

}

void *lamb_worker_loop(void *data) {
    int err;
    lamb_cache_t *cache;
    lamb_list_t *queue;
    lamb_queues_t *channel;
    //lamb_list_t *storage;
    lamb_account_t *account;
    lamb_templates_t *template;
    lamb_work_object_t *object;
    lamb_message_t *message;
    lamb_submit_t *submit;

    object = (lamb_work_object_t *)data;
    cache = object->cache;
    queue = object->queue;
    channel = object->channel;
    //storage = object->storage;
    account = object->account;
    template = object->template;
    
    while (true) {
        lamb_list_node_t *node;

        pthread_mutex_lock(&queue->lock);
        node = lamb_list_lpop(queue);
        pthread_mutex_unlock(&queue->lock);

        if (node != NULL) {
            message = (lamb_message_t *)node->val;
            submit = (lamb_submit_t *)&message->data;

            printf("-> id: %llu, phone: %s, spcode: %s, content: %s, length: %d\n", submit->id, submit->phone, submit->spcode, submit->content, submit->length);

            /* Check Message Encoded Convert */
            int codeds[] = {0, 8, 11, 15};
            if (!lamb_check_msgfmt(submit->msgFmt, codeds, sizeof(codeds) / sizeof(int))) {
                continue;
            }

            char *fromcode;
            char content[256];

            if (submit->msgFmt == 0) {
                fromcode = "ASCII";
            } else if (submit->msgFmt == 8) {
                fromcode = "UCS-2";
            } else if (submit->msgFmt == 11) {
                fromcode = NULL;
            } else if (submit->msgFmt == 15) {
                fromcode = "GBK";
            } else {
                printf("-> [encoded] message encoded %d not support\n", submit->msgFmt);
                continue;
            }
            
            if (fromcode != NULL) {
                memset(content, 0, sizeof(content));
                err = lamb_encoded_convert(submit->content, submit->length, content, sizeof(content), fromcode, "UTF-8", &submit->length);
                if (err || (submit->length == 0)) {
                    printf("-> [encoded] Message encoding conversion failed\n");
                    continue;
                }

                submit->msgFmt = 11;
                memset(submit->content, 0, 160);
                memcpy(submit->content, content, submit->length);
                printf("-> [encoded] Message encoding conversion successfull\n");
            }

            printf("-> id: %llu, phone: %s, spcode: %s, msgFmt: %d, content: %s, length: %d\n", submit->id, submit->phone, submit->spcode, submit->msgFmt, submit->content, submit->length);
                        
            /* Blacklist and Whitelist */
            if (account->policy != LAMB_POL_EMPTY) {
                if (lamb_security_check(cache, account->policy, submit->phone)) {
                    printf("-> [policy] The security check will not pass\n");
                    continue;
                }
                printf("-> [policy] The Security check OK\n");
            }

            /* SpCode Processing  */
            printf("-> [spcode] check message spcode extended\n");
            if (account->extended) {
                lamb_account_spcode_process(account->spcode, submit->spcode, 20);
            } else {
                strcpy(submit->spcode, account->spcode);
            }

            /* Template Processing */
            
            if (account->check_template) {
                if (!lamb_template_check(template, submit->content, submit->length)) {
                    printf("-> [template] The template check will not pass\n");
                    continue;
                }
                printf("-> [template] The template check OK\n");
            }

            /* Keywords Filtration */
            if (account->check_keyword) {
                if (lamb_keyword_check(&keys, submit->content)) {
                    printf("-> [keyword] The keyword check will not pass\n");
                    continue;
                }
                printf("-> [keyword] The keyword check OK\n");
            }


            if (channel->len < 1) {
                printf("-> [channel] No gateway channels available");
                lamb_errlog(config.logfile, "Lamb No gateway channels available", 0);
                lamb_sleep(3000);
            }

            /* Routing Scheduling */
            for (int i = 0; i < channel->len; i++) {
                err = lamb_queue_send(channel->queues[i], (char *)message, sizeof(lamb_message_t), 0);
                if (!err) {
                    break;
                }

                if ((i + 1) == channel->len) {
                    i = 0;
                }
            }

            /* Send Message to Storage */
            pthread_mutex_lock(&storage->lock);
            node = lamb_list_rpush(storage, node);
            pthread_mutex_unlock(&storage->lock);
            if (node == NULL) {
                lamb_errlog(config.logfile, "Memory cannot be allocated for the storage queue", 0);
                lamb_sleep(3000);
            }

            free(node);
            free(message);
        } else {
            lamb_sleep(10);
        }
    }

    pthread_exit(NULL);
}

void *lamb_storage_billing(void *data) {
    int err;
    lamb_db_t *db;
    lamb_cache_t *cache;
    lamb_list_t *storage;
    lamb_account_t *account;
    lamb_company_t *company;
    lamb_work_object_t *object;

    object = (lamb_work_object_t *)data;
    db = object->db;
    cache = object->cache;
    storage = object->storage;
    account = object->account;
    company = object->company;

    lamb_submit_t *submit;
    lamb_list_node_t *node;
    lamb_message_t *message;
    
    while (true) {
        pthread_mutex_lock(&storage->lock);
        node = lamb_list_lpop(storage);
        pthread_mutex_unlock(&storage->lock);

        if (node == NULL) {
            lamb_sleep(10);
            continue;
        }

        /* Billing Processing */
        if (account->charge_type == LAMB_CHARGE_SUBMIT) {
            err = lamb_company_billing(company->id, 1);
            if (err) {
                lamb_errlog(config.logfile, "Lamb Account %d chargeback failure", company->id);
            }
        }

        /* Write Message to Cache */
        message = (lamb_message_t *)node->val;
        submit = (lamb_submit_t *)&(message->data);
        err = lamb_cache_message(cache, account->id, company->id, submit);
        if (err) {
            lamb_errlog(config.logfile, "Write submit message to cache failure", 0);
            lamb_sleep(1000);
        }

        /* Write Message to Database */
        err = lamb_write_message(db, account->id, company->id, submit);
        if (err) {
            lamb_errlog(config.logfile, "Write submit message to database failure", 0);
            lamb_sleep(1000);
        }

        free(node);
        free(message);
        
    }

    pthread_exit(NULL);
}

int lamb_each_queue(lamb_group_t *group, lamb_queue_opt *opt, lamb_queues_t *list, int size) {
    int err;
    char name[128];
    lamb_queue_t *queue;

    list->len = 0;

    for (int i = 0, j = 0; (i < group->len) && (group->channels[i] != NULL) && (i < size); i++, j++) {
        queue = (lamb_queue_t *)calloc(1, sizeof(lamb_queue_t));
        if (queue != NULL) {
            sprintf(name, "/gw.%d.message", group->channels[i]->id);
            err = lamb_queue_open(queue, name, opt);
            if (err) {
                j--;
                lamb_errlog(config.logfile, "Can't open %s gateway queue", name);
                continue;
            }

            list->queues[j] = queue;
            list->len++;
        } else {
            return -1;
        }
        
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

    if (lamb_get_string(&cfg, "CacheHost", conf->cache_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'CacheHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "CachePort", &conf->cache_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'CachePort' parameter\n");
        goto error;
    }

    if (conf->cache_port < 1 || conf->cache_port > 65535) {
        fprintf(stderr, "ERROR: Invalid cache port number\n");
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

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}


