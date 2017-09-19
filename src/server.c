
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
#include <sched.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <cmpp.h>
#include "server.h"
#include "utils.h"
#include "config.h"
#include "keyword.h"
#include "security.h"
#include "message.h"

static int aid = 0;
static lamb_db_t db;
static lamb_db_t mdb;
static lamb_cache_t rdb;
static lamb_caches_t cache;
static lamb_cache_t black;
static lamb_queue_t client;
static lamb_list_t *queue;
static lamb_list_t *storage;
static lamb_queues_t channel;
static lamb_group_t group;
static lamb_account_t account;
static lamb_company_t company;
static lamb_templates_t template;
static lamb_keywords_t keywords;
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
            break;
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

    /* Resource limit processing */
    lamb_rlimit_processing();
    
    /* Start main event thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err, ret;

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

    /* Blacklist Cache Initialization */
    err = lamb_cache_connect(&black, config.black_host, config.black_port, NULL, config.black_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to cache server", 0);
        return;
    }
    
    /* Cache Node Initialization */
    memset(&cache, 0, sizeof(cache));
    lamb_nodes_connect(&cache, LAMB_MAX_CACHE, config.nodes, 7);
    if (cache.len < 1) {
        lamb_errlog(config.logfile, "Can't connect to cache node server", 0);
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

    /* Postgresql Database  */
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

    /* fetch  account */
    memset(&account, 0, sizeof(account));
    err = lamb_account_fetch(&db, aid, &account);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch account '%d' information", aid);
        return;
    }

    /* Open Client Queue */
    lamb_init_queues(&account);

    /* Fetch company information */
    memset(&company, 0, sizeof(company));
    err = lamb_company_get(&db, account.company, &company);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch id %d company information", account.company);
        return;
    }

    /* Fetch template information */
    memset(&template, 0, sizeof(template));
    err = lamb_template_get_all(&db, account.id, &template, LAMB_MAX_TEMPLATE);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch template information", 0);
        return;
    }

    /* Fetch group information */
    memset(&group, 0, sizeof(group));
    err = lamb_group_get(&db, account.route, &group, LAMB_MAX_CHANNEL);
    if (err) {
        lamb_errlog(config.logfile, "No channel routing available", 0);
        return;
    }

    /* Fetch keyword information */
    memset(&keywords, 0, sizeof(keywords));
    if (account.check_keyword) {
        err = lamb_keyword_get_all(&db, &keywords, LAMB_MAX_KEYWORD);
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
    long cpus = lamb_get_cpu();
    lamb_start_thread(lamb_worker_loop, NULL, config.work_threads > cpus ? cpus : config.work_threads);

    /* Start Billing and Update thread */
    lamb_start_thread(lamb_save_message, NULL, 1);

    /* Start online status thread */
    lamb_start_thread(lamb_online_update, NULL, 1);

    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }

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

void *lamb_worker_loop(void *data) {
    int err;
    lamb_list_node_t *ret;
    lamb_list_node_t *node;
    lamb_message_t *message;
    lamb_submit_t *submit;

    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }
    
    while (true) {
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
            if (account.policy != LAMB_POL_EMPTY) {
                if (lamb_security_check(&black, account.policy, submit->phone)) {
                    if (account.policy == LAMB_BLACKLIST) {
                        printf("-> [policy] The security check not pass\n");
                        continue;
                    }
                } else {
                    if (account.policy == LAMB_WHITELIST) {
                        printf("-> [policy] The security check not pass\n");
                        continue;
                    }
                }
                printf("-> [policy] The Security check OK\n");
            }

            /* SpCode Processing  */
            printf("-> [spcode] check message spcode extended\n");
            if (account.extended) {
                lamb_account_spcode_process(account.spcode, submit->spcode, 20);
            } else {
                strcpy(submit->spcode, account.spcode);
            }

            /* Template Processing */
            if (account.check_template) {
                if (!lamb_template_check(&template, submit->content, submit->length)) {
                    printf("-> [template] The template check will not pass\n");
                    continue;
                }
                printf("-> [template] The template check OK\n");
            }

            /* Keywords Filtration */
            if (account.check_keyword) {
                if (lamb_keyword_check(&keywords, submit->content)) {
                    printf("-> [keyword] The keyword check not pass\n");
                    continue;
                }
                printf("-> [keyword] The keyword check OK\n");
            }

        routing:
            /* Routing Scheduling */
            if (channel.len > 0) {
                for (int i = 0; i < channel.len; i++) {
                    err = lamb_queue_send(channel.queues[i], (char *)message, sizeof(lamb_message_t), 0);
                    if (err) {
                        if ((i + 1) > channel.len) {
                            i = 0;
                            lamb_sleep(10);
                        }
                        continue;
                    }
                    break;
                }
            } else {
                printf("-> [channel] No gateway channels available\n");
                lamb_sleep(3000);
                goto routing;
            }

            /* Billing Processing */
            if (account.charge_type == LAMB_CHARGE_SUBMIT) {
                pthread_mutex_lock(&(rdb.lock));
                err = lamb_company_billing(&rdb, company.id, -1, &(company.money));
                pthread_mutex_unlock(&(rdb.lock));
                if (err) {
                    lamb_errlog(config.logfile, "Lamb Account %d for company %d chargeback failure", account.id, company.id);
                    lamb_sleep(3000);
                }
            }
        
            /* Send Message to Storage */
            pthread_mutex_lock(&storage->lock);
            ret = lamb_list_rpush(storage, node);
            pthread_mutex_unlock(&storage->lock);
            if (ret == NULL) {
                lamb_errlog(config.logfile, "Memory cannot be allocated for the storage queue", 0);
                lamb_sleep(3000);
            }
        } else {
            lamb_sleep(10);
        }
    }

    pthread_exit(NULL);
}

void *lamb_save_message(void *data) {
    int i, err;
    lamb_list_node_t *node;
    lamb_submit_t *submit;
    lamb_message_t *message;

    while (true) {
        pthread_mutex_lock(&storage->lock);
        node = lamb_list_lpop(storage);
        pthread_mutex_unlock(&storage->lock);

        if (node == NULL) {
            lamb_sleep(10);
            continue;
        }

        /* Write Message to Cache */
        message = (lamb_message_t *)node->val;
        submit = (lamb_submit_t *)&message->data;
        i = (submit->id % cache.len);
        pthread_mutex_lock(&(cache.nodes[i]->lock));
        err = lamb_cache_message(cache.nodes[i], &account, &company, submit);
        pthread_mutex_unlock(&(cache.nodes[i]->lock));
        if (err) {
            lamb_errlog(config.logfile, "Write submit message to cache failure", 0);
            lamb_sleep(1000);
        }

        /* Write Message to Database */
        err = lamb_write_message(&mdb, &account, &company, submit);
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

void *lamb_online_update(void *data) {
    int err;
    redisReply *reply = NULL;

    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }

    while (true) {
        pthread_mutex_lock(&(rdb.lock));
        reply = redisCommand(rdb.handle, "HMSET server.%d pid %u queue %u storage %u", account.id, getpid(), queue->len, storage->len);
        pthread_mutex_unlock(&(rdb.lock));
        if (reply != NULL) {
            freeReplyObject(reply);
        }
        sleep(3);
    }

    pthread_exit(NULL);
}

void lamb_init_queues(lamb_account_t *account) {
    int err;
    char name[128];
    lamb_queue_opt sopt, ropt;
    struct mq_attr sattr, rattr;
    
    sopt.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    sattr.mq_maxmsg = 65535;
    sattr.mq_msgsize = 512;
    sopt.attr = &sattr;

    sprintf(name, "/cli.%d.message", account->id);
    err = lamb_queue_open(&client, name, &sopt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s client queue", name);
    }

    lamb_queue_t deliver;

    ropt.flag = O_CREAT | O_WRONLY | O_NONBLOCK;
    rattr.mq_maxmsg = 65535;
    rattr.mq_msgsize = 512;
    ropt.attr = &rattr;

    sprintf(name, "/cli.%d.deliver", account->id);
    err = lamb_queue_open(&deliver, name, &ropt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s client queue", name);
    } else {
        lamb_queue_close(&deliver);
    }

    return;
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

    if (lamb_get_string(&cfg, "BlackHost", conf->black_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'BlackHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "BlackPort", &conf->black_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'BlackPort' parameter\n");
        goto error;
    }

    if (conf->black_port < 1 || conf->black_port > 65535) {
        fprintf(stderr, "ERROR: Invalid blacklist cache port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "BlackPassword", conf->black_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'BlackPassword' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "BlackDb", &conf->black_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'BlackDb' parameter\n");
        goto error;
    }
    
    if (conf->black_db < 0 || conf->black_db > 65535) {
        fprintf(stderr, "ERROR: Invalid blacklist cache database name\n");
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

    if (lamb_get_string(&cfg, "MsgHost", conf->msg_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MsgPort", &conf->msg_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgPort' parameter\n");
        goto error;
    }

    if (conf->msg_port < 1 || conf->msg_port > 65535) {
        fprintf(stderr, "ERROR: Invalid MsgPort number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgUser", conf->msg_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgPassword", conf->msg_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "MsgName", conf->msg_name, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgName' parameter\n");
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


