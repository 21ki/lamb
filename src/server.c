
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
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <cmpp.h>
#include "server.h"
#include "config.h"
#include "socket.h"
#include "keyword.h"
#include "security.h"


static int aid = 0;
static long long money;
static lamb_db_t db;
static lamb_db_t mdb;
static int mt, mo;
static int scheduler, deliverd;
static lamb_cache_t rdb;
static lamb_caches_t cache;
static lamb_queue_t *storage;
static lamb_queue_t *billing;
static lamb_queue_t *channels;
static lamb_account_t account;
static lamb_company_t company;
static lamb_queue_t *templates;
static lamb_queue_t *keywords;
static lamb_config_t config;
static lamb_status_t status;
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
    int err;

    lamb_set_process("lamb-server");

    money = 0;
    memset(&status, 0, sizeof(status));
    
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

    /* Billing Queue Initialization */
    billing = lamb_queue_new(aid);
    if (!billing) {
        lamb_errlog(config.logfile, "billing queue initialization failed", 0);
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


    lamb_nn_option opt;

    memset(&opt, 0, sizeof(opt));
    opt.id = aid;
    opt.type = LAMB_NN_PULL;
    memcpy(opt.addr, "127.0.0.1", 10);
    
    /* Connect to MT server */
    err = lamb_nn_connect(&mt, &opt, config.mt_host, config.mt_port, NN_REQ, config.timeout);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to MT %s server", config.mt_host);
        return;
    }

    /* Connect to MO server */
    opt.type = LAMB_NN_PUSH;
    err = lamb_nn_connect(&mo, &opt, config.mo_host, config.mo_port, NN_PAIR, config.timeout);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to MO %s server", config.mo_host);
        return;
    }

    /* Connect to Scheduler server */
    opt.type = LAMB_NN_PUSH;
    err = lamb_nn_connect(&scheduler, &opt, config.scheduler_host, config.scheduler_port, NN_REQ, config.timeout);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to Scheduler %s server", config.scheduler_host);
        return;
    }

    /* Connect to Deliver server */
    opt.type = LAMB_NN_PULL;
    err = lamb_nn_connect(&deliverd, &opt, config.deliver_host, config.deliver_port, NN_REQ, config.timeout);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to deliver %s server", config.deliver_host);
        return;
    }
    
    /* Fetch company information */
    memset(&company, 0, sizeof(company));
    err = lamb_company_get(&db, account.company, &company);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch id %d company information", account.company);
        return;
    }

    /* Template information Initialization */
    templates = lamb_queue_new(aid);
    if (!templates) {
        lamb_errlog(config.logfile, "template queue initialization failed ", 0);
        return;
    }

    err = lamb_template_get_all(&db, account.id, templates);
    if (err) {
        lamb_errlog(config.logfile, "Can't fetch template information", 0);
        return;
    }

    /* Fetch group information */
    channels = lamb_queue_new(aid);
    if (!channels) {
        lamb_errlog(config.logfile, "channels queue initialization failed", 0);
        return;
    }

    err = lamb_group_get(&db, account.route, channels);
    if (channels->len < 1) {
        lamb_errlog(config.logfile, "No channel routing available", 0);
        return;
    }
    
    /* Keyword information Initialization */
    keywords = lamb_queue_new(aid);
    if (!keywords) {
        lamb_errlog(config.logfile, "keyword queue initialization failed ", 0);
        return;
    }

    if (account.check_keyword) {
        err = lamb_keyword_get_all(&db, keywords);
        if (err) {
            lamb_errlog(config.logfile, "Can't fetch keyword information", 0);
        }
    }

    /* Thread lock initialization */
    pthread_mutex_init(&lock, NULL);

    /* Message Table initialization */
    lamb_new_table(&mdb);
    
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
    int err;
    char *buf;
    int rc, len;
    bool success;
    lamb_bill_t *bill;
    lamb_store_t *store;
    lamb_submit_t *submit;
    lamb_node_t *node;
    lamb_template_t *template;
    lamb_keyword_t *keyword;
    lamb_message_t req;
    lamb_channel_t *channel;
    
    len = sizeof(lamb_submit_t);
    
    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }
    
    req.type = LAMB_REQ;

    while (true) {

        /* Request */
        rc = nn_send(mt, &req, sizeof(req), 0);
        
        if (rc < 0) {
            continue;
        }

        /* Response */
        rc = nn_recv(mt, &buf, NN_MSG, 0);

        if (rc < len) {
            if (rc > 0) {
                nn_freemsg(buf);
                lamb_sleep(100);
            }
            continue;
        }

        submit = (lamb_submit_t *)buf;
            
        ++status.toal;
        //printf("-> id: %llu, phone: %s, spcode: %s, content: %s, length: %d\n", submit->id, submit->phone, submit->spcode, submit->content, submit->length);

        /* Message Encoded Convert */
        char *fromcode;
        char content[512];

        if (submit->msgFmt == 0) {
            fromcode = "ASCII";
        } else if (submit->msgFmt == 8) {
            fromcode = "UCS-2BE";
        } else if (submit->msgFmt == 11) {
            fromcode = NULL;
        } else if (submit->msgFmt == 15) {
            fromcode = "GBK";
        } else {
            status.fmt++;
            printf("-> [encoded] message encoded %d not support\n", submit->msgFmt);
            nn_freemsg(buf);
            continue;
        }

        if (fromcode != NULL) {
            memset(content, 0, sizeof(content));
            err = lamb_encoded_convert(submit->content, submit->length, content, sizeof(content), fromcode, "UTF-8", &submit->length);
            if (err || (submit->length == 0)) {
                status.fmt++;
                nn_freemsg(buf);
                printf("-> [encoded] Message encoding conversion failed\n");
                continue;
            }

            submit->msgFmt = 11;
            memset(submit->content, 0, 160);
            memcpy(submit->content, content, submit->length);
            printf("-> [encoded] Message encoding conversion successfull\n");
        }

        //printf("-> id: %llu, phone: %s, spcode: %s, msgFmt: %d, content: %s, length: %d\n", submit->id, submit->phone, submit->spcode, submit->msgFmt, submit->content, submit->length);

        /* Blacklist and Whitelist */
        /* 
           if (account.policy != LAMB_POL_EMPTY) {
           if (lamb_security_check(&black, account.policy, submit->phone)) {
           if (account.policy == LAMB_BLACKLIST) {
           status.blk++;
           nn_freemsg(buf);
           //printf("-> [policy] The security check not pass\n");
           continue;
           }
           } else {
           if (account.policy == LAMB_WHITELIST) {
           status.blk++;
           nn_freemsg(buf);
           //printf("-> [policy] The security check not pass\n");
           continue;
           }
           }
           //printf("-> [policy] The Security check OK\n");
           }
        */
            
        /* SpCode Processing  */
        if (account.extended) {
            printf("-> [spcode] check message spcode extended\n");
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
                if (lamb_template_check(template, submit->content, submit->length)) {
                    success = true;
                    break;
                }
                node = node->next;
            } 

            if (!success) {
                status.tmp++;
                printf("-> [template] The template check will not pass\n");
                nn_freemsg(buf);
                continue;
                    
            }
            printf("-> [template] The template check OK\n");
        }

        /* Keywords Filtration */
        if (account.check_keyword) {
            success = false;
            node = keywords->head;

            while (node != NULL) {
                keyword = (lamb_keyword_t *)node->val;
                if (lamb_keyword_check(keyword, submit->content)) {
                    success = true;
                    break;
                }
                node = node->next;
            }
               
            if (success) {
                status.key++;
                nn_freemsg(buf);
                printf("-> [keyword] The keyword check not pass\n");
                continue;
            }
            printf("-> [keyword] The keyword check OK\n");
        }

        /* Scheduling */
    routing:
        success = false;
        node = channels->head;

        while (node != NULL) {
            channel = (lamb_channel_t *)node->val;
            submit->channel = channel->id;
            if (nn_send(scheduler, (char *)submit, sizeof(lamb_submit_t), 0) > 0) {
                rc = nn_recv(scheduler, &buf, NN_MSG, 0);
                if (rc > 1 && (strncmp(buf, "ok", 2) == 0)) {
                    success = true;
                    status.sub++;
                    break;
                }
            }

            node = node->next;
        }

        if (!success) {
            if (channels->len < 1) {
                printf("-> [channel] No gateway channel available\n");
            }
            //printf("-> [channel] busy all channels\n");
            lamb_sleep(10);
            goto routing;
        }

        //printf("-> [channel] Message sent to %d channel\n", channel->id);
            
        /* Save message to billing queue */
        if (account.charge == LAMB_CHARGE_SUBMIT) {
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
            lamb_queue_push(storage, store);
        }
    }

    pthread_exit(NULL);
}

void *lamb_deliver_loop(void *data) {
    int rc;
    char *buf;
    lamb_bill_t *bill;
    lamb_store_t *store;
    lamb_message_t req;
    lamb_report_t *report;
    lamb_deliver_t *deliver;
    lamb_message_t *message;

    int rlen = sizeof(lamb_report_t);
    int dlen = sizeof(lamb_deliver_t);

    req.type = LAMB_REQ;

    while (true) {
        rc = nn_send(deliverd, &req, sizeof(req), NN_DONTWAIT);

        if (rc < 0) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(deliverd, &buf, NN_MSG, 0);

        if (rc != rlen && rc != dlen) {
            if (rc > 0) {
                nn_freemsg(buf);
                lamb_sleep(10);
            }
            continue;
        }

        message = (lamb_message_t *)buf;

        if (message->type == LAMB_REPORT) {
            status.rep++;
            report = (lamb_report_t *)buf;

            report->account = account.id;
            memcpy(report->spcode, account.spcode, 20);
            
            //printf("-> msgId: %llu, acc: %llu, comp: %d, charge: %d\n", report->id, acc, comp, charge);
            int coin = 0;

            if ((report->status == 1) && (account.charge == LAMB_CHARGE_SUCCESS)) {
                coin = 1;
            } else if ((report->status != 1) && (account.charge == LAMB_CHARGE_SUBMIT)) {
                coin = -1;
            }

            if (coin != 0) {
                bill = (lamb_bill_t *)malloc(sizeof(lamb_bill_t));
                if (bill) {
                    bill->id = company.id;
                    bill->money = (report->status == 1) ? -1 : 1;
                    lamb_queue_push(billing, bill);
                }
            }

            nn_send(mo, report, rlen, 0);

            /* Store report to database */
            store = (lamb_store_t *)malloc(sizeof(lamb_store_t));

            if (store) {
                store->type = LAMB_REPORT;
                store->val = report;
                lamb_queue_push(storage, store);
            } else {
                nn_freemsg(buf);
            }

            continue;
        }

        if (message->type == LAMB_DELIVER) {
            deliver = (lamb_deliver_t *)buf;

            status.delv++;
            //nn_send(mo, deliver, sizeof(lamb_deliver_t), 0);

            store = (lamb_store_t *)malloc(sizeof(lamb_store_t));

            if (store) {
                store->type = LAMB_DELIVER;
                store->val = deliver;
                lamb_queue_push(storage, store);
            } else {
                nn_freemsg(buf);
            }

            continue;
        }

        nn_freemsg(buf);
    }

    pthread_exit(NULL);
}

void *lamb_store_loop(void *data) {
    lamb_node_t *node;
    lamb_store_t *store;
    lamb_report_t *report;
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
        } else if (store->type == LAMB_REPORT) {
            report = (lamb_report_t *)store->val;
            lamb_write_report(&mdb, report);
        } else if (store->type == LAMB_DELIVER) {
            deliver = (lamb_deliver_t *)store->val;
            lamb_write_deliver(&mdb, &account, &company, deliver);
        }

        nn_freemsg(store->val);
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
        pthread_mutex_lock(&(rdb.lock));
        err = lamb_company_billing(&rdb, bill->id, bill->money, &money);
        pthread_mutex_unlock(&(rdb.lock));
        if (err) {
            lamb_errlog(config.logfile, "Account %d billing money %d failure", bill->id, bill->money);
        }

        free(bill);
        free(node);
    }

    pthread_exit(NULL);
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
        reply = redisCommand(rdb.handle, "HMSET server.%d pid %u", account.id, getpid());
        pthread_mutex_unlock(&(rdb.lock));
        if (reply != NULL) {
            freeReplyObject(reply);
        }

        printf("-[ %s ]-> store: %lld, bill: %lld, toal: %llu, sub: %llu, rep: %llu, delv: %llu, fmt: %llu, blk: %llu, tmp: %llu, key: %llu\n",
               account.username, storage->len, billing->len, status.toal, status.sub, status.rep, status.delv, status.fmt, status.blk, status.tmp, status.key);

        sleep(3);
    }

    
    pthread_exit(NULL);
}

int lamb_write_report(lamb_db_t *db, lamb_report_t *message) {
    char sql[512];
    char table[128];
    PGresult *res = NULL;

    lamb_get_today("message_", table);

    if (message != NULL) {
        sprintf(sql, "UPDATE %s SET status = %d WHERE id = %lld", table, message->status, (long long)message->id);

        res = PQexec(db->conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            return -1;
        }

        PQclear(res);
    }

    return 0;
}

int lamb_write_message(lamb_db_t *db, lamb_account_t *account, lamb_company_t *company, lamb_submit_t *message) {
    char *column;
    char sql[512];
    char table[128];
    PGresult *res = NULL;

    lamb_get_today("message_", table);
    
    if (message != NULL) {
        column = "id, spid, spcode, phone, content, status, account, company";
        sprintf(sql, "INSERT INTO %s(%s) VALUES(%lld, '%s', '%s', '%s', '%s', %d, %d, %d)", table, column,
                (long long int)message->id, message->spid, message->spcode, message->phone, message->content,
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

void lamb_get_today(const char *pfx, char *val) {
    time_t rawtime;
    struct tm *t;

    time(&rawtime);
    t = localtime(&rawtime);
    sprintf(val, "%s%4d%02d%02d", pfx, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    return;
}

void lamb_new_table(lamb_db_t *db) {
    char sql[512];
    char table[128];
    PGresult *res = NULL;
    const char *prefix = "message_";
    
    lamb_get_today(prefix, table);

    sprintf(sql, "CREATE TABLE IF NOT EXISTS %s(id bigint NOT NULL,spid varchar(6) NOT NULL,"
            "spcode varchar(21) NOT NULL,phone varchar(21) NOT NULL,content text NOT NULL,status int NOT NULL,"
            "account int NOT NULL, company int NOT NULL, create_time timestamp without time zone NOT NULL "
            "default now()::timestamp(0) without time zone);", table);
    
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        printf("-> lamb_new_table() sql exec error\n");
        return;
    }

    PQclear(res);
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

    if (lamb_get_string(&cfg, "SchedulerHost", conf->scheduler_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid scheduler server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "SchedulerPort", &conf->scheduler_port) != 0) {
        fprintf(stderr, "ERROR: Can't read scheduler server port parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DeliverHost", conf->deliver_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid deliver server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DeliverPort", &conf->deliver_port) != 0) {
        fprintf(stderr, "ERROR: Can't read deliver server port parameter\n");
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


