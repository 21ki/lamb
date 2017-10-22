
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
#include "socket.h"
#include "keyword.h"
#include "security.h"
#include "message.h"


static int aid = 0;
static lamb_db_t db;
static lamb_db_t mdb;
static int mt, mo, md;
static lamb_cache_t rdb;
static lamb_caches_t cache;
static lamb_queue_t *storage;
static lamb_queue_t *billing;
static lamb_group_t group;
static lamb_account_t account;
static lamb_company_t company;
static lamb_queue_t *channels;
static lamb_queue_t *templates;
static lamb_queue_t *keywords;
static lamb_config_t config;
static pthread_mutex_t lock;

int main(int argc, char *argv[]) {
    bool background = false;
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
            background = true;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    if (aid < 1) {
        fprintf(stderr, "Invalid account number\n");
        return -1;
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

    /* Storage Queue Initialization */
    storage = lamb_queue_new(aid);
    if (!storage) {
        lamb_errlog(config.logfile, "storage queue initialization failed ", 0);
        return;
    }

    /* Channel Queue Initialization */
    channels = lamb_queue_new(aid);
    if (!channels) {
        lamb_errlog(config.logfile, "channels queue initialization failed ", 0);
        return;
    }

    /* Template Queue Initialization */
    templates = lamb_queue_new(aid);
    if (!templates) {
        lamb_errlog(config.logfile, "template queue initialization failed ", 0);
        return;
    }

    /* Keyword Queue Initialization */
    keywords = lamb_queue_new(aid);
    if (!keywords) {
        lamb_errlog(config.logfile, "keyword queue initialization failed ", 0);
        return;
    }

    /* Redis Initialization */
    err = lamb_cache_connect(&rdb, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis server", 0);
        return;
    }

    /* Cache Cluster Initialization */
    memset(&cache, 0, sizeof(cache));
    lamb_nodes_connect(&cache, LAMB_MAX_CACHE, config.nodes, 7, 1);
    if (cache.len != 7) {
        lamb_errlog(config.logfile, "connect to cache cluster server failed", 0);
        return;
    }

    /* Postgresql Database  */
    err = lamb_db_init(&db);
    if (err) {
        lamb_errlog(config.logfile, "postgresql database initialization failed", 0);
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
        lamb_errlog(config.logfile, "postgresql database initialization failed", 0);
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

    /* Connect to MT server */
    lamb_nn_option opt;

    memset(&opt, 0, sizeof(opt));
    opt.id = aid;
    strcpy(opt.addr, "127.0.0.1");
    opt.timeout = config.timeout;
    
    err = lamb_nn_connect(&mt, &opt, config.mt_host, config.mt_port);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to MT %s server", config.mt_host);
        return;
    }
    
    /* Connect to MO server */
    err = lamb_nn_connect(&mo, &option, config.mo_host, config.mo_port);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to MO %s server", config.mo_host);
        return;
    }

    /* Connect to MD server */    
    err = lamb_nn_connect(&md, &option, config.md_host, config.md_port);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to MD %s server", config.mo_host);
        return;
    }

    /* Fetch company information */
    memset(&company, 0, sizeof(company));
    err = lamb_company_get(&db, account.company, &company);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch id %d company information", account.company);
        return;
    }

    /* Fetch template information */
    err = lamb_template_get_all(&db, account.id, templates);
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
    if (account.check_keyword) {
        err = lamb_keyword_get_all(&db, keywords);
        if (err) {
            lamb_errlog(config.logfile, "Can't fetch keyword information", 0);
        }
    }

    /* Channel Initialization */
    lamb_mq_opt opts;

    channels = lamb_queue_new(aid);
    if (!channels) {
        lamb_errlog(config.logfile, "channel queue initialization failed", 0);
        return;
    }

    opts.flag = O_WRONLY | O_NONBLOCK;
    opts.attr = NULL;

    err = lamb_open_channel(&group, channels, &opts);
    if (err || channels->len < 1) {
        lamb_errlog(config.logfile, "No gateway channel available", 0);
        return;
    }

    /* Thread lock initialization */
    pthread_mutex_init(&lock, NULL);
    
    /* Start sender thread */
    long cpus = lamb_get_cpu();
    lamb_start_thread(lamb_work_loop, NULL, config.work_threads > cpus ? cpus : config.work_threads);

    /* Start deliver thread */
    lamb_start_thread(lamb_deliver_loop, NULL, 1);
    
    /* Start billing thread */
    lamb_start_thread(lamb_billing_loop, NULL, 1);

    /* Start storage thread */
    lamb_start_thread(lamb_store_loop, NULL, 1);

    /* Start status thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);

    /* Master control loop*/
    while (true) {
        lamb_sleep(3000);
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

void *lamb_work_loop(void *data) {
    int i, err;
    char *buf;
    int rc, len;
    bool success;
    lamb_bill_t *bill;
    lamb_store_t *store;
    lamb_submit_t *submit;
    lamb_node_t *node;
    lamb_mq_t *channel;
    lamb_template_t *template;
    lamb_keyword_t *keyword;

    len = sizeof(lamb_submit_t);
    
    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }
    
    while (true) {
        submit = NULL;

        pthread_mutex_lock(&lock);
        rc = nn_send(mt, "req", 4, 0);
        if (rc > 0) {
            rc = nn_recv(mt, &buf, NN_MSG, 0);
            if (rc == len) {
                submit = (lamb_submit_t *)buf;
            }
        }
        pthread_mutex_unlock(&lock);

        if (submit != NULL) {
            //printf("-> id: %llu, phone: %s, spcode: %s, content: %s, length: %d\n", submit->id, submit->phone, submit->spcode, submit->content, submit->length);

            /* Message Encoded Convert */
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
                //printf("-> [encoded] message encoded %d not support\n", submit->msgFmt);
                nn_freemsg(buf);
                continue;
            }

            if (fromcode != NULL) {
                memset(content, 0, sizeof(content));
                err = lamb_encoded_convert(submit->content, submit->length, content, sizeof(content), fromcode, "UTF-8", &submit->length);
                if (err || (submit->length == 0)) {
                    //printf("-> [encoded] Message encoding conversion failed\n");
                    nn_freemsg(buf);
                    continue;
                }

                submit->msgFmt = 11;
                memset(submit->content, 0, 160);
                memcpy(submit->content, content, submit->length);
                //printf("-> [encoded] Message encoding conversion successfull\n");
            }

            //printf("-> id: %llu, phone: %s, spcode: %s, msgFmt: %d, content: %s, length: %d\n", submit->id, submit->phone, submit->spcode, submit->msgFmt, submit->content, submit->length);

            /* Blacklist and Whitelist */
            /* 
               if (account.policy != LAMB_POL_EMPTY) {
               if (lamb_security_check(&black, account.policy, submit->phone)) {
               if (account.policy == LAMB_BLACKLIST) {
               nn_freemsg(buf);
               //printf("-> [policy] The security check not pass\n");
               continue;
               }
               } else {
               if (account.policy == LAMB_WHITELIST) {
               nn_freemsg(buf);
               //printf("-> [policy] The security check not pass\n");
               continue;
               }
               }
               //printf("-> [policy] The Security check OK\n");
               }
            */
            
            /* SpCode Processing  */
            //printf("-> [spcode] check message spcode extended\n");
            if (account.extended) {
                lamb_spcode_process(account.spcode, submit->spcode, 20);
            } else {
                strcpy(submit->spcode, account.spcode);
            }

            /* Template Processing */
            if (account.check_template) {
                success = false;
                node = templates->head;

                while (node != NULL) {
                    template = (lamb_template_t *)node->val;
                    if (lamb_template_check(&template, submit->content, submit->length)) {
                        success = true;
                        break;
                    }
                    node = node->next;
                } 

                if (!success) {
                    //printf("-> [template] The template check will not pass\n");
                    nn_freemsg(buf);
                    continue;
                    
                }
                //printf("-> [template] The template check OK\n");
            }

            /* Keywords Filtration */
            if (account.check_keyword) {
                success = false;
                node = keywords->head;

                while (node != NULL) {
                    keyword = (lamb_keyword_t *)node->val;
                    if (lamb_keyword_check(&keyword, submit->content)) {
                        success = true;
                        break;
                    }
                    node = node->next;
                }
               
                if (success) {
                    //printf("-> [keyword] The keyword check not pass\n");
                    nn_freemsg(buf);
                    continue;
                }
                //printf("-> [keyword] The keyword check OK\n");
           }

        routing: /* Routing Scheduling */
            success = false;
            channel = channels->head;

            while (node != NULL) {
                channel = (lamb_mq_t *)node->val;
                err = lamb_mq_send(channel, (char *)submit, sizeof(lamb_submit_t), 0);
                if (err) {
                    node = node->next;
                } else {
                    success = true;
                    break;
                }
            }

            if (!success) {
                if (channels->len < 1) {
                    printf("-> [channel] No gateway channel available\n");
                }
                lamb_sleep(100);
                goto routing;
            }

            /* Cache message information */
            i = (submit->id % cache.len);
            pthread_mutex_lock(&cache.nodes[i]->lock);
            err = lamb_cache_message(cache.nodes[i], &account, &company, submit);
            pthread_mutex_unlock(&cache.nodes[i]->lock);

            if (err) {
                lamb_errlog(config.logfile, "lamb cache message failed", 0);
                lamb_sleep(1000);
            }
            
            /* Save message to billing queue */
            if (account.charge_type == LAMB_CHARGE_SUBMIT) {
                bill = (lamb_bill_t *)malloc(sizeof(lamb_bill_t));
                if (bill) {
                    bill->id = company.id;
                    bill->money = -1;
                    lamb_queue_push(billing, bill);
                }
            }

            /* Save message to storage queue */
            store = (lamb_store_t *)malloc(sizeof(lamb_store_t));
            if (store) {
                store->type = LAMB_SUBMIT;
                store->val = submit;
                lamb_queue_push(storage, submit);
            }
        } else {
            lamb_sleep(1000);
        }
    }

    pthread_exit(NULL);
}

void *lamb_deliver_loop(void *data) {
    char *buf;
    int i, rc, len;
    lamb_bill_t *bill;
    lamb_store_t *store;
    lamb_report_t *report;
    lamb_deliver_t *deliver;
    lamb_message_t *message;

    len = sizeof(lamb_message_t);
    
    while (true) {
        rc = nn_send(md, "req", 4, 0);

        if (rc < 0) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(md, &buf, NN_MSG, 0);
        if (rc != len) {
            nn_freemsg(buf);
            continue;
        }

        message = (lamb_message_t *)buf;

        if (message->type == LAMB_REPORT) {
            lamb_report_t *tmp;
            report = (lamb_report_t *)&message->data;

            tmp = (lamb_report_t *)malloc(sizeof(lamb_report_t));
            if (tmp) {
                memcpy(tmp, report, sizeof(lamb_report_t));
                store = (lamb_store_t *)malloc(sizeof(lamb_store_t));

                if (store) {
                    store->type = LAMB_REPORT;
                    store->val = tmp;
                    lamb_queue_push(storage, store);
                } else {
                    free(tmp);
                }
            }

            char spid[6];
            char spcode[24];
            int charge, acc, comp;

            i = (report->id % cache.len);
            pthread_mutex_lock(&cache.nodes[i]->lock);
            lamb_cache_query(cache.nodes[i], spid, spcode, &acc, &comp, &charge);
            pthread_mutex_unlock(&cache.nodes[i]->lock);

            /* Billing processing */
            if (charge == LAMB_CHARGE_SUCCESS) {
                bill = (lamb_bill_t *)malloc(sizeof(lamb_bill_t));
                if (bill) {
                    bill->id = comp;
                    bill->money = (report->status == 1) ? -1 : 1;
                    lamb_queue_push(billing, bill);
                }
            }

            continue;
        }

        if (message->type == LAMB_DELIVER) {
            lamb_deliver_t *tmp;
            deliver = (lamb_deliver_t *)&message->data;

            nn_send(mo, deliver, sizeof(lamb_deliver_t), 0);

            tmp = (lamb_deliver_t *)malloc(sizeof(lamb_deliver_t));
            
            if (tmp) {
                memcpy(tmp, deliver, sizeof(lamb_deliver_t));
                store = (lamb_store_t *)malloc(sizeof(lamb_store_t));

                if (store) {
                    store->type = LAMB_DELIVER;
                    store->val = deliver;

                    lamb_queue_push(storage, store);
                }
            }
        }
    }

    pthread_exit(NULL);
}

void *lamb_store_loop(void *data) {
    int err;
    lamb_node_t *node;
    lamb_store_t *store;
    lamb_submit_t *submit;
    lamb_deliver_t *deliver;
    
    while (true) {
        node = lamb_queue_pop(storage);

        if (!node) {
            lamb_sleep(10);
            continue;
        }

        store = (lamb_store_t *)node->val;

        if (store->type == LAMB_SUBMIT) {
            submit = (lamb_submit_t *)store->val;
            lamb_write_message(&mdb, &account, &company, submit);
        }

        if (store->type == LAMB_DELIVER) {
            deliver = (lamb_deliver_t *)store->val;
            lamb_write_deliver(&mdb, &account, &company, deliver);
        }

        free(store->val);
        free(store);
        free(node);
    }

    pthread_exit(NULL);
}

void *lamb_billing_loop(void *data) {
    int err;
    lamb_bill_t *bill;
    lamb_node_t *node;
    
    while (true) {
        node = lamb_queue_pop(billing);

        if (!node) {
            lamb_sleep(10);
            continue;
        }

        bill = (lamb_bill_t *)node->val;
        err = lamb_company_billing(bill->id, bill->money);
        if (err) {
            lamb_errlog(config.logfile, "Account %d billing money %d failure", bill->id, bill->money);
        }

        free(bill);
        free(node);
    }

    pthread_exit(NULL);
}

int lamb_open_channel(lamb_group_t *group, lamb_queue_t *channels, lamb_mq_opt *opt) {
    int err;
    char name[128];
    lamb_mq_t *queue;

    for (int i = 0; (i < group->len) && (group->channels[i] != NULL); i++) {
        queue = (lamb_mq_t *)calloc(1, sizeof(lamb_mq_t));
        if (queue != NULL) {
            sprintf(name, "/gw.%d.message", group->channels[i]->id);
            err = lamb_mq_open(queue, name, opt);
            if (err) {
                lamb_errlog(config.logfile, "Can't open %s gateway queue", name);
                continue;
            }

            lamb_queue_push(channels, queue);
        } else {
            return -1;
        }
    }

    return 0;
}

void *lamb_stat_loop(void *data) {
    int err;
    redisReply *reply = NULL;

    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }

    while (true) {
        pthread_mutex_lock(&(rdb.lock));
        reply = redisCommand(rdb.handle, "HMSET server.%d pid %u queue %u storage %u", account.id, getpid(),
                             queue->len, storage->len);
        pthread_mutex_unlock(&(rdb.lock));
        if (reply != NULL) {
            freeReplyObject(reply);
        }
        sleep(3);
    }

    pthread_exit(NULL);
}

int lamb_cache_message(lamb_cache_t *cache, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HMSET %llu spid %s spcode %s phone %s account %d company %d charge %d",
                         message->id, message->spid, message->spcode, message->phone, account->id, company->id,
                         account->charge_type);
    if (reply == NULL) {
        return -1;
    }

    freeReplyObject(reply);

    return 0;
}

int lamb_cache_query(lamb_cache_t *cache, char *spid, char *spcode, int *account, int *company, int *charge) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HMGET %llu spid spcode account company charge");

    if (reply != NULL) {
        if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements == 5)) {
            if (reply->element[0]->len > 0) {
                memcpy(spid, reply->element[0]->str, reply->element[0]->len);
            }

            if (reply->element[1]->len > 0) {
                memcpy(spcode, reply->element[1]->str, reply->element[1]->len);
            }

            if (reply->element[2]->len > 0) {
                *account = atoi(reply->element[2]->str);
            }

            if (reply->element[3]->len > 0) {
                *company = atoi(reply->element[3]->str);
            }

            if (reply->element[4]->len > 0) {
                *charge = atoi(reply->element[4]->str);
            }

        }

        freeReplyObject(reply);
    } else {
        return -1;
    }

    return 0;
}

int lamb_write_message(lamb_db_t *db, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message) {
    char *column;
    char sql[512];
    PGresult *res = NULL;

    if (message != NULL) {
        column = "id, msgid, spid, spcode, phone, content, status, account, company";
        sprintf(sql, "INSERT INTO message(%s) VALUES(%lld, %u, '%s', '%s', '%s', '%s', %d, %d, %d)", column,
                (long long int)message->id, 0, message->spid, message->spcode, message->phone, message->content,
                6, account->id, company->id);

        res = PQexec(db->conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            return -1;
        }

        PQclear(res);
    }

    return 0;
}

int lamb_write_deliver(lamb_db_t *db, lamb_account_t *account, lamb_company_t *company, lamb_deliver_t *message) {
    char *column;
    char sql[512];
    PGresult *res = NULL;

    if (message != NULL) {
        column = "id, spcode, phone, content";
        sprintf(sql, "INSERT INTO deliver(%s) VALUES(%lld, '%s', '%s', '%s')", column, (long long int)message->id,
                message->spcode, message->phone, message->content);
        res = PQexec(db->conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            return -1;
        }

        PQclear(res);
    }

    return 0;
}

int lamb_spcode_process(char *code, char *spcode, size_t size) {
    size_t len;

    len = strlen(code);
    
    if (len > strlen(spcode)) {
        memcpy(spcode, code, len >= size ? (size - 1) : len);
        return 0;
    }

    for (int i = 0; (i < len) && (i < (size - 1)); i++) {
        spcode[i] = code[i];
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

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
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

    if (lamb_get_string(&cfg, "MtHost", conf->mt_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid MT server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MtPort", &conf->mt_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MtPort' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MoHost", conf->mo_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid MO server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MoPort", &conf->mo_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MoPort' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MdHost", conf->md_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid MD server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MdPort", &conf->md_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MdPort' parameter\n");
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


